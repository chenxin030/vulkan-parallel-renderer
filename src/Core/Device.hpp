#pragma once

#include <optional>
#include <vector>

#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

// ═══════════════════════════════════════════════
// Vulkan 逻辑设备封装
// — 物理设备选择（按类型评分）
// — Queue Family 检测
// — Dynamic Rendering 特性开关
// ═══════════════════════════════════════════════

class Device
{
public:
    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        std::optional<uint32_t> transferFamily;
        std::optional<uint32_t> computeFamily;

        bool IsComplete() const
        {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct CreateInfo
    {
        vk::raii::Instance* instance = nullptr;    // non-owning
        vk::raii::SurfaceKHR* surface = nullptr;   // non-owning
        bool enableValidation = true;
        std::vector<const char*> requiredDeviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        };
    };

    static std::optional<Device> Create(const CreateInfo& info);

    Device(Device&&) noexcept = default;
    Device& operator=(Device&&) noexcept = default;
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    // ── 访问器 ──
    vk::raii::Device& Get() { return m_device; }
    const vk::raii::Device& Get() const { return m_device; }
    VkDevice GetRaw() const { return *m_device; }

    vk::raii::PhysicalDevice& GetPhysicalDevice() { return m_physicalDevice; }
    const vk::raii::PhysicalDevice& GetPhysicalDevice() const { return m_physicalDevice; }
    VkPhysicalDevice GetRawPhysicalDevice() const { return *m_physicalDevice; }

    const QueueFamilyIndices& GetQueueFamilies() const { return m_queueFamilies; }

    vk::Queue GetGraphicsQueue() const { return *m_graphicsQueue; }
    vk::Queue GetPresentQueue() const { return *m_presentQueue; }
    vk::Queue GetTransferQueue() const { return *m_transferQueue; }
    vk::Queue GetComputeQueue() const { return *m_computeQueue; }

    uint32_t GetGraphicsFamily() const { return *m_queueFamilies.graphicsFamily; }
    uint32_t GetPresentFamily() const { return *m_queueFamilies.presentFamily; }

    auto GetDispatcher() const { return m_device.getDispatcher(); }

private:
    Device() = default;

    // 物理设备评分 — 越高越好
    static int RatePhysicalDevice(const vk::raii::PhysicalDevice& device);

    // 查询 Queue Families
    static std::optional<QueueFamilyIndices> FindQueueFamilies(
        const vk::raii::PhysicalDevice& physicalDevice,
        const vk::raii::SurfaceKHR& surface);

    // 检查设备扩展支持
    static bool CheckDeviceExtensions(
        const vk::raii::PhysicalDevice& physicalDevice,
        const std::vector<const char*>& requiredExtensions);

    // 查询 SwapChain 支持详情
    struct SwapChainSupportDetails
    {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };
    static std::optional<SwapChainSupportDetails> QuerySwapChainSupport(
        const vk::raii::PhysicalDevice& physicalDevice,
        const vk::raii::SurfaceKHR& surface);

    vk::raii::PhysicalDevice m_physicalDevice{nullptr};
    vk::raii::Device m_device{nullptr};

    QueueFamilyIndices m_queueFamilies;

    // Queue handles — vk::raii::Queue (device owns them)
    vk::raii::Queue m_graphicsQueue{nullptr};
    vk::raii::Queue m_presentQueue{nullptr};
    vk::raii::Queue m_transferQueue{nullptr};
    vk::raii::Queue m_computeQueue{nullptr};
};
