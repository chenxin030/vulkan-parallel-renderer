# ADR-0016: GPU Culling

> 状态：已确定

## 选择：GPU-Driven Frustum + Occlusion Culling + Multi-Indirect Buffer

| 方案 | 描述 |
|------|------|
| 单 Pass Indirect | 所有物体同 material，不适合多 material 场景 |
| GPU Culling 同时出桶 | Compute 按 material ID 写不同桶，需要 atomics |
| GPU→CPU 回读→分桶 | 有 read-back 延迟，退化了 GPU-Driven 优势 |
| **Multi-Indirect Buffer** ✅ | 每个 material 预分配 indirect draw buffer，GPU culling 直接写入 |

## 与 Per-Material Bucket 的融合

- GPU culling compute shader 根据 material ID 将 culled instance append 到对应 bucket 的 indirect draw buffer
- Graphics queue 上每个 bucket 一次 `vkCmdDrawIndexedIndirect`
- **CPU 不读取 GPU Culling 结果**，依赖在 GPU 侧：Timeline Semaphore 保证 culling 完成后 graphics 才执行间接绘制
