#include "Core/Window.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <thread>

#ifdef VK_USE_PLATFORM_WIN32_KHR
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h> // glfwGetWin32Window
#include <dwmapi.h>           // DwmFlush
#pragma comment(lib, "dwmapi.lib")
#endif

// ═══════════════════════════════════════════════
// 静态创建 — glfwInit → 窗口 → Surface 一步完成
// ═══════════════════════════════════════════════

std::optional<Window> Window::Create(const vk::raii::Instance& instance, const CreateInfo& info)
{
    Window result;

    // GLFW lifecycle is managed by Engine, not Window.
    // glfwInit() is already called before Window::Create.

    // Vulkan 不需要 OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, info.resizable ? GLFW_TRUE : GLFW_FALSE);

    // ── 2. 创建 GLFW 窗口 ──
    result.m_window = glfwCreateWindow(info.width, info.height, info.title.c_str(), nullptr, nullptr);
    if (!result.m_window)
    {
        spdlog::critical("Failed to create GLFW window.");
        return std::nullopt;
    }

    result.m_width = info.width;
    result.m_height = info.height;

    // ── 3. Set callbacks BEFORE any glfwPollEvents() ──
    // The framebuffer-resize callback must be registered before the first
    // glfwPollEvents() call; otherwise a WM_SIZE that arrives during the
    // show/paint/DWM sequence will be silently dropped, and the Window's
    // cached m_width/m_height will never see the real framebuffer dimensions
    // (or worse — stay at 0×0 if the compositor briefly minimizes the window).
    glfwSetWindowUserPointer(result.m_window, &result);
    glfwSetFramebufferSizeCallback(result.m_window, FramebufferResizeCallback);

    // ── 4. Force window realization BEFORE surface creation ──
    // On Windows 11 + some GPU drivers (e.g. Intel UHD Graphics), the driverʼs
    // implementation of vkCreateWin32SurfaceKHR internally peeks at the HWND and
    // caches its composited dimensions.  If the DWM has not finished processing
    // the window yet, the cached values are garbage and persist across all
    // subsequent getSurfaceCapabilitiesKHR() queries — pollEvents alone cannot
    // fix them once the surface is created.
    //
    // The sequence below ensures the window is fully realized (shown, painted,
    // and composited by DWM) before the VkSurface is created.
    glfwPollEvents(); // dispatch initial WM_SIZE / WM_SHOWWINDOW

#ifdef VK_USE_PLATFORM_WIN32_KHR
    {
        HWND hwnd = glfwGetWin32Window(result.m_window);
        ShowWindow(hwnd, SW_SHOW);       // ensure visible (idempotent)
        UpdateWindow(hwnd);              // synchronous WM_PAINT — forces
                                         // DWM to assign a composition surface
        DwmFlush();                      // block until DWM finishes compositing
    }
#endif

    glfwPollEvents(); // dispatch any DWM response messages (WM_DPICHANGED etc.)

    // ── 5. 创建 Vulkan Surface ──
    VkSurfaceKHR rawSurface;
    VkResult vkResult = glfwCreateWindowSurface(*instance, result.m_window, nullptr, &rawSurface);
    if (vkResult != VK_SUCCESS)
    {
        spdlog::critical("Failed to create window surface: {}", vk::to_string(vk::Result(vkResult)));
        return std::nullopt;
    }

    result.m_surface = vk::raii::SurfaceKHR(instance, rawSurface);

    // ── 6. Verify framebuffer dimensions are valid ──
    // On some Windows 11 + driver combinations, even after DWM flush
    // the framebuffer might not report valid dimensions immediately.
    // Verify and wait briefly if needed.
    {
        int fbWidth = 0, fbHeight = 0;
        glfwGetFramebufferSize(result.m_window, &fbWidth, &fbHeight);
        if (fbWidth > 0 && fbHeight > 0 && fbWidth <= 16384 && fbHeight <= 16384)
        {
            result.m_width = fbWidth;
            result.m_height = fbHeight;
            spdlog::debug("Window framebuffer verified: {}x{}", fbWidth, fbHeight);
        }
        else
        {
            spdlog::warn("Window framebuffer returned invalid dimensions ({}x{}) "
                         "immediately after surface creation — using requested dimensions {}x{}",
                         fbWidth, fbHeight, info.width, info.height);
        }
    }

    spdlog::info("Window created: {}x{} — \"{}\"", info.width, info.height, info.title);
    return result;
}

// ═══════════════════════════════════════════════

Window::~Window()
{
    // 必须在 window 销毁前释放 surface
    m_surface.clear();
    if (m_window)
    {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    // GLFW lifecycle is managed by Engine — do NOT call glfwTerminate() here.
    // Calling glfwTerminate() on a moved-from Window (e.g. when Window::Create
    // returns without NRVO, or after move-constructing into Engine::m_window)
    // kills GLFW globally while the real window/surface are still in use,
    // causing VK_ERROR_SURFACE_LOST_KHR or garbage surface capabilities.
    spdlog::debug("Window destroyed.");
}

// ═══════════════════════════════════════════════
// Surface 重建 — 驱动 Bug 恢复路径
// 当 vkGetPhysicalDeviceSurfaceCapabilitiesKHR 持续返回垃圾值时，
// 销毁旧 Surface 并创建新 Surface。窗口此时已运行了足够长时间，
// DWM 和驱动都已完成初始化，新 Surface 更可能获得正确的 capabilities。
// ═══════════════════════════════════════════════

void Window::RecreateSurface(const vk::raii::Instance& instance)
{
    // ── Destroy old surface FIRST ──
    // The previous approach (create new before destroying old, two surfaces
    // briefly coexist on the same HWND) fails with VK_ERROR_INITIALIZATION_FAILED
    // on Intel UHD Graphics drivers, which reject multiple surfaces per window.
    //
    // Similarly, destroying the old surface and immediately creating a new one
    // can fail if the driver hasn't finished tearing down the old surface yet.
    // To mitigate this, we poll events and insert a small delay between destroy
    // and create to let the driver's deferred cleanup complete.
    m_surface.clear();
    glfwPollEvents();
    std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~1 frame

    VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
    VkResult vkResult = glfwCreateWindowSurface(*instance, m_window, nullptr, &rawSurface);
    if (vkResult != VK_SUCCESS)
    {
        // The window still has no surface — this is a fatal recovery failure.
        // Subsequent surface queries will fail until the next engine restart.
        spdlog::error("Window::RecreateSurface: glfwCreateWindowSurface failed: {} "
                      "(no surface available — window will be unusable)",
                      vk::to_string(vk::Result(vkResult)));
        return;
    }

    m_surface = vk::raii::SurfaceKHR(instance, rawSurface);
    spdlog::debug("Window::RecreateSurface: surface recreated.");
}

// ═══════════════════════════════════════════════

bool Window::IsMinimized() const
{
    int w, h;
    glfwGetFramebufferSize(m_window, &w, &h);
    return w == 0 || h == 0;
}

bool Window::ShouldClose() const
{
    return glfwWindowShouldClose(m_window);
}

void Window::PollEvents()
{
    glfwPollEvents();
}

// ═══════════════════════════════════════════════
// 窗口 resize → 触发 SwapChain 重建
// ═══════════════════════════════════════════════

void Window::FramebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    self->m_width = width;
    self->m_height = height;

    spdlog::debug("Window resized: {}x{}", width, height);

    if (self->m_resizeCallback)
    {
        self->m_resizeCallback(width, height);
    }
}
