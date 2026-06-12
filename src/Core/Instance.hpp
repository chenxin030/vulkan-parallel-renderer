#pragma once

#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

// ═══════════════════════════════════════════════
// Vulkan Instance 封装
// — Vulkan 1.1 + 扩展检测
// — Validation Layer (Debug 构建)
// — Debug Utils Messenger → spdlog
// ═══════════════════════════════════════════════

class Instance
{
public:
    struct CreateInfo
    {
        std::string applicationName = "VulkanEngine";
        uint32_t apiVersion = VK_API_VERSION_1_1; // 目标 Vulkan 1.1
        bool enableValidation = true;
    };

    static std::optional<Instance> Create(const CreateInfo& info = {});

    Instance(Instance&&) noexcept = default;
    Instance& operator=(Instance&&) noexcept = default;
    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;

    // ── 访问器 ──
    vk::raii::Instance& Get() { return m_instance; }
    const vk::raii::Instance& Get() const { return m_instance; }
    VkInstance GetRaw() const { return *m_instance; }

    // ── 扩展检测 ──
    bool IsExtensionSupported(const char* extensionName) const;

private:
    Instance() = default;

    bool CheckValidationLayerSupport() const;
    std::vector<const char*> GetRequiredExtensions() const;
    VKAPI_ATTR static VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    vk::raii::Context m_context;
    vk::raii::Instance m_instance{nullptr};
    vk::raii::DebugUtilsMessengerEXT m_debugMessenger{nullptr};

    std::vector<std::string> m_supportedExtensions;
    bool m_validationEnabled{false};
};
