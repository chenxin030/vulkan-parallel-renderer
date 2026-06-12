# ADR-0011: Async Compute（C 方案 — 深度）

> 状态：已确定

## 放在 Async Queue 的 Pass

| Pass | 理由 |
|------|------|
| **GPU Culling** | Frustum + Occlusion Culling，纯 compute，独立于 graphics |
| **SSAO** | 输入 GBuffer Normal+Depth，不依赖当前帧 lighting |
| **VSM Filter** | 输入 CSM shadow map（来自上一帧或同帧 CSM 后），可并行 |
| **Bloom Chain** | 需要 lighting 结果，不能和 GBuffer parallel，但可以和下一帧的 shadow pass 并行 |

## 跨队列同步

**选择：Timeline Semaphore**

| 方案 | 描述 | 评估 |
|------|------|------|
| 手动 Binary Semaphore | 每个依赖手写 signal/wait | 容易漏依赖，调试地狱 |
| **Timeline Semaphore** ✅ | `VK_KHR_timeline_semaphore`，uint64 计数器 | 单对象管理多依赖，1.1 设备广泛支持 |
| Barrier Manager | 迷你 task graph 自动生成 | 过度设计 |

## 量化目标

| 指标 | 测量工具 | 产出 |
|------|---------|------|
| Graphics/Compute overlap 比例 | Nsight Graphics GPU Trace | Timeline 截图 + overlap % |
| GPU idle 率对比（开/关 async） | Nsight Graphics | % 差值 |
| 各 compute pass 的 GPU 耗时 | Vulkan Timestamp Query | ms 表格 |
