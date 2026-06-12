# ADR-0013: Descriptor 策略

> 状态：已确定

## 核心策略

**选择：Descriptor Buffer（VK_EXT_descriptor_buffer）双路径**

| 路径 | 平台 | 做法 |
|------|------|------|
| 桌面 | RTX 4060 等支持设备 | Descriptor Buffer 直接写入 GPU 可见 buffer 的 descriptor 指针 |
| 移动端 fallback | 不支持扩展的设备 | 传统 Descriptor Set + Per-thread pool |

## 多线程下的 Descriptor 管理

**选择：Per-thread/Bucket 独立管理**

- 桌面路径：每个 worker 独立分配 descriptor buffer 并 bind → 无锁
- 移动端路径：每个 worker 独立 descriptor pool → 无锁（但更耗内存）

## Shader 资源绑定

**选择：部分 Bindless**

- 所有 material 纹理打包进 descriptor array
- Shader 用 `NonUniformResourceIndex` 按 object index 索引
- 减少 bind 次数，和 Per-Material Bucket 分发契合
