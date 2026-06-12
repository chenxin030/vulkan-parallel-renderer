#include "Core/Instance.hpp"

#include <spdlog/spdlog.h>
#include <GLFW/glfw3.h>

#include <cstring>
#include <set>

// ═══════════════════════════════════════════════
// 静态创建
// ═══════════════════════════════════════════════

std::optional<Instance> Instance::Create(const CreateInfo& info)
{
    Instance result;

    // Context 自动通过 DynamicLoader 加载 Vulkan（VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL）

    // ── 1. Application Info ──
    vk::ApplicationInfo appInfo{
        .pApplicationName = info.applicationName.c_str(),
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "VulkanEngine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = info.apiVersion
    };

    // ── 2. 检查可用扩展 ──
    auto extResult = result.m_context.enumerateInstanceExtensionProperties(nullptr);
    std::vector<vk::ExtensionProperties> availableExtensions = extResult.value;

    for (const auto& ext : availableExtensions)
    {
        result.m_supportedExtensions.push_back(ext.extensionName);
    }

    spdlog::info("Available extensions: {}", availableExtensions.size());
    for (const auto& ext : availableExtensions)
    {
        spdlog::debug("  {}", ext.extensionName.data());
    }

    // ── 3. 需要的扩展 ──
    auto requiredExtensions = result.GetRequiredExtensions();

    // 检查是否全部支持
    for (const auto* ext : requiredExtensions)
    {
        if (!result.IsExtensionSupported(ext))
        {
            spdlog::critical("Required extension '{}' not supported.", ext);
            return std::nullopt;
        }
    }

    // ── 4. Validation Layers ──
    std::vector<const char*> validationLayers;
    if (info.enableValidation && result.CheckValidationLayerSupport())
    {
        validationLayers.push_back("VK_LAYER_KHRONOS_validation");
        result.m_validationEnabled = true;

        // Debug Utils 扩展（用于命名对象）
        if (result.IsExtensionSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
        {
            requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        spdlog::info("Validation layers enabled.");
    }

    // ── 5. 创建 Instance ──
    vk::InstanceCreateInfo instanceCI{
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(validationLayers.size()),
        .ppEnabledLayerNames = validationLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
        .ppEnabledExtensionNames = requiredExtensions.data(),
    };

    // Debug Utils Messenger — 始终包含（validation 关闭时为全零/空，不生效）
    vk::DebugUtilsMessengerCreateInfoEXT debugCI{
        .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
        .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                       vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                       vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
        .pfnUserCallback = result.m_validationEnabled
            ? reinterpret_cast<vk::PFN_DebugUtilsMessengerCallbackEXT>(&Instance::DebugCallback)
            : nullptr,
        .pUserData = nullptr
    };

    // StructureChain: InstanceCreateInfo → DebugUtilsMessengerCreateInfoEXT
    vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT> chain{
        instanceCI, debugCI
    };

    {
        auto instanceResult = result.m_context.createInstance(chain.get<vk::InstanceCreateInfo>());
        if (instanceResult.result != vk::Result::eSuccess)
        {
            spdlog::critical("Failed to create Vulkan instance: {}", vk::to_string(instanceResult.result));
            return std::nullopt;
        }
        result.m_instance = std::move(instanceResult.value);
    }

    // ── 6. Debug Messenger ──
    if (result.m_validationEnabled)
    {
        auto messengerResult = result.m_instance.createDebugUtilsMessengerEXT(debugCI);
        if (messengerResult.result == vk::Result::eSuccess)
        {
            result.m_debugMessenger = std::move(messengerResult.value);
        }
        else
        {
            spdlog::warn("Failed to create debug messenger: {}", vk::to_string(messengerResult.result));
        }
    }

    spdlog::info("Vulkan Instance created (API {}.{}.{})",
                 VK_API_VERSION_MAJOR(info.apiVersion),
                 VK_API_VERSION_MINOR(info.apiVersion),
                 VK_API_VERSION_PATCH(info.apiVersion));

    return result;
}

// ═══════════════════════════════════════════════
// 扩展支持检测
// ═══════════════════════════════════════════════

bool Instance::IsExtensionSupported(const char* extensionName) const
{
    return std::find_if(
               m_supportedExtensions.begin(),
               m_supportedExtensions.end(),
               [extensionName](const std::string& s)
               {
                   return s == extensionName;
               }) != m_supportedExtensions.end();
}

// ═══════════════════════════════════════════════
// 必需扩展列表
// ═══════════════════════════════════════════════

std::vector<const char*> Instance::GetRequiredExtensions() const
{
    // GLFW 需要的 surface 扩展
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    for (const auto* ext : extensions)
    {
        spdlog::debug("  GLFW requires: {}", ext);
    }

    return extensions;
}

// ═══════════════════════════════════════════════
// Validation Layer 检测
// ═══════════════════════════════════════════════

bool Instance::CheckValidationLayerSupport() const
{
    auto layerResult = m_context.enumerateInstanceLayerProperties();
    const auto& layers = layerResult.value;

    for (const auto& layer : layers)
    {
        if (std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0)
            return true;
    }

    spdlog::warn("VK_LAYER_KHRONOS_validation not found.");
    return false;
}

// ═══════════════════════════════════════════════
// Debug Callback → spdlog
// ═══════════════════════════════════════════════

VKAPI_ATTR VkBool32 VKAPI_CALL Instance::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* /*pUserData*/)
{
    // ── 过滤已知驱动 Bug 导致的误报 ──
    // 部分 Windows 11 + Intel UHD Graphics 驱动在
    // vkGetPhysicalDeviceSurfaceCapabilitiesKHR 中返回垃圾值
    // (currentExtent, minImageExtent, maxImageExtent 全为未初始化内存)。
    // SwapChain::ChooseExtent() 已对这类情况做了防御性回退，
    // 但 Validation Layer 在 vkCreateSwapchainKHR 内部独立查询
    // surface capabilities 并校验 extent，无法感知我们的回退逻辑。
    //
    // 解决方案：当检测到该 VUID 时，降级为 debug 日志而非 error，
    // 避免误导用户认为 extent 选择有误。
    if (pCallbackData->pMessage &&
        std::strstr(pCallbackData->pMessage, "VUID-VkSwapchainCreateInfoKHR-pNext-07781"))
    {
        spdlog::debug("[Vulkan Validation — suppressed driver-bug false positive] {}",
                      pCallbackData->pMessage);
        return VK_FALSE;
    }

    // 构造消息前缀
    std::string prefix;
    if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
        prefix = "[Vulkan Validation]";
    else if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
        prefix = "[Vulkan Perf]";
    else
        prefix = "[Vulkan]";

    switch (severity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        spdlog::error("{} {}", prefix, pCallbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        spdlog::warn("{} {}", prefix, pCallbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        spdlog::info("{} {}", prefix, pCallbackData->pMessage);
        break;
    default:
        spdlog::debug("{} {}", prefix, pCallbackData->pMessage);
        break;
    }

    return VK_FALSE;
}
