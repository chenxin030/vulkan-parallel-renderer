# Vulkan 游戏引擎 — 领域知识索引

> 所有架构决策记录（ADR）拆分至 `docs/adr/` 目录，按编号独立文件管理。

## 项目概要

- **定位**: Vulkan API 能力证明 — 自研渲染引擎
- **硬件**: Intel i7-12650H (6P+4E) + RTX 4060 Laptop 8GB + Windows 11
- **核心深度方向**: 并行命令录制 + Async Compute
- **语言**: C++20，Shader 用 Slang

## ADR 索引

| 编号 | 标题 | 状态 |
|------|------|------|
| [0001](docs/adr/0001-project-positioning.md) | 项目定位 | ✅ 已确定 |
| [0002](docs/adr/0002-hardware-environment.md) | 硬件环境 | ✅ 已记录 |
| [0003](docs/adr/0003-core-selling-points.md) | 核心卖点与深度方向 | ✅ 已确定 |
| [0004](docs/adr/0004-vulkan-foundation.md) | Vulkan 基础架构 | ✅ 已确定 |
| [0005](docs/adr/0005-rendering-pipeline.md) | 渲染管线 | ✅ 已确定 |
| [0006](docs/adr/0006-gbuffer-design.md) | GBuffer 设计 | ✅ 已确定 |
| [0007](docs/adr/0007-shadow-system.md) | 阴影系统 | ✅ 已确定 |
| [0008](docs/adr/0008-post-processing-chain.md) | 后处理链 | ✅ 已确定 |
| [0009](docs/adr/0009-ibl-pipeline.md) | IBL 管线 | ✅ 已确定 |
| [0010](docs/adr/0010-parallel-recording.md) | 多线程并行录制（深度） | ✅ 已确定 |
| [0011](docs/adr/0011-async-compute.md) | Async Compute（深度） | ✅ 已确定 |
| [0012](docs/adr/0012-async-resource-loading.md) | 异步资源加载 | ✅ 已确定 |
| [0013](docs/adr/0013-descriptor-strategy.md) | Descriptor 策略 | ✅ 已确定 |
| [0014](docs/adr/0014-pipeline-management.md) | Pipeline 管理 | ✅ 已确定 |
| [0015](docs/adr/0015-swapchain-hdr.md) | SwapChain 与 HDR | ✅ 已确定 |
| [0016](docs/adr/0016-gpu-culling.md) | GPU Culling | ✅ 已确定 |
| [0017](docs/adr/0017-shader-system.md) | Shader 系统 | ✅ 已确定 |
| [0018](docs/adr/0018-memory-management.md) | 内存管理 | ✅ 已确定 |
| [0019](docs/adr/0019-debug-validation.md) | 调试与验证 | ✅ 已确定 |
| [0020](docs/adr/0020-cpp-conventions.md) | C++ 使用规范 | ✅ 已确定 |
| [0021](docs/adr/0021-third-party-libs.md) | 第三方库 | ✅ 已确定 |
| [0022](docs/adr/0022-source-organization.md) | 源码组织结构 | ✅ 已确定 |
| [0023](docs/adr/0023-frame-rendering-flow.md) | 一帧渲染流程 | ✅ 已确定 |
| [0024](docs/adr/0024-project-phase-plan.md) | 项目阶段计划 | 🔄 进行中 |

## 术语表

- **PBR**: Physically Based Rendering — Metallic-Roughness workflow
- **IBL**: Image-Based Lighting — HDR 环境贴图驱动的间接光照
- **GBuffer**: Geometry Buffer — 延迟渲染的中间几何数据
- **CSM**: Cascaded Shadow Maps — 级联阴影贴图
- **PSSM**: Parallel-Split Shadow Maps — CSM 的 split 策略
- **VSM**: Variance Shadow Maps — 方差阴影贴图，支持 separable blur
- **SSAO**: Screen-Space Ambient Occlusion
- **TAA**: Temporal Anti-Aliasing
- **VMA**: Vulkan Memory Allocator
- **Dynamic Rendering**: `VK_KHR_dynamic_rendering` — 无需 VkRenderPass
- **Timeline Semaphore**: `VK_KHR_timeline_semaphore` — uint64 计数器的跨队列同步
- **Descriptor Buffer**: `VK_EXT_descriptor_buffer` — 直接写入 GPU 可见 descriptor
- **Bindless**: 纹理数组 + `NonUniformResourceIndex` 间接索引

## 未覆盖 / 待决定

- [ ] 场景管理 / Scene Graph 的具体数据结构
- [ ] SSAO 具体算法（HBAO / GTAO / RTAO？）
- [ ] Bloom 的 downscale 级数和 filter 半径
- [ ] ImGui 性能面板展示哪些实时指标
- [ ] 多材质物体的 bucket 归属策略（primary material？）
- [ ] 是否支持透明物体（forward pass after Opaque？）
- [ ] 是否做 GPU Profiling 自动标记（Tracy GPU zone？）
- [ ] 移动端 fallback 的具体触发条件（设备检测阈值）
