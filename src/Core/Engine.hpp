#pragma once

#include <memory>
#include <optional>
#include <vector>
#include <array>

#ifndef VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_EXCEPTIONS
#endif
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Core/Instance.hpp"
#include "Core/Window.hpp"
#include "Core/Device.hpp"
#include "Core/SwapChain.hpp"

// ═══════════════════════════════════════════════
// 引擎主类 — 拥有所有 Vulkan 对象
// ═══════════════════════════════════════════════

class Engine
{
public:
    static std::optional<Engine> Create();
    ~Engine();

    Engine(Engine&&) noexcept = default;
    Engine& operator=(Engine&&) noexcept = default;
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void Run();

private:
    Engine() = default;

    bool Init();
    void MainLoop();
    void DrawFrame(uint32_t frameIndex);
    void RecreateSwapChain();
    void CreateRenderFinishedSemaphores();

    // ── 平台检测 ──
    enum class Platform { Desktop, Mobile };
    Platform DetectPlatform() const;

    // ── Vulkan 核心 ──
    std::unique_ptr<Instance> m_instance;
    std::unique_ptr<Window> m_window;
    std::unique_ptr<Device> m_device;
    std::unique_ptr<SwapChain> m_swapChain;

    bool CreatePipeline();

    // ── Per-Frame 资源（2 in-flight） ──
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    uint32_t m_currentFrame{0};

    struct FrameResources
    {
        vk::raii::CommandPool commandPool{nullptr};
        vk::raii::CommandBuffer commandBuffer{nullptr};
        vk::raii::Semaphore imageAvailableSemaphore{nullptr};
        vk::raii::Fence inFlightFence{nullptr};
    };
    std::array<FrameResources, MAX_FRAMES_IN_FLIGHT> m_frames;

    // Per-swapchain-image semaphores — indexed by acquired imageIndex.
    // renderFinished semaphores are consumed by the presentation engine and
    // may only be reused when the same swapchain image is re-acquired
    // (Vulkan spec VUID-vkQueueSubmit-pSignalSemaphores-00067).
    std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores;

    // ── Pipeline ──
    vk::raii::ShaderModule m_vertShaderModule{nullptr};
    vk::raii::ShaderModule m_fragShaderModule{nullptr};
    vk::raii::PipelineLayout m_pipelineLayout{nullptr};
    vk::raii::Pipeline m_pipeline{nullptr};

    // ── 平台 ──
    Platform m_platform{Platform::Desktop};
    bool m_swapChainDirty{false};
};
