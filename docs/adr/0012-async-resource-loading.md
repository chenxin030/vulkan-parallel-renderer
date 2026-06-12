# ADR-0012: 异步资源加载（D 方案 — 简要）

> 状态：已确定

## IO 线程架构

**选择：独立 IO 线程 + 独立 Transfer Queue + Command Pool**

| 方案 | 描述 |
|------|------|
| IO 只读磁盘，主线程上传 | 安全但卡顿 |
| **独立 Transfer Queue** ✅ | IO 线程独立上传 GPU staging buffer，主线程只"认领" |
| 多 VkDevice | 过于复杂 |

**面试可讲：** "IO 线程独立持有 Transfer Queue，纹理/模型上传不阻塞渲染主线程——利用 Vulkan 的多队列特性。"
