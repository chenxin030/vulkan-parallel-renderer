#include "Core/Engine.hpp"
#include "Utils/DebugUtils.hpp"
#include "Utils/Tracy.hpp"

#include <spdlog/spdlog.h>
#include <GLFW/glfw3.h>

#ifdef VK_USE_PLATFORM_WIN32_KHR
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <thread>
#include <vector>

// ═══════════════════════════════════════════════
// 静态创建 — 按依赖顺序初始化所有 Vulkan 对象
// ═══════════════════════════════════════════════

std::optional<Engine> Engine::Create()
{
    Engine engine;

    // ── 1. Platform detection ──
    engine.m_platform = engine.DetectPlatform();
    spdlog::info("Platform: {}", engine.m_platform == Platform::Desktop ? "Desktop" : "Mobile");

    // ── 2. GLFW Init (must be before Instance for glfwGetRequiredInstanceExtensions) ──
    if (!glfwInit())
    {
        spdlog::critical("Failed to initialize GLFW.");
        return std::nullopt;
    }

    // ── 3. Instance ──
    Instance::CreateInfo instanceCI;
    instanceCI.applicationName = "VulkanEngine";
    instanceCI.apiVersion = VK_API_VERSION_1_2;  // SPIR-V 1.5 support
    instanceCI.enableValidation = true;

    auto instance = Instance::Create(instanceCI);
    if (!instance)
    {
        spdlog::critical("Failed to create Instance.");
        return std::nullopt;
    }
    engine.m_instance = std::make_unique<Instance>(std::move(*instance));

    // ── 4. Window ──
    Window::CreateInfo windowCI;
    windowCI.title = "VulkanEngine";
    windowCI.width = 1920;
    windowCI.height = 1080;
    windowCI.resizable = true;

    auto window = Window::Create(engine.m_instance->Get(), windowCI);
    if (!window)
    {
        spdlog::critical("Failed to create Window.");
        return std::nullopt;
    }
    engine.m_window = std::make_unique<Window>(std::move(*window));

    // Pump initial window messages so that surface capabilities queries return
    // sane values (mitigates garbage-capabilities bug on Intel UHD Graphics).
    glfwPollEvents();

    // ── 5. Resize callback ──
    engine.m_window->SetResizeCallback([&engine](int, int)
    {
        engine.m_swapChainDirty = true;
    });

    // ── 6. Device ──
    Device::CreateInfo deviceCI;
    deviceCI.instance = &engine.m_instance->Get();
    deviceCI.surface = &engine.m_window->GetSurface();
    deviceCI.enableValidation = true;
    deviceCI.requiredDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        // VK_KHR_dynamic_rendering requires VK_KHR_depth_stencil_resolve on some
        // drivers (e.g. Intel UHD Graphics) that expose it as a dependency.
        VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
    };

    auto device = Device::Create(deviceCI);
    if (!device)
    {
        spdlog::critical("Failed to create Device.");
        return std::nullopt;
    }
    engine.m_device = std::make_unique<Device>(std::move(*device));

    // ── 6.5. Wait for surface capabilities to stabilize ──
    // On Windows 11 + some GPU drivers (especially Intel UHD Graphics),
    // vkGetPhysicalDeviceSurfaceCapabilitiesKHR returns garbage values
    // (e.g. currentExtent=3840803712x378) if the window hasn't been fully
    // realized by the DWM yet, or — in worse cases — the driver's
    // implementation is permanently broken for the current surface.
    //
    // Strategy: poll with ~1-frame delays.  If caps never become sane,
    // destroy + recreate the surface once (the window is much more
    // "settled" by now after Device creation), then poll again.
    // If still garbage, proceed anyway — SwapChain::ChooseExtent() has
    // its own defense-in-depth, and the debug callback suppresses the
    // inevitable validation false-positive for this VUID.
    {
        auto* physDevice = &engine.m_device->GetPhysicalDevice();
        auto& surfaceRAII = engine.m_window->GetSurface();
        auto& instanceRAII = engine.m_instance->Get();

        auto isValid = [](uint32_t v) { return v > 0 && v <= 16384; };

        auto capsAreSane = [&](const vk::SurfaceCapabilitiesKHR& caps) -> bool {
            bool curOk = (caps.currentExtent.width == UINT32_MAX) ||
                         (isValid(caps.currentExtent.width) &&
                          isValid(caps.currentExtent.height));
            bool minOk = isValid(caps.minImageExtent.width) &&
                         isValid(caps.minImageExtent.height);
            return curOk && minOk;
        };

        bool stabilized = false;

        // ── Phase A: poll with ~16 ms delays (one frame) ──
        {
            constexpr int kMaxAttempts = 60;  // ~1 s total
            for (int attempt = 0; attempt < kMaxAttempts; ++attempt)
            {
                auto capResult = physDevice->getSurfaceCapabilitiesKHR(*surfaceRAII);
                if (capResult.result != vk::Result::eSuccess) break;

                const auto& caps = capResult.value;
                if (capsAreSane(caps))
                {
                    stabilized = true;
                    if (attempt > 0)
                    {
                        spdlog::debug("Surface caps stabilized after {} poll(s): "
                                      "cur={}x{}, min={}x{}",
                                      attempt,
                                      caps.currentExtent.width, caps.currentExtent.height,
                                      caps.minImageExtent.width, caps.minImageExtent.height);
                    }
                    break;
                }

                spdlog::debug("Surface caps still garbage (attempt {}/{}): "
                              "cur={}x{}, min={}x{}, max={}x{}",
                              attempt + 1, kMaxAttempts,
                              caps.currentExtent.width, caps.currentExtent.height,
                              caps.minImageExtent.width, caps.minImageExtent.height,
                              caps.maxImageExtent.width, caps.maxImageExtent.height);

                glfwPollEvents();
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        }

        // ── Phase B: if still garbage, recreate the surface ──
        // The window has had significant time to settle by now
        // (Device creation took non-trivial time).  A fresh surface
        // sometimes fixes the cached garbage state in the driver.
        if (!stabilized)
        {
            spdlog::warn("Surface caps still garbage after polling — "
                         "recreating surface to clear driver cache...");

            // Recreate the surface on the same window
            engine.m_window->RecreateSurface(instanceRAII);

            // Poll again with the new surface
            constexpr int kMaxAttempts2 = 30;
            for (int attempt = 0; attempt < kMaxAttempts2; ++attempt)
            {
                auto capResult = physDevice->getSurfaceCapabilitiesKHR(*surfaceRAII);
                if (capResult.result != vk::Result::eSuccess) break;

                const auto& caps = capResult.value;
                if (capsAreSane(caps))
                {
                    stabilized = true;
                    spdlog::info("Surface caps stabilized after surface recreate + {} poll(s): "
                                 "cur={}x{}, min={}x{}",
                                 attempt,
                                 caps.currentExtent.width, caps.currentExtent.height,
                                 caps.minImageExtent.width, caps.minImageExtent.height);
                    break;
                }

                glfwPollEvents();
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        }

        if (!stabilized)
        {
            spdlog::warn("Surface caps never stabilized — driver likely returns "
                         "persistent garbage.  SwapChain::ChooseExtent() will use "
                         "window dimensions as fallback, and the debug callback "
                         "will suppress the unavoidable validation false-positive.");
        }
    }

    // ── 7. SwapChain ──
    SwapChain::CreateInfo swapChainCI;
    swapChainCI.device = &engine.m_device->Get();
    swapChainCI.physicalDevice = &engine.m_device->GetPhysicalDevice();
    swapChainCI.surface = &engine.m_window->GetSurface();
    swapChainCI.graphicsFamily = engine.m_device->GetGraphicsFamily();
    swapChainCI.presentFamily = engine.m_device->GetPresentFamily();
    swapChainCI.initialWidth = engine.m_window->GetWidth();
    swapChainCI.initialHeight = engine.m_window->GetHeight();
    swapChainCI.preferHDR = (engine.m_platform == Platform::Desktop);

    auto swapChain = SwapChain::Create(swapChainCI);
    if (!swapChain)
    {
        spdlog::critical("Failed to create SwapChain.");
        return std::nullopt;
    }
    engine.m_swapChain = std::make_unique<SwapChain>(std::move(*swapChain));

    // ── 8. Pipeline ──
    if (!engine.CreatePipeline())
    {
        spdlog::critical("Failed to create Pipeline.");
        return std::nullopt;
    }

    // ── 9. Per-frame resources ──
    if (!engine.Init())
    {
        spdlog::critical("Failed to initialize per-frame resources.");
        return std::nullopt;
    }

    // ── 10. Debug Naming (SwapChain resources) ──
    {
        auto imagesResult = engine.m_swapChain->Get().getImages();
        auto& images = imagesResult.value;
        for (size_t i = 0; i < images.size(); ++i)
        {
            DebugUtils::SetName(engine.m_device->Get(), images[i], DebugUtils::SwapChainImageName(static_cast<int>(i)).c_str());
        }
        const auto& imageViews = engine.m_swapChain->GetImageViews();
        for (size_t i = 0; i < imageViews.size(); ++i)
        {
            DebugUtils::SetName(engine.m_device->Get(), imageViews[i], DebugUtils::SwapChainImageViewName(static_cast<int>(i)).c_str());
        }
    }

    // Pipeline resources
    DebugUtils::SetName(engine.m_device->Get(), engine.m_vertShaderModule, "TriangleVS");
    DebugUtils::SetName(engine.m_device->Get(), engine.m_fragShaderModule, "TriangleFS");
    DebugUtils::SetName(engine.m_device->Get(), engine.m_pipelineLayout, "TrianglePipelineLayout");
    DebugUtils::SetName(engine.m_device->Get(), engine.m_pipeline, "TrianglePipeline");

    // ── Final: ensure window is visible before entering main loop ──
    // Between the last glfwPollEvents() and now the Vulkan initialization
    // (Device + SwapChain + Pipeline creation) ran for ~100ms without
    // pumping window messages.  On some systems (Windows 11) the compositor
    // may hide the window during this gap, which tears down the swapchain
    // surface and causes ErrorSurfaceLostKHR on the first frame.
    glfwPollEvents();
#ifdef VK_USE_PLATFORM_WIN32_KHR
    {
        HWND hwnd = glfwGetWin32Window(engine.m_window->GetHandle());
        ShowWindow(hwnd, SW_SHOW);
    }
#endif
    glfwShowWindow(engine.m_window->GetHandle());
    glfwPollEvents();

    spdlog::info("Engine created successfully.");
    return engine;
}

// ═══════════════════════════════════════════════

Engine::~Engine()
{
    // vk::raii 成员自动销毁，但顺序重要 — 先等设备空闲
    if (m_device)
    {
        (void)m_device->Get().waitIdle();
    }

    // 按依赖逆序销毁（unique_ptr + vk::raii 自动处理）
    m_pipeline = vk::raii::Pipeline(nullptr);
    m_pipelineLayout = vk::raii::PipelineLayout(nullptr);
    m_fragShaderModule = vk::raii::ShaderModule(nullptr);
    m_vertShaderModule = vk::raii::ShaderModule(nullptr);
    m_frames = {}; // array of FrameResources
    m_renderFinishedSemaphores.clear();
    m_swapChain.reset();
    m_device.reset();
    m_window.reset();      // destroys GLFW window + Vulkan surface
    m_instance.reset();

    // GLFW lifecycle — terminate AFTER window is destroyed.
    // (Window no longer calls glfwTerminate; Engine owns the GLFW lifecycle.)
    //
    // Guard: m_instance is nullptr for moved-from Engine objects (the
    // local `engine` in Create() is destroyed at function exit, which used
    // to call glfwTerminate() and tear down GLFW before main() could run).
    if (m_instance)
        glfwTerminate();

    spdlog::info("Engine destroyed.");
}

// ═══════════════════════════════════════════════
// Per-Frame 资源创建（2 in-flight）
// ═══════════════════════════════════════════════

bool Engine::Init()
{
    uint32_t graphicsFamily = m_device->GetGraphicsFamily();

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        auto& frame = m_frames[i];

        // ── CommandPool ──
        vk::CommandPoolCreateInfo poolCI{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = graphicsFamily
        };
        {
            auto result = m_device->Get().createCommandPool(poolCI);
            if (result.result != vk::Result::eSuccess)
            {
                spdlog::critical("Failed to create CommandPool[{}]: {}", i, vk::to_string(result.result));
                return false;
            }
            frame.commandPool = std::move(result.value);
        }

        // ── CommandBuffer (primary, 1 per frame) ──
        vk::CommandBufferAllocateInfo allocInfo{
            .commandPool = *frame.commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
        };
        {
            auto result = m_device->Get().allocateCommandBuffers(allocInfo);
            if (result.result != vk::Result::eSuccess)
            {
                spdlog::critical("Failed to allocate CommandBuffer[{}]: {}", i, vk::to_string(result.result));
                return false;
            }
            frame.commandBuffer = std::move(result.value[0]);
        }

        // ── Semaphore (imageAvailable — per-frame, consumed by submit) ──
        vk::SemaphoreCreateInfo semCI{};
        {
            auto result = m_device->Get().createSemaphore(semCI);
            if (result.result != vk::Result::eSuccess)
            {
                spdlog::critical("Failed to create Semaphore (imageAvailable)[{}]: {}", i, vk::to_string(result.result));
                return false;
            }
            frame.imageAvailableSemaphore = std::move(result.value);
        }

        // ── Fence (initially signaled so first wait passes) ──
        vk::FenceCreateInfo fenceCI{
            .flags = vk::FenceCreateFlagBits::eSignaled
        };
        {
            auto result = m_device->Get().createFence(fenceCI);
            if (result.result != vk::Result::eSuccess)
            {
                spdlog::critical("Failed to create Fence[{}]: {}", i, vk::to_string(result.result));
                return false;
            }
            frame.inFlightFence = std::move(result.value);
        }

        // ── Debug Names ──
        DebugUtils::SetName(m_device->Get(), frame.commandPool, DebugUtils::FrameName(i, "CommandPool").c_str());
        DebugUtils::SetName(m_device->Get(), frame.commandBuffer, DebugUtils::FrameName(i, "CommandBuffer").c_str());
        DebugUtils::SetName(m_device->Get(), frame.imageAvailableSemaphore, DebugUtils::FrameName(i, "ImageAvailable").c_str());
        DebugUtils::SetName(m_device->Get(), frame.inFlightFence, DebugUtils::FrameName(i, "InFlightFence").c_str());

        spdlog::debug("Frame {} resources created.", i);
    }

    // Per-swapchain-image render finished semaphores
    CreateRenderFinishedSemaphores();

    return true;
}

// ═══════════════════════════════════════════════
// Per-Swapchain-Image RenderFinished Semaphores
// ═══════════════════════════════════════════════
// Must be one per swapchain image, NOT per frame-in-flight.
// The presentation engine binds the semaphore to a specific image;
// reusing it with a different image before that image is re-acquired
// triggers VUID-vkQueueSubmit-pSignalSemaphores-00067.

void Engine::CreateRenderFinishedSemaphores()
{
    m_renderFinishedSemaphores.clear();

    uint32_t imageCount = m_swapChain->GetImageCount();
    vk::SemaphoreCreateInfo semCI{};

    for (uint32_t i = 0; i < imageCount; i++)
    {
        auto result = m_device->Get().createSemaphore(semCI);
        if (result.result != vk::Result::eSuccess)
        {
            spdlog::error("Failed to create renderFinishedSemaphore[{}]: {}",
                         i, vk::to_string(result.result));
            return;
        }
        m_renderFinishedSemaphores.push_back(std::move(result.value));

        DebugUtils::SetName(m_device->Get(),
                           m_renderFinishedSemaphores[i],
                           ("RenderFinished_Img" + std::to_string(i)).c_str());
    }

    spdlog::debug("Created {} render-finished semaphores (per swapchain image).", imageCount);
}

// ═══════════════════════════════════════════════
// Pipeline 创建（Dynamic Rendering）
// ═══════════════════════════════════════════════

bool Engine::CreatePipeline()
{
    // ── 1. Load SPIR-V shaders ──
    auto loadShader = [](const char* filename) -> std::vector<uint32_t>
    {
        std::string path = std::string(VK_SHADERS_DIR) + filename;
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            spdlog::error("Cannot open shader file: {}", path);
            return {};
        }

        std::streamsize size = file.tellg();
        file.seekg(0);

        std::vector<uint32_t> code(size / sizeof(uint32_t));
        file.read(reinterpret_cast<char*>(code.data()), size);
        file.close();

        spdlog::debug("Loaded shader: {} ({} bytes)", path, size);
        return code;
    };

    auto vertCode = loadShader("triangle_vert.spv");
    auto fragCode = loadShader("triangle_frag.spv");
    if (vertCode.empty() || fragCode.empty())
    {
        spdlog::error("Failed to load shader SPIR-V files.");
        return false;
    }

    // ── 2. Create ShaderModules ──
    vk::ShaderModuleCreateInfo vertCI{
        .codeSize = vertCode.size() * sizeof(uint32_t),
        .pCode = vertCode.data()
    };
    vk::ShaderModuleCreateInfo fragCI{
        .codeSize = fragCode.size() * sizeof(uint32_t),
        .pCode = fragCode.data()
    };

    {
        auto vertResult = m_device->Get().createShaderModule(vertCI);
        if (vertResult.result != vk::Result::eSuccess)
        {
            spdlog::critical("Failed to create vertex shader module: {}", vk::to_string(vertResult.result));
            return false;
        }
        m_vertShaderModule = std::move(vertResult.value);
    }
    {
        auto fragResult = m_device->Get().createShaderModule(fragCI);
        if (fragResult.result != vk::Result::eSuccess)
        {
            spdlog::critical("Failed to create fragment shader module: {}", vk::to_string(fragResult.result));
            return false;
        }
        m_fragShaderModule = std::move(fragResult.value);
    }

    // ── 3. PipelineLayout (empty — no descriptors for Phase 1) ──
    vk::PipelineLayoutCreateInfo pipelineLayoutCI{};
    {
        auto layoutResult = m_device->Get().createPipelineLayout(pipelineLayoutCI);
        if (layoutResult.result != vk::Result::eSuccess)
        {
            spdlog::critical("Failed to create PipelineLayout: {}", vk::to_string(layoutResult.result));
            return false;
        }
        m_pipelineLayout = std::move(layoutResult.value);
    }

    // ── 4. Shader stages ──
    std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {
        vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = *m_vertShaderModule,
            .pName = "main"
        },
        vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = *m_fragShaderModule,
            .pName = "main"
        },
    };

    // ── 5. Vertex input (no vertex buffer for Phase 1) ──
    vk::PipelineVertexInputStateCreateInfo vertexInputState{};

    // ── 6. Input assembly ──
    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState{
        .topology = vk::PrimitiveTopology::eTriangleList
    };

    // ── 7. Viewport + Scissor (dynamic) ──
    vk::PipelineViewportStateCreateInfo viewportState{
        .viewportCount = 1,
        .scissorCount = 1
    };

    // ── 8. Rasterizer ──
    vk::PipelineRasterizationStateCreateInfo rasterizerState{
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .lineWidth = 1.0f
    };

    // ── 9. Multisampling ──
    vk::PipelineMultisampleStateCreateInfo multisampleState{
        .rasterizationSamples = vk::SampleCountFlagBits::e1
    };

    // ── 10. Blend (no blending, single attachment) ──
    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
        .colorWriteMask = vk::ColorComponentFlagBits::eR |
                          vk::ColorComponentFlagBits::eG |
                          vk::ColorComponentFlagBits::eB |
                          vk::ColorComponentFlagBits::eA
    };

    vk::PipelineColorBlendStateCreateInfo colorBlendState{
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment
    };

    // ── 11. Dynamic states ──
    std::array<vk::DynamicState, 2> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };
    vk::PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };

    // ── 12. Dynamic Rendering pNext ──
    vk::Format colorFormat = m_swapChain->GetFormat();
    vk::PipelineRenderingCreateInfoKHR renderingCreateInfo{
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colorFormat
    };

    // ── 13. Pipeline CI ──
    // NOTE: pNext must come early (second field in GraphicsPipelineCreateInfo)
    vk::GraphicsPipelineCreateInfo pipelineCI{
        .pNext = &renderingCreateInfo,
        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInputState,
        .pInputAssemblyState = &inputAssemblyState,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizerState,
        .pMultisampleState = &multisampleState,
        .pColorBlendState = &colorBlendState,
        .pDynamicState = &dynamicState,
        .layout = *m_pipelineLayout,
        .renderPass = VK_NULL_HANDLE,  // dynamic rendering
        .subpass = 0
    };

    {
        auto pipelineResult = m_device->Get().createGraphicsPipeline(nullptr, pipelineCI);
        if (pipelineResult.result != vk::Result::eSuccess)
        {
            spdlog::critical("Failed to create Pipeline: {}", vk::to_string(pipelineResult.result));
            return false;
        }
        m_pipeline = std::move(pipelineResult.value);
    }

    spdlog::info("Pipeline created (dynamic rendering, format={})",
                 vk::to_string(m_swapChain->GetFormat()));
    return true;
}

// ═══════════════════════════════════════════════
// 平台检测
// ═══════════════════════════════════════════════

Engine::Platform Engine::DetectPlatform() const
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    return Platform::Desktop;
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
    return Platform::Mobile;
#else
    return Platform::Desktop; // 默认
#endif
}

// ═══════════════════════════════════════════════
// Run — 进入主循环
// ═══════════════════════════════════════════════

void Engine::Run()
{
    spdlog::info("Entering main loop...");
    MainLoop();
}

// ═══════════════════════════════════════════════
// 主循环
// ═══════════════════════════════════════════════

void Engine::MainLoop()
{
    while (!m_window->ShouldClose())
    {
        m_window->PollEvents();

        if (m_window->IsMinimized())
        {
            // Window framebuffer is 0×0 — the compositor may have hidden
            // the window during Vulkan init (~100ms of no message pumping).
            // Show it and continue; the swapchain will be recreated if the
            // surface was lost.
#ifdef VK_USE_PLATFORM_WIN32_KHR
            {
                HWND hwnd = glfwGetWin32Window(m_window->GetHandle());
                ShowWindow(hwnd, SW_SHOW);
                SetForegroundWindow(hwnd);
            }
#endif
            glfwShowWindow(m_window->GetHandle());
            // Fall through — render even with 0×0 framebuffer.
            // RecreateSwapChain handles ErrorSurfaceLostKHR.
        }

        if (m_swapChainDirty)
        {
            RecreateSwapChain();
        }

        DrawFrame(m_currentFrame);
        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

        VK_TRACY_FRAME_MARK("MainLoop");
    }

    // 等待 GPU 完成所有提交
    (void)m_device->Get().waitIdle();
}

// ═══════════════════════════════════════════════
// 绘制一帧
// ═══════════════════════════════════════════════

void Engine::DrawFrame(uint32_t frameIndex)
{
    VK_TRACY_ZONE("DrawFrame");
    auto& frame = m_frames[frameIndex];

    // ── 1. Wait for previous submission of this frame slot ──
    {
        auto waitResult = m_device->Get().waitForFences(
            *frame.inFlightFence, VK_TRUE, UINT64_MAX);
        if (waitResult != vk::Result::eSuccess)
        {
            spdlog::warn("waitForFences returned {}", vk::to_string(waitResult));
        }
    }

    (void)m_device->Get().resetFences(*frame.inFlightFence);

    // ── 2. Acquire swapchain image ──
    auto acquireResult = m_swapChain->AcquireNextImage(frame.imageAvailableSemaphore);
    if (acquireResult.result == vk::Result::eErrorOutOfDateKHR ||
        acquireResult.result == vk::Result::eErrorSurfaceLostKHR)
    {
        m_swapChainDirty = true;
        return;
    }
    if (acquireResult.result != vk::Result::eSuccess &&
        acquireResult.result != vk::Result::eSuboptimalKHR)
    {
        spdlog::error("acquireNextImage failed: {}", vk::to_string(acquireResult.result));
        return;
    }

    uint32_t imageIndex = acquireResult.imageIndex;

    // ── 3. Reset & Record CommandBuffer ──
    (void)(*frame.commandBuffer).reset({});

    vk::CommandBufferBeginInfo beginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    };
    (void)(*frame.commandBuffer).begin(beginInfo);

    // Get swapchain image for barrier
    auto imagesResult = m_swapChain->Get().getImages();
    VkImage currentImage = imagesResult.value[imageIndex];

    // Image barrier: Undefined → ColorAttachmentOptimal
    {
        vk::ImageMemoryBarrier barrier{
            .srcAccessMask = {},
            .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = currentImage,
            .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
        };

        (*frame.commandBuffer).pipelineBarrier(
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            {},          // dependencyFlags
            nullptr,     // memoryBarriers
            nullptr,     // bufferMemoryBarriers
            barrier);
    }

    // ── 4. Begin Dynamic Rendering ──
    vk::RenderingAttachmentInfoKHR colorAttachment{
        .imageView = m_swapChain->GetImageViews()[imageIndex],
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = {std::array<float, 4>{0.1f, 0.15f, 0.2f, 1.0f}}
    };

    vk::RenderingInfoKHR renderingInfo{
        .renderArea = vk::Rect2D{{0, 0}, m_swapChain->GetExtent()},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment
    };

    // Dynamic rendering via raw function pointer (avoids vcpkg header version issues)
    {
        auto vkCmdBeginRendering = (PFN_vkCmdBeginRenderingKHR)
            vkGetDeviceProcAddr(*m_device->Get(), "vkCmdBeginRenderingKHR");
        VkRenderingInfoKHR info = renderingInfo;
        vkCmdBeginRendering((VkCommandBuffer)*frame.commandBuffer, &info);
    }

    // ── 5. Draw Triangle ──
    (*frame.commandBuffer).bindPipeline(vk::PipelineBindPoint::eGraphics, *m_pipeline);

    vk::Viewport viewport{
        0.0f, 0.0f,
        static_cast<float>(m_swapChain->GetExtent().width),
        static_cast<float>(m_swapChain->GetExtent().height),
        0.0f, 1.0f
    };
    (*frame.commandBuffer).setViewport(0, viewport);

    vk::Rect2D scissor{{0, 0}, m_swapChain->GetExtent()};
    (*frame.commandBuffer).setScissor(0, scissor);

    (*frame.commandBuffer).draw(3, 1, 0, 0);

    {
        auto vkCmdEndRendering = (PFN_vkCmdEndRenderingKHR)
            vkGetDeviceProcAddr(*m_device->Get(), "vkCmdEndRenderingKHR");
        vkCmdEndRendering((VkCommandBuffer)*frame.commandBuffer);
    }

    // Image barrier: ColorAttachmentOptimal → PresentSrcKHR
    {
        vk::ImageMemoryBarrier barrier{
            .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
            .dstAccessMask = {},
            .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .newLayout = vk::ImageLayout::ePresentSrcKHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = currentImage,
            .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
        };

        (*frame.commandBuffer).pipelineBarrier(
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::PipelineStageFlagBits::eBottomOfPipe,
            {},
            nullptr, nullptr, barrier);
    }

    (void)(*frame.commandBuffer).end();

    // ── 6. Submit ──
    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

    vk::Semaphore waitSemaphore = *frame.imageAvailableSemaphore;
    vk::CommandBuffer cmdBuffer = *frame.commandBuffer;
    vk::Semaphore signalSemaphore = *m_renderFinishedSemaphores[imageIndex];

    vk::SubmitInfo submitInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &waitSemaphore,
        .pWaitDstStageMask = &waitStage,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmdBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &signalSemaphore
    };

    {
        auto submitResult = m_device->GetGraphicsQueue().submit(submitInfo, *frame.inFlightFence);
        if (submitResult != vk::Result::eSuccess)
        {
            spdlog::error("Queue submit failed: {}", vk::to_string(submitResult));
            return;
        }
    }

    // ── 7. Present ──
    vk::Result presentResult = m_swapChain->Present(
        m_device->GetPresentQueue(),
        imageIndex,
        m_renderFinishedSemaphores[imageIndex]);

    if (presentResult == vk::Result::eErrorOutOfDateKHR ||
        presentResult == vk::Result::eSuboptimalKHR ||
        presentResult == vk::Result::eErrorSurfaceLostKHR)
    {
        m_swapChainDirty = true;
    }
    else if (presentResult != vk::Result::eSuccess)
    {
        spdlog::error("Present failed: {}", vk::to_string(presentResult));
    }
}

// ═══════════════════════════════════════════════
// SwapChain 重建
// ═══════════════════════════════════════════════

void Engine::RecreateSwapChain()
{
    (void)m_device->Get().waitIdle();

    spdlog::info("Recreating SwapChain ({}x{})...",
                 m_window->GetWidth(), m_window->GetHeight());

    // If the surface has been lost (ErrorSurfaceLostKHR), recreate it
    // before trying to build a new swapchain.  The surface can be lost
    // when the window is hidden by the compositor during lengthy init.
    //
    // Use the raw C API to avoid Vulkan-Hpp Debug assertions —
    // the raii getSurfaceCapabilitiesKHR wrapper defaults to
    // {eSuccess} as the only valid result and would assert on
    // ErrorSurfaceLostKHR.
    {
        VkSurfaceCapabilitiesKHR caps;
        VkResult vkResult = m_device->GetPhysicalDevice().getDispatcher()->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            static_cast<VkPhysicalDevice>(*m_device->GetPhysicalDevice()),
            *m_window->GetSurface(),
            &caps);
        if (vkResult == VK_ERROR_SURFACE_LOST_KHR)
        {
            spdlog::info("Surface lost — recreating before SwapChain rebuild...");
            m_window->RecreateSurface(m_instance->Get());
        }
    }

    m_swapChain->SetWindowDimensions(m_window->GetWidth(), m_window->GetHeight());
    m_swapChain->CreateSwapChain();
    m_swapChain->CreateImageViews();
    CreateRenderFinishedSemaphores();
    m_swapChain->ClearNeedsRecreate();
    m_swapChainDirty = false;

    spdlog::info("SwapChain recreated: {}x{}",
                 m_swapChain->GetExtent().width,
                 m_swapChain->GetExtent().height);
}
