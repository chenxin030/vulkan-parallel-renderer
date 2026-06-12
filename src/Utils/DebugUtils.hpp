#pragma once

#include <cstring>
#include <string>

#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

// ═══════════════════════════════════════════════
// Debug Utils — 为 Vulkan 对象设置调试名
// ═══════════════════════════════════════════════

namespace DebugUtils
{
    // ── Helpers to extract raw C handle as uint64_t ──
    namespace detail
    {
        template<typename T>
        inline uint64_t HandleToU64(T vkHandle)
        {
            // All Vulkan handles are 64-bit on 64-bit systems
            // Use memcpy to avoid strict aliasing and incomplete type issues
            uint64_t result = 0;
            memcpy(&result, &vkHandle, sizeof(vkHandle));
            return result;
        }
    }

    // ── 通用模板：vk::raii 包装类型 ──
    template<typename RaiiType,
             typename = decltype(std::declval<const RaiiType&>().operator*())>
    void SetName(const vk::raii::Device& device, const RaiiType& obj, const char* name)
    {
        using CType = typename RaiiType::CType;
        static_assert(sizeof(CType) <= sizeof(uint64_t), "Vulkan handle must fit in uint64_t");

        vk::DebugUtilsObjectNameInfoEXT nameInfo{
            .objectType = RaiiType::objectType,
            .objectHandle = detail::HandleToU64((CType)(*obj)),
            .pObjectName = name
        };
        try { device.setDebugUtilsObjectNameEXT(nameInfo); } catch (...) {}
    }

    // ── 重载：裸 C handle（用于 swapchain images 等） ──
    inline void SetName(const vk::raii::Device& device, VkImage image, const char* name)
    {
        vk::DebugUtilsObjectNameInfoEXT nameInfo{
            .objectType = vk::ObjectType::eImage,
            .objectHandle = detail::HandleToU64(image),
            .pObjectName = name
        };
        try { device.setDebugUtilsObjectNameEXT(nameInfo); } catch (...) {}
    }

    inline void SetName(const vk::raii::Device& device, VkImageView imageView, const char* name)
    {
        vk::DebugUtilsObjectNameInfoEXT nameInfo{
            .objectType = vk::ObjectType::eImageView,
            .objectHandle = detail::HandleToU64(imageView),
            .pObjectName = name
        };
        try { device.setDebugUtilsObjectNameEXT(nameInfo); } catch (...) {}
    }

    // ── 字符串格式化辅助 ──
    inline std::string FrameName(int frameIndex, const char* resource)
    {
        return "Frame" + std::to_string(frameIndex) + "_" + resource;
    }

    inline std::string SwapChainImageName(int index)
    {
        return "SwapChainImage_" + std::to_string(index);
    }

    inline std::string SwapChainImageViewName(int index)
    {
        return "SwapChainImageView_" + std::to_string(index);
    }
}
