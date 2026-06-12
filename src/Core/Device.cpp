#include "Core/Device.hpp"

#include <spdlog/spdlog.h>

#include <set>

// ═══════════════════════════════════════════════
// 物理设备评分 — 越高越好
// ═══════════════════════════════════════════════

int Device::RatePhysicalDevice(const vk::raii::PhysicalDevice& device)
{
    vk::PhysicalDeviceProperties props = device.getProperties();
    vk::PhysicalDeviceFeatures features = device.getFeatures();

    int score = 0;

    // 类型评分
    switch (props.deviceType)
    {
    case vk::PhysicalDeviceType::eDiscreteGpu:
        score += 1000; // 最强优先
        break;
    case vk::PhysicalDeviceType::eIntegratedGpu:
        score += 100;
        break;
    case vk::PhysicalDeviceType::eVirtualGpu:
        score += 10;
        break;
    default:
        score += 1;
        break;
    }

    // VRAM 大小作为次要参考（粗略估算：maxAllocationSize 不是总 VRAM，仅用做相对比较）
    score += static_cast<int>(props.limits.maxMemoryAllocationCount / (1024 * 1024));

    // 需要 samplerAnisotropy
    if (!features.samplerAnisotropy)
    {
        score = 0;
    }

    return score;
}

// ═══════════════════════════════════════════════
// Queue Family 探测
// ═══════════════════════════════════════════════

std::optional<Device::QueueFamilyIndices> Device::FindQueueFamilies(
    const vk::raii::PhysicalDevice& physicalDevice,
    const vk::raii::SurfaceKHR& surface)
{
    QueueFamilyIndices indices;

    std::vector<vk::QueueFamilyProperties> queueFamilies = physicalDevice.getQueueFamilyProperties();

    for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilies.size()); ++i)
    {
        const auto& qf = queueFamilies[i];

        // Graphics
        if (qf.queueFlags & vk::QueueFlagBits::eGraphics)
        {
            indices.graphicsFamily = i;
        }

        // Compute (优先选非 graphics 的 dedicated compute queue)
        if (qf.queueFlags & vk::QueueFlagBits::eCompute && !indices.computeFamily.has_value())
        {
            indices.computeFamily = i;
        }

        // Transfer (优先选非 graphics/compute 的 dedicated transfer queue)
        if (qf.queueFlags & vk::QueueFlagBits::eTransfer && !indices.transferFamily.has_value())
        {
            if (!(qf.queueFlags & vk::QueueFlagBits::eGraphics) &&
                !(qf.queueFlags & vk::QueueFlagBits::eCompute))
            {
                indices.transferFamily = i;
            }
        }

        // Present — 使用二参数版本避免 VULKAN_HPP_NO_EXCEPTIONS 下的 assert
        vk::Bool32 supported;
        if ((*physicalDevice).getSurfaceSupportKHR(i, *surface, &supported) == vk::Result::eSuccess && supported)
        {
            indices.presentFamily = i;
        }

        // 全部找到就提前退出
        if (indices.IsComplete() &&
            indices.computeFamily.has_value() &&
            indices.transferFamily.has_value())
        {
            break;
        }
    }

    // 如果没有专用的 transfer queue，回落到 graphics family
    if (!indices.transferFamily.has_value())
    {
        indices.transferFamily = indices.graphicsFamily;
    }

    // 如果没有专用的 compute queue，回落到 graphics family
    if (!indices.computeFamily.has_value())
    {
        indices.computeFamily = indices.graphicsFamily;
    }

    return indices;
}

// ═══════════════════════════════════════════════
// 设备扩展检测
// ═══════════════════════════════════════════════

bool Device::CheckDeviceExtensions(
    const vk::raii::PhysicalDevice& physicalDevice,
    const std::vector<const char*>& requiredExtensions)
{
    auto extResult = physicalDevice.enumerateDeviceExtensionProperties();
    const auto& availableExtensions = extResult.value;

    std::set<std::string> required(requiredExtensions.begin(), requiredExtensions.end());

    for (const auto& ext : availableExtensions)
    {
        required.erase(ext.extensionName);
    }

    if (!required.empty())
    {
        for (const auto& missing : required)
        {
            spdlog::warn("  Device missing extension: {}", missing);
        }
    }

    return required.empty();
}

// ═══════════════════════════════════════════════
// SwapChain 支持查询
// ═══════════════════════════════════════════════

std::optional<Device::SwapChainSupportDetails> Device::QuerySwapChainSupport(
    const vk::raii::PhysicalDevice& physicalDevice,
    const vk::raii::SurfaceKHR& surface)
{
    SwapChainSupportDetails details;

    {
        // 使用 vk::PhysicalDevice 的二参数版本 (返回 Result，不触发 assert)
        vk::SurfaceCapabilitiesKHR caps;
        auto result = (*physicalDevice).getSurfaceCapabilitiesKHR(*surface, &caps);
        if (result != vk::Result::eSuccess)
        {
            spdlog::warn("  getSurfaceCapabilitiesKHR failed: {}", vk::to_string(result));
            return std::nullopt;
        }
        details.capabilities = caps;
    }
    {
        uint32_t count = 0;
        auto result = (*physicalDevice).getSurfaceFormatsKHR(*surface, &count, nullptr);
        if (result != vk::Result::eSuccess)
        {
            spdlog::warn("  getSurfaceFormatsKHR failed: {}", vk::to_string(result));
            return std::nullopt;
        }
        details.formats.resize(count);
        if (count > 0)
        {
            result = (*physicalDevice).getSurfaceFormatsKHR(*surface, &count, details.formats.data());
            if (result != vk::Result::eSuccess)
            {
                spdlog::warn("  getSurfaceFormatsKHR (data) failed: {}", vk::to_string(result));
                return std::nullopt;
            }
        }
    }
    {
        uint32_t count = 0;
        auto result = (*physicalDevice).getSurfacePresentModesKHR(*surface, &count, nullptr);
        if (result != vk::Result::eSuccess)
        {
            spdlog::warn("  getSurfacePresentModesKHR failed: {}", vk::to_string(result));
            return std::nullopt;
        }
        details.presentModes.resize(count);
        if (count > 0)
        {
            result = (*physicalDevice).getSurfacePresentModesKHR(*surface, &count, details.presentModes.data());
            if (result != vk::Result::eSuccess)
            {
                spdlog::warn("  getSurfacePresentModesKHR (data) failed: {}", vk::to_string(result));
                return std::nullopt;
            }
        }
    }

    if (details.formats.empty() || details.presentModes.empty())
    {
        spdlog::warn("Physical device has no swapchain format or present mode.");
        return std::nullopt;
    }

    return details;
}

// ═══════════════════════════════════════════════
// 静态创建
// ═══════════════════════════════════════════════

std::optional<Device> Device::Create(const CreateInfo& info)
{
    if (!info.instance || !info.surface)
    {
        spdlog::critical("Device::CreateInfo: instance and surface must not be null.");
        return std::nullopt;
    }

    Device result;

    // ── 1. 枚举物理设备并评分 ──
    auto pdResult = info.instance->enumeratePhysicalDevices();
    auto& physicalDevices = pdResult.value;

    if (physicalDevices.empty())
    {
        spdlog::critical("No Vulkan-capable GPU found.");
        return std::nullopt;
    }

    spdlog::info("Found {} physical device(s):", physicalDevices.size());

    vk::raii::PhysicalDevice bestDevice(nullptr);
    QueueFamilyIndices bestIndices;
    SwapChainSupportDetails bestSwapChainDetails;
    int bestScore = -1;

    for (auto& device : physicalDevices)
    {
        vk::PhysicalDeviceProperties props = device.getProperties();

        // 检查扩展支持
        if (!CheckDeviceExtensions(device, info.requiredDeviceExtensions))
        {
            spdlog::debug("  [SKIP] {} — missing required extensions", props.deviceName.data());
            continue;
        }

        // 检查 Queue Families
        auto indices = FindQueueFamilies(device, *info.surface);
        if (!indices || !indices->IsComplete())
        {
            spdlog::debug("  [SKIP] {} — incomplete queue families", props.deviceName.data());
            continue;
        }

        // 检查 SwapChain 支持
        auto swapChainDetails = QuerySwapChainSupport(device, *info.surface);
        if (!swapChainDetails)
        {
            spdlog::debug("  [SKIP] {} — no swapchain support", props.deviceName.data());
            continue;
        }

        // 评分
        int score = RatePhysicalDevice(device);
        if (score <= 0)
        {
            spdlog::debug("  [SKIP] {} — score=0 (missing samplerAnisotropy or other)", props.deviceName.data());
            continue;
        }

        spdlog::info("  [{}] {} (score={})", score, props.deviceName.data(), score);

        if (score > bestScore)
        {
            bestScore = score;
            bestDevice = std::move(device);
            bestIndices = *indices;
            bestSwapChainDetails = *swapChainDetails;
        }
    }

    if (!*bestDevice)
    {
        spdlog::critical("No suitable GPU found.");
        return std::nullopt;
    }

    vk::PhysicalDeviceProperties props = bestDevice.getProperties();
    spdlog::info("Selected GPU: {} (type={})",
                 props.deviceName.data(),
                 vk::to_string(props.deviceType));

    result.m_physicalDevice = std::move(bestDevice);
    result.m_queueFamilies = bestIndices;

    // ── 2. 创建逻辑设备 ──
    std::set<uint32_t> uniqueQueueFamilies = {
        *result.m_queueFamilies.graphicsFamily,
        *result.m_queueFamilies.presentFamily,
    };
    if (result.m_queueFamilies.transferFamily.has_value())
        uniqueQueueFamilies.insert(*result.m_queueFamilies.transferFamily);
    if (result.m_queueFamilies.computeFamily.has_value())
        uniqueQueueFamilies.insert(*result.m_queueFamilies.computeFamily);

    std::vector<vk::DeviceQueueCreateInfo> queueCIs;
    float queuePriority = 1.0f;
    for (uint32_t family : uniqueQueueFamilies)
    {
        queueCIs.push_back({
            .queueFamilyIndex = family,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        });
    }

    // ── 物理设备特性 ──
    vk::PhysicalDeviceFeatures enabledFeatures{
        .samplerAnisotropy = VK_TRUE
    };

    // Vulkan 1.1 features: shaderDrawParameters 对应 SPIR-V DrawParameters capability
    vk::PhysicalDeviceVulkan11Features vulkan11Features{
        .shaderDrawParameters = VK_TRUE
    };

    vk::PhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeature{
        .dynamicRendering = VK_TRUE
    };

    vk::DeviceCreateInfo deviceCI{
        .queueCreateInfoCount = static_cast<uint32_t>(queueCIs.size()),
        .pQueueCreateInfos = queueCIs.data(),
        .enabledExtensionCount = static_cast<uint32_t>(info.requiredDeviceExtensions.size()),
        .ppEnabledExtensionNames = info.requiredDeviceExtensions.data(),
        .pEnabledFeatures = &enabledFeatures
    };

    // StructureChain: DeviceCreateInfo → Vulkan11Features → DynamicRenderingFeatures
    auto chain = vk::StructureChain<vk::DeviceCreateInfo,
                                    vk::PhysicalDeviceVulkan11Features,
                                    vk::PhysicalDeviceDynamicRenderingFeaturesKHR>{
        deviceCI,
        vulkan11Features,
        dynamicRenderingFeature
    };

    // Validation layer（逻辑设备层已废弃，但仍提供兼容）
    std::vector<const char*> validationLayers;
    if (info.enableValidation)
    {
        // Vulkan 1.1+ 不再使用 device-level validation layers
        // 但保留兼容性代码路径
        spdlog::debug("Validation enabled (instance-level only for Vulkan 1.1+).");
    }

    {
        auto devResult = result.m_physicalDevice.createDevice(chain.get<vk::DeviceCreateInfo>());
        if (devResult.result != vk::Result::eSuccess)
        {
            spdlog::critical("Failed to create logical device: {}", vk::to_string(devResult.result));
            return std::nullopt;
        }
        result.m_device = std::move(devResult.value);
    }

    // ── 3. 获取 Queue Handles ──
    result.m_graphicsQueue = result.m_device.getQueue(*result.m_queueFamilies.graphicsFamily, 0);
    result.m_presentQueue = result.m_device.getQueue(*result.m_queueFamilies.presentFamily, 0);
    result.m_transferQueue = result.m_device.getQueue(*result.m_queueFamilies.transferFamily, 0);
    result.m_computeQueue = result.m_device.getQueue(*result.m_queueFamilies.computeFamily, 0);

    spdlog::info("Logical device created (Graphics={}, Present={}, Transfer={}, Compute={})",
                 *result.m_queueFamilies.graphicsFamily,
                 *result.m_queueFamilies.presentFamily,
                 *result.m_queueFamilies.transferFamily,
                 *result.m_queueFamilies.computeFamily);

    return result;
}
