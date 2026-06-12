#pragma once

// ═══════════════════════════════════════════════
// Tracy Profiler — conditional macros
// 当 ENABLE_TRACY=ON 且 external/tracy 存在时激活
// 否则所有宏展开为 no-op
// ═══════════════════════════════════════════════

#if defined(TRACY_ENABLE)
    #include <tracy/Tracy.hpp>
    #include <tracy/TracyVulkan.hpp>

    // Convenience macros for VulkanEngine
    #define VK_TRACY_ZONE(name)      ZoneScopedN(name)
    #define VK_TRACY_FRAME_MARK(name) FrameMarkNamed(name)
    #define VK_TRACY_ZONE_SCOPED      ZoneScoped

    // Vulkan context — NULL if not using GPU tracing
    #define VK_TRACY_VK_CONTEXT       nullptr

#else
    // No-op stubs
    #define VK_TRACY_ZONE(name)       ((void)0)
    #define VK_TRACY_FRAME_MARK(name) ((void)0)
    #define VK_TRACY_ZONE_SCOPED      ((void)0)
    #define VK_TRACY_VK_CONTEXT       nullptr
#endif
