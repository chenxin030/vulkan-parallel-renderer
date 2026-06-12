# ADR-0004: Vulkan 基础架构

> 状态：已确定

## Vulkan 版本

| 方案 | 描述 | 取舍 |
|------|------|------|
| Vulkan 1.3 | 桌面全覆盖，移动端几乎不支持 | 面试可能被问"能在骁龙 870 上跑吗" |
| **Vulkan 1.1 + 扩展** ✅ | 桌面 + 移动端全覆盖 | 需动态检测扩展支持，可讲兼容性设计 |

## API 风格

| 方案 | 描述 |
|------|------|
| 纯 C API | 和 Vulkan Spec 一一对应，手动管理生命周期 |
| 纯 vk::raii | RAII 自动释放，C++17 |
| **混用** ✅ | 核心长生命周期对象用 raii，短生命周期/复杂 allocator 用手动 |

## Frame Pipelining

**选择：2 帧 in-flight**

- 同步模型更简单，调试友好
- 每个 in-flight frame 持有独立的 per-frame 资源池（command pool、descriptor pool、uniform buffer）

## Dynamic Rendering

**选择：硬性要求 VK_KHR_dynamic_rendering**

- 不需要 VkRenderPass / VkFramebuffer
- Vulkan 1.1 设备广泛支持该扩展
- 简历说法："基于 Dynamic Rendering 的现代 Vulkan 管线，减少 API 对象开销"
