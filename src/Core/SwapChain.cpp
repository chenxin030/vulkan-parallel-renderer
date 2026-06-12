#include "Core/SwapChain.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <set>

// ═══════════════════════════════════════════════
// 格式选择 — HDR 优先，否则 SRGB
// ═══════════════════════════════════════════════

std::optional<std::pair<vk::Format, vk::ColorSpaceKHR>> SwapChain::ChooseFormat(
    const std::vector<vk::SurfaceFormatKHR>& available,
    bool preferHDR)
{
    // 如果只有一个 UNDEFINED，表示 surface 支持任意格式
    if (available.size() == 1 && available[0].format == vk::Format::eUndefined)
    {
        // 默认 SDR
        return std::make_pair(vk::Format::eR8G8B8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear);
    }

    // HDR 路径（Desktop）
    if (preferHDR)
    {
        for (const auto& fmt : available)
        {
            if (fmt.format == vk::Format::eR16G16B16A16Sfloat &&
                fmt.colorSpace == vk::ColorSpaceKHR::eHdr10St2084EXT)
            {
                spdlog::info("SwapChain: HDR mode (R16G16B16A16_SFLOAT + HDR10 ST2084)");
                return std::make_pair(fmt.format, fmt.colorSpace);
            }
        }
    }

    // SDR 回退：SRGB
    for (const auto& fmt : available)
    {
        if (fmt.format == vk::Format::eR8G8B8A8Srgb &&
            fmt.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            spdlog::info("SwapChain: SDR mode (R8G8B8A8_SRGB + SRGB_NONLINEAR)");
            return std::make_pair(fmt.format, fmt.colorSpace);
        }
    }

    // 再回退：R8G8B8A8_UNORM
    for (const auto& fmt : available)
    {
        if (fmt.format == vk::Format::eR8G8B8A8Unorm &&
            fmt.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            spdlog::info("SwapChain: SDR mode (R8G8B8A8_UNORM + SRGB_NONLINEAR)");
            return std::make_pair(fmt.format, fmt.colorSpace);
        }
    }

    // 最终回退：取第一个
    spdlog::warn("SwapChain: falling back to first available format ({})", vk::to_string(available[0].format));
    return std::make_pair(available[0].format, available[0].colorSpace);
}

// ═══════════════════════════════════════════════
// 呈现模式选择 — 桌面 MAILBOX / 移动 FIFO
// ═══════════════════════════════════════════════

vk::PresentModeKHR SwapChain::ChoosePresentMode(
    const std::vector<vk::PresentModeKHR>& available,
    bool isDesktop)
{
    if (isDesktop)
    {
        // MAILBOX: 非阻塞 vsync，最低延迟
        for (const auto& mode : available)
        {
            if (mode == vk::PresentModeKHR::eMailbox)
            {
                spdlog::info("SwapChain: PresentMode = MAILBOX");
                return mode;
            }
        }
    }

    // 回退：FIFO（vsync，保证存在）
    spdlog::info("SwapChain: PresentMode = FIFO");
    return vk::PresentModeKHR::eFifo;
}

// ═══════════════════════════════════════════════
// Extent 选择
// ═══════════════════════════════════════════════

vk::Extent2D SwapChain::ChooseExtent(
    const vk::SurfaceCapabilitiesKHR& capabilities,
    int width, int height)
{
    // Per-format maxImageDimension2D 至少 4096，多数 GPU 为 16384。
    // surface capabilities 的 maxImageExtent 可能为 UINT32_MAX（无上限），
    // 但 format-specific limit 可能更严格 — 加硬上限防止垃圾值透传。
    // 16K 足够覆盖所有实际用例（8K 渲染 + 超级采样也远小于此值）。
    constexpr uint32_t kAbsoluteMaxExtent = 16384;

    // ── 防御：验证 capabilities 字段是否在合理范围内 ──
    // 驱动在窗口未完全初始化时可能返回垃圾值（Windows 11 + 特定 GPU）。
    auto isValidDimension = [](uint32_t val) -> bool {
        return val > 0 && val <= kAbsoluteMaxExtent;
    };

    bool capsSane =
        isValidDimension(capabilities.minImageExtent.width) &&
        isValidDimension(capabilities.minImageExtent.height) &&
        (capabilities.maxImageExtent.width == UINT32_MAX ||
         isValidDimension(capabilities.maxImageExtent.width)) &&
        (capabilities.maxImageExtent.height == UINT32_MAX ||
         isValidDimension(capabilities.maxImageExtent.height));

    if (!capsSane)
    {
        spdlog::warn("SwapChain: surface capabilities out of sane range "
                     "(min={}x{}, max={}x{}, cur={}x{}), using window dimensions",
                     capabilities.minImageExtent.width, capabilities.minImageExtent.height,
                     capabilities.maxImageExtent.width, capabilities.maxImageExtent.height,
                     capabilities.currentExtent.width, capabilities.currentExtent.height);
        // 使用窗口尺寸，硬 clamp 到绝对上限
        return vk::Extent2D{
            std::min(static_cast<uint32_t>(std::max(width, 1)), kAbsoluteMaxExtent),
            std::min(static_cast<uint32_t>(std::max(height, 1)), kAbsoluteMaxExtent)
        };
    }

    // ── 安全 clamp：lo > hi 时返回硬上限而非垃圾 lo ──
    auto safeClamp = [](uint32_t val, uint32_t lo, uint32_t hi) -> uint32_t {
        if (lo > hi) {
            // 退化范围：驱动返回了不一致的 min/max。
            // 回退到硬上限，绝不返回可能为垃圾的 lo。
            spdlog::warn("SwapChain: degenerate extent range [{}, {}], falling back to hard cap {}",
                         lo, hi, kAbsoluteMaxExtent);
            return std::min(val, kAbsoluteMaxExtent);
        }
        if (val < lo) return lo;
        if (val > hi) return hi;
        return val;
    };

    auto clampWidth = [&](uint32_t w) {
        uint32_t maxW = std::min(capabilities.maxImageExtent.width, kAbsoluteMaxExtent);
        return safeClamp(w, capabilities.minImageExtent.width, maxW);
    };
    auto clampHeight = [&](uint32_t h) {
        uint32_t maxH = std::min(capabilities.maxImageExtent.height, kAbsoluteMaxExtent);
        return safeClamp(h, capabilities.minImageExtent.height, maxH);
    };

    if (capabilities.currentExtent.width != UINT32_MAX &&
        capabilities.currentExtent.height != UINT32_MAX)
    {
        uint32_t curW = capabilities.currentExtent.width;
        uint32_t curH = capabilities.currentExtent.height;

        // 验证 currentExtent 是否在合法范围内（驱动可能返回零或垃圾值）
        bool valid = (curW > 0 && curH > 0 &&
                      curW >= capabilities.minImageExtent.width &&
                      curH >= capabilities.minImageExtent.height &&
                      curW <= capabilities.maxImageExtent.width &&
                      curH <= capabilities.maxImageExtent.height);

        if (valid)
        {
            // 仍然过一遍安全 clamp（处理 maxImageExtent = UINT32_MAX 的情况）
            return vk::Extent2D{clampWidth(curW), clampHeight(curH)};
        }

        spdlog::warn("SwapChain: currentExtent ({}x{}) out of bounds, falling back to window dimensions ({}x{})",
                     curW, curH, width, height);
    }

    // 回退：使用窗口尺寸手动 clamp
    return vk::Extent2D{
        clampWidth(static_cast<uint32_t>(width)),
        clampHeight(static_cast<uint32_t>(height))
    };
}

// ═══════════════════════════════════════════════
// 静态创建
// ═══════════════════════════════════════════════

std::optional<SwapChain> SwapChain::Create(const CreateInfo& info)
{
    if (!info.device || !info.physicalDevice || !info.surface)
    {
        spdlog::critical("SwapChain::CreateInfo: device, physicalDevice, and surface must not be null.");
        return std::nullopt;
    }

    SwapChain result;

    result.m_device = info.device;
    result.m_physicalDevice = info.physicalDevice;
    result.m_surface = info.surface;
    result.m_graphicsFamily = info.graphicsFamily;
    result.m_presentFamily = info.presentFamily;
    result.m_width = info.initialWidth;
    result.m_height = info.initialHeight;
    result.m_preferHDR = info.preferHDR;

    result.CreateSwapChain();
    if (!*result.m_swapChain)
    {
        spdlog::critical("SwapChain creation failed.");
        return std::nullopt;
    }

    result.CreateImageViews();

    spdlog::info("SwapChain created: format={}, extent={}x{}, imageCount={}",
                 vk::to_string(result.m_format),
                 result.m_extent.width, result.m_extent.height,
                 result.m_imageCount);

    return result;
}

// ═══════════════════════════════════════════════
// 创建 VkSwapchainKHR
// ═══════════════════════════════════════════════

void SwapChain::CreateSwapChain()
{
    // 等待之前的 swapchain 被释放（raii 自动处理旧对象的析构）

    auto capResult = m_physicalDevice->getSurfaceCapabilitiesKHR(**m_surface);
    if (capResult.result != vk::Result::eSuccess)
    {
        spdlog::error("SwapChain: getSurfaceCapabilitiesKHR failed: {}", vk::to_string(capResult.result));
        return;
    }
    vk::SurfaceCapabilitiesKHR capabilities = capResult.value;

    auto fmtResult = m_physicalDevice->getSurfaceFormatsKHR(**m_surface);
    if (fmtResult.result != vk::Result::eSuccess)
    {
        spdlog::error("SwapChain: getSurfaceFormatsKHR failed: {}", vk::to_string(fmtResult.result));
        return;
    }
    std::vector<vk::SurfaceFormatKHR> formats = fmtResult.value;

    auto modeResult = m_physicalDevice->getSurfacePresentModesKHR(**m_surface);
    if (modeResult.result != vk::Result::eSuccess)
    {
        spdlog::error("SwapChain: getSurfacePresentModesKHR failed: {}", vk::to_string(modeResult.result));
        return;
    }
    std::vector<vk::PresentModeKHR> presentModes = modeResult.value;

    // ── 选择格式 ──
    auto formatResult = ChooseFormat(formats, m_preferHDR);
    if (!formatResult)
    {
        spdlog::error("SwapChain: no suitable format found.");
        return;
    }
    m_format = formatResult->first;
    m_colorSpace = formatResult->second;
    m_hdr = (m_format == vk::Format::eR16G16B16A16Sfloat);

    // ── 选择呈现模式 ──
    bool isDesktop = m_preferHDR; // 简化：Desktop = preferHDR
    m_presentMode = ChoosePresentMode(presentModes, isDesktop);

    // ── 选择 Extent ──
    m_extent = ChooseExtent(capabilities, m_width, m_height);

    // 防御纵深：即便 ChooseExtent 因某种原因返回了超限值，
    // 在此处硬截断，确保不会传入 vkCreateSwapchainKHR 导致 abort。
    {
        constexpr uint32_t kHardLimit = 16384;
        if (m_extent.width > kHardLimit || m_extent.height > kHardLimit || m_extent.width == 0 || m_extent.height == 0)
        {
            spdlog::error("SwapChain: ChooseExtent returned invalid extent {}x{}, clamping to {}x{}",
                          m_extent.width, m_extent.height,
                          std::min(m_extent.width, kHardLimit),
                          std::min(m_extent.height, kHardLimit));
            m_extent.width = std::clamp(m_extent.width, 1u, kHardLimit);
            m_extent.height = std::clamp(m_extent.height, 1u, kHardLimit);
        }
    }

    // ── 选择图片数量 ──
    // Defense against garbage surface capabilities: minImageCount can be
    // absurdly large on broken drivers (e.g. Intel UHD Graphics on Windows 11).
    // Clamp to a sane range [2, 8] — FIFO requires ≥2, mailbox prefers 3,
    // and >8 is wasteful for any real use case.
    {
        constexpr uint32_t kMinSaneImageCount = 2;
        constexpr uint32_t kMaxSaneImageCount = 8;

        uint32_t rawMin = capabilities.minImageCount;
        m_imageCount = std::clamp(rawMin, kMinSaneImageCount, kMaxSaneImageCount);

        if (rawMin != m_imageCount)
        {
            spdlog::warn("SwapChain: minImageCount {} out of sane range [{},{}], clamped to {}",
                         rawMin, kMinSaneImageCount, kMaxSaneImageCount, m_imageCount);
        }

        if (m_presentMode == vk::PresentModeKHR::eMailbox)
        {
            m_imageCount = std::min(m_imageCount + 1, kMaxSaneImageCount);
        }
        if (capabilities.maxImageCount > 0 && m_imageCount > capabilities.maxImageCount)
        {
            m_imageCount = capabilities.maxImageCount;
        }
    }

    // ── Sharing Mode ──
    std::vector<uint32_t> queueFamilies = {m_graphicsFamily, m_presentFamily};
    vk::SharingMode sharingMode;
    if (m_graphicsFamily != m_presentFamily)
    {
        sharingMode = vk::SharingMode::eConcurrent;
        spdlog::debug("SwapChain: CONCURRENT sharing (graphics={}, present={})",
                       m_graphicsFamily, m_presentFamily);
    }
    else
    {
        sharingMode = vk::SharingMode::eExclusive;
        spdlog::debug("SwapChain: EXCLUSIVE sharing (same queue family={})", m_graphicsFamily);
    }

    // ── 创建 SwapChain ──
    vk::SwapchainCreateInfoKHR swapchainCI{
        .surface = **m_surface,
        .minImageCount = m_imageCount,
        .imageFormat = m_format,
        .imageColorSpace = m_colorSpace,
        .imageExtent = m_extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = sharingMode,
        .queueFamilyIndexCount = static_cast<uint32_t>(queueFamilies.size()),
        .pQueueFamilyIndices = queueFamilies.data(),
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = m_presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = *m_swapChain
    };

    {
        auto scResult = m_device->createSwapchainKHR(swapchainCI);
        if (scResult.result != vk::Result::eSuccess)
        {
            spdlog::error("Failed to create SwapChain: {}", vk::to_string(scResult.result));
            m_swapChain = vk::raii::SwapchainKHR(nullptr);
            return;
        }
        m_swapChain = std::move(scResult.value);
    }
}

// ═══════════════════════════════════════════════
// 创建 ImageViews
// ═══════════════════════════════════════════════

void SwapChain::CreateImageViews()
{
    m_imageViews.clear();

    auto imagesResult = m_swapChain.getImages();
    if (imagesResult.result != vk::Result::eSuccess)
    {
        spdlog::error("SwapChain: getImages failed: {}", vk::to_string(imagesResult.result));
        return;
    }
    const auto& swapchainImages = imagesResult.value;

    for (auto image : swapchainImages)
    {
        vk::ImageViewCreateInfo viewCI{
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = m_format,
            .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
        };

        {
            auto viewResult = m_device->createImageView(viewCI);
            if (viewResult.result == vk::Result::eSuccess)
            {
                m_imageViews.emplace_back(std::move(viewResult.value));
            }
            else
            {
                spdlog::error("Failed to create ImageView: {}", vk::to_string(viewResult.result));
            }
        }
    }
}

// ═══════════════════════════════════════════════
// Acquire — 获取下一帧的 swapchain image
// ═══════════════════════════════════════════════

SwapChain::AcquireResult SwapChain::AcquireNextImage(const vk::raii::Semaphore& signalSemaphore)
{
    AcquireResult result;

    // Use the raw C API to avoid Vulkan-Hpp Debug assertions.
    // The raii wrapper for acquireNextImageKHR only accepts
    // {eSuccess, eTimeout, eNotReady, eSuboptimalKHR, eErrorOutOfDateKHR}
    // as valid codes and ASSERTS on ErrorSurfaceLostKHR — but we need
    // to handle surface loss gracefully.
    uint32_t imageIndex = 0;
    VkResult vkResult = m_device->getDispatcher()->vkAcquireNextImageKHR(
        static_cast<VkDevice>(**m_device),
        static_cast<VkSwapchainKHR>(*m_swapChain),
        UINT64_MAX,
        static_cast<VkSemaphore>(*signalSemaphore),
        VK_NULL_HANDLE,  // no fence
        &imageIndex);

    result.result = static_cast<vk::Result>(vkResult);

    if (result.result == vk::Result::eSuccess ||
        result.result == vk::Result::eSuboptimalKHR)
    {
        result.imageIndex = imageIndex;
    }

    // 检测 out-of-date / suboptimal / surface lost
    if (result.result == vk::Result::eErrorOutOfDateKHR ||
        result.result == vk::Result::eSuboptimalKHR ||
        result.result == vk::Result::eErrorSurfaceLostKHR)
    {
        m_needsRecreate = true;
    }

    return result;
}

// ═══════════════════════════════════════════════
// Present — 呈现到屏幕
// ═══════════════════════════════════════════════

vk::Result SwapChain::Present(const vk::Queue& queue, uint32_t imageIndex, const vk::raii::Semaphore& waitSemaphore)
{
    VkSemaphore sem = static_cast<VkSemaphore>(*waitSemaphore);
    VkSwapchainKHR sc = static_cast<VkSwapchainKHR>(*m_swapChain);

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &sem,
        .swapchainCount = 1,
        .pSwapchains = &sc,
        .pImageIndices = &imageIndex
    };

    // Use raw C API to avoid Vulkan-Hpp Debug assertion.
    // The raii wrapper for presentKHR ASSERTS on SuboptimalKHR / ErrorOutOfDateKHR,
    // but we need to handle those gracefully (they just trigger a recreate).
    VkResult vkResult = m_device->getDispatcher()->vkQueuePresentKHR(
        static_cast<VkQueue>(queue),
        &presentInfo);
    vk::Result result = static_cast<vk::Result>(vkResult);

    if (result == vk::Result::eErrorOutOfDateKHR ||
        result == vk::Result::eSuboptimalKHR)
    {
        m_needsRecreate = true;
    }

    return result;
}
