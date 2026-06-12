#pragma once

#include <optional>
#include <vector>

#ifndef VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_EXCEPTIONS
#endif
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

// ═══════════════════════════════════════════════
// SwapChain 封装 — HDR/SDR 双路径
// — 自动选择格式 / 色彩空间 / 呈现模式
// — 支持 Recreate（窗口 resize）
// ═══════════════════════════════════════════════

class SwapChain
{
public:
    struct CreateInfo
    {
        vk::raii::Device* device = nullptr;              // non-owning
        vk::raii::PhysicalDevice* physicalDevice = nullptr; // non-owning
        vk::raii::SurfaceKHR* surface = nullptr;         // non-owning
        uint32_t graphicsFamily = 0;
        uint32_t presentFamily = 0;
        int initialWidth = 1920;
        int initialHeight = 1080;
        bool preferHDR = true;  // Desktop=true per ADR-0015
    };

    struct AcquireResult
    {
        uint32_t imageIndex = 0;
        vk::Result result = vk::Result::eSuccess;
    };

    static std::optional<SwapChain> Create(const CreateInfo& info);

    SwapChain(SwapChain&&) noexcept = default;
    SwapChain& operator=(SwapChain&&) noexcept = default;
    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;

    // ── Per-Frame 操作 ──
    AcquireResult AcquireNextImage(const vk::raii::Semaphore& signalSemaphore);
    vk::Result Present(const vk::Queue& queue, uint32_t imageIndex, const vk::raii::Semaphore& waitSemaphore);

    // ── Recreate ──
    void SetWindowDimensions(int width, int height) { m_width = width; m_height = height; }
    void CreateSwapChain();
    void CreateImageViews();

    // ── 查询 ──
    bool NeedsRecreate() const { return m_needsRecreate; }
    void ClearNeedsRecreate() { m_needsRecreate = false; }

    vk::raii::SwapchainKHR& Get() { return m_swapChain; }
    const vk::raii::SwapchainKHR& Get() const { return m_swapChain; }
    VkSwapchainKHR GetRaw() const { return *m_swapChain; }

    vk::Format GetFormat() const { return m_format; }
    vk::ColorSpaceKHR GetColorSpace() const { return m_colorSpace; }
    vk::Extent2D GetExtent() const { return m_extent; }
    vk::PresentModeKHR GetPresentMode() const { return m_presentMode; }
    uint32_t GetImageCount() const { return m_imageCount; }
    bool IsHDR() const { return m_hdr; }

    const std::vector<vk::raii::ImageView>& GetImageViews() const { return m_imageViews; }

private:
    SwapChain() = default;

    // 格式选择
    static std::optional<std::pair<vk::Format, vk::ColorSpaceKHR>> ChooseFormat(
        const std::vector<vk::SurfaceFormatKHR>& available,
        bool preferHDR);

    // 呈现模式选择
    static vk::PresentModeKHR ChoosePresentMode(
        const std::vector<vk::PresentModeKHR>& available,
        bool isDesktop);

    // Extent 选择
    static vk::Extent2D ChooseExtent(
        const vk::SurfaceCapabilitiesKHR& capabilities,
        int width, int height);

    // Non-owning references (set during Create)
    vk::raii::Device* m_device{nullptr};
    vk::raii::PhysicalDevice* m_physicalDevice{nullptr};
    vk::raii::SurfaceKHR* m_surface{nullptr};

    uint32_t m_graphicsFamily{0};
    uint32_t m_presentFamily{0};

    // Owning resources
    vk::raii::SwapchainKHR m_swapChain{nullptr};
    std::vector<vk::raii::ImageView> m_imageViews;

    // SwapChain state
    vk::Format m_format{vk::Format::eUndefined};
    vk::ColorSpaceKHR m_colorSpace{vk::ColorSpaceKHR::eSrgbNonlinear};
    vk::Extent2D m_extent{0, 0};
    vk::PresentModeKHR m_presentMode{vk::PresentModeKHR::eFifo};
    uint32_t m_imageCount{0};
    bool m_hdr{false};

    // Window dimensions (for recreate)
    int m_width{0};
    int m_height{0};
    bool m_preferHDR{true};

    // State
    bool m_needsRecreate{false};
};
