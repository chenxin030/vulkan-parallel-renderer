#pragma once

#include <memory>
#include <optional>
#include <string>
#include <functional>

#ifndef VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_EXCEPTIONS
#endif
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// ═══════════════════════════════════════════════
// GLFW 窗口 + Vulkan Surface
// ═══════════════════════════════════════════════

class Window
{
public:
    struct CreateInfo
    {
        std::string title = "VulkanEngine";
        int width = 1920;
        int height = 1080;
        bool resizable = true;
    };

    static std::optional<Window> Create(const vk::raii::Instance& instance, const CreateInfo& info = {});

    Window(Window&& other) noexcept
        : m_window(std::exchange(other.m_window, nullptr))
        , m_surface(std::move(other.m_surface))
        , m_width(other.m_width)
        , m_height(other.m_height)
        , m_resizeCallback(std::move(other.m_resizeCallback))
    {
        // Update GLFW user pointer to point to the new (moved-to) Window.
        // Without this, the pointer - set to &result in Create() - becomes
        // dangling after the implicit move into std::optional, and any
        // subsequent framebuffer-resize callback writes to freed stack memory
        // (undefined behavior that can set the GLFW close flag as a side effect).
        if (m_window)
            glfwSetWindowUserPointer(m_window, this);
    }
    Window& operator=(Window&& other) noexcept
    {
        if (this != &other)
        {
            m_surface.clear();
            if (m_window) glfwDestroyWindow(m_window);
            m_window = std::exchange(other.m_window, nullptr);
            m_surface = std::move(other.m_surface);
            m_width = other.m_width;
            m_height = other.m_height;
            m_resizeCallback = std::move(other.m_resizeCallback);

            if (m_window)
                glfwSetWindowUserPointer(m_window, this);
        }
        return *this;
    }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    ~Window();

    // ── 访问器 ──
    GLFWwindow* GetHandle() const { return m_window; }
    vk::raii::SurfaceKHR& GetSurface() { return m_surface; }
    const vk::raii::SurfaceKHR& GetSurface() const { return m_surface; }

    // ── Surface 重建（用于驱动 Bug 恢复） ──
    void RecreateSurface(const vk::raii::Instance& instance);

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    bool IsMinimized() const;
    bool ShouldClose() const;

    // ── 回调 ──
    using ResizeCallback = std::function<void(int width, int height)>;
    void SetResizeCallback(ResizeCallback cb) { m_resizeCallback = std::move(cb); }

    // ── 帧控制 ──
    void PollEvents();

private:
    Window() = default;

    static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);

    GLFWwindow* m_window{nullptr};
    vk::raii::SurfaceKHR m_surface{nullptr};

    int m_width{0};
    int m_height{0};
    ResizeCallback m_resizeCallback;
};
