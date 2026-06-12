# PRD: Phase 2 — PBR 渲染管线

> 标签：ready-for-agent | 状态：待实现

## Problem Statement

Phase 1 已经实现了 Vulkan 最小启动骨架——一个窗口 + 一个硬编码的三角形。但这个三角形没有任何材质光照信息，距离一个"能展示 Vulkan 能力的渲染器"还有巨大差距。游戏行业面试官期望看到一个完整的 PBR 渲染管线：glTF 模型加载、物理材质、环境光照、阴影系统。缺少这些，整个项目就只是一个"Hello Triangle"的复刻，无法证明引擎架构能力。

## Solution

在 Phase 1 骨架上搭建完整的 Deferred PBR 渲染管线，分四个子系统逐步实现：

1. **GBuffer**: 5 通道 Deferred Rendering，动态选择桌面/移动端格式
2. **glTF PBR**: 完整的 Metallic-Roughness 工作流 + 模型加载
3. **IBL**: HDR 环境贴图 → Irradiance/Prefiltered/BRDF LUT 自动生成
4. **CSM 阴影**: 4 级 Cascade Shadow Maps + VSM 滤波

每个子系统的输出都是可独立验证的一个渲染画面或可检查的中间纹理。

## User Stories

### GBuffer 子系统
1. 作为引擎开发者，我希望所有几何体在一次 GBuffer Pass 中写入 5 个 MRT attachment（Albedo/Normal/MetalRoughAO/MotionVector/Depth），以便后续光照 Pass 从中采样
2. 作为引擎开发者，我希望 GBuffer 的 Normal 格式在桌面端使用 R16G16B16A16_SFLOAT，移动端自动降级为 A2B10G10R10_UNORM_PACK32，以在精度和带宽之间取得平衡
3. 作为引擎开发者，我希望 Motion Vector 作为 GBuffer 的一个额外 attachment 直接输出，而非独立 Pass，以减少带宽开销

### glTF 模型加载子系统
4. 作为引擎开发者，我希望通过 tinygltf 加载 glTF 2.0 文件（.gltf/.glb），解析出 Mesh/Primitive/Node 层级结构，以便在引擎中渲染任意模型
5. 作为引擎开发者，我希望从 glTF 材质中提取 Metallic-Roughness PBR 参数（baseColorFactor/metallicFactor/roughnessFactor 等），自动生成 Material 数据结构
6. 作为引擎开发者，我希望在加载模型时同步加载关联的纹理贴图（KTX2/PNG/JPG），并通过 VMA 管理 GPU 端的 Image 和 ImageView
7. 作为引擎开发者，我希望看到 Camera 控制器（Orbit Camera），以便旋转/缩放/平移视角来观察渲染结果

### PBR 渲染子系统
8. 作为引擎开发者，我希望有一个独立的 PBR Lighting Pass，从 GBuffer 采样几何数据 + IBL 环境贴图，计算最终的 Fragment Shaded Color
9. 作为引擎开发者，我希望 Shader 使用 Slang 的 Module/Interface 系统组织（如 IMaterial interface），避免一个巨大的 monolithic shader
10. 作为引擎开发者，我希望 Descriptor Layout 由 Slang Reflection API 自动生成，而非手动硬编码，以保证 C++ 端和 Shader 端绑定一致

### IBL 子系统
11. 作为引擎开发者，我希望引擎启动时检测 assets/textures/ 下是否存在 IBL 派生贴图（irradiance/prefiltered/brdf_lut），若不存在则自动用 Compute Shader 生成并缓存到磁盘
12. 作为引擎开发者，我希望加载 HDR 环境贴图（.hdr 格式）作为 IBL 的源输入
13. 作为引擎开发者，我希望 BRDF LUT（128×128）也纳入首次启动自动生成流程，不依赖外部离线工具

### CSM 阴影子系统
14. 作为引擎开发者，我希望 4 级 Cascade Shadow Maps 根据 PSSM λ-mix 算法计算每级的 split distance
15. 作为引擎开发者，我希望每个 cascade 渲染到 1024×1024 的深度贴图（VSM 格式，存储 depth 和 depth²），以支持后续 separable blur
16. 作为引擎开发者，希望在 PBR Lighting Pass 中采样 shadow map，对光照结果应用阴影，并使用范围截断减少 light bleeding

### 工程基础设施
17. 作为引擎开发者，我希望 CMake target Shaders 自动编译所有 .slang 文件为 SPIR-V，按 _vert/_frag/_comp 后缀推断 stage
18. 作为引擎开发者，我希望 Debug 构建支持 Shader 热重载（Slang API 实时编译）

## Implementation Decisions

### 新增模块（与 ADR-0022 源码组织对齐）

- `src/Scene/` — Camera（Orbit Camera 控制器）、Transform、Model/Geometry（tinygltf 封装）、SceneGraph
- `src/Renderer/GBuffer/` — 5 通道 MRT 管理、双平台格式自适应
- `src/Renderer/PBRPipeline/` — Deferred PBR Lighting Pass、Material 数据结构
- `src/Renderer/Shadow/` — CSM 4 级、VSM 滤波、范围截断
- `src/Shaders/` — Slang 编译管理（CMake 预编译 + Debug 热重载路径）、Descriptor Layout 反射生成
- `src/Renderer/IBL/` — HDR 加载、Irradiance/Prefiltered/BRDF LUT 生成

### 修改现有模块

- `Engine.cpp` — DrawFrame 重构为多 Pass（GBuffer → Shadow → PBR Lighting），管理新子系统生命周期
- `CMakeLists.txt` — 新增 src/Scene/ src/Renderer/ 路径到 include 和 source glob

### 架构决策（继承自 ADR）

- Dynamic Rendering（无 VkRenderPass）贯穿 Phase 2 所有 Pass
- Dynamic State 全开（Viewport/Scissor/CullMode/DepthTestEnable）
- Dynamic Vertex Input 桌面路径 — 不同顶点布局共用 Pipeline
- vk::raii 管长生命周期对象，裸 C handle 管短生命周期
- std::optional 代替异常，std::variant 在 Material 类型判别时首次使用

### Pipeline 数量估算

| Pass | Pipeline 数 | 说明 |
|------|------------|------|
| GBuffer | 1 | Dynamic Vertex Input 使所有几何体共用一个 pipeline |
| Shadow (CSM) | 1 | 4 级 cascade 共用同一 pipeline，只改 viewport |
| PBR Lighting | 1 (fullscreen quad) | 采样 GBuffer + IBL + Shadow Map |
| Skybox | 1 | HDR 环境贴图背景渲染 |
| IBL Generation | 3 | Irradiance convolution / Prefilter / BRDF LUT（Compute） |

### 关键接口约定

- `Model::Load(path)` → 返回 Scene 树（Node + Mesh + Material）
- `GBuffer::Render(scene, camera)` → 写入 5 attachment
- `PBRPipeline::Light(gbuffer, ibl, shadow, camera)` → 输出 final lit color
- `IBLManager::EnsureGenerated(device, hdrPath)` → 检测缓存，必要时生成
- `ShadowRenderer::RenderCascades(scene, lightDir, camera)` → 输出 4 级 shadow map array

## Testing Decisions

### 测试 Seam（由高到低）

1. **RenderDoc 手动检查（最高实用 seam）**：每个 Pass 完成后，用 RenderDoc 抓帧检查各 attachment 内容——Albedo 是否 sRGB 正确、Normal 是否在 [-1,1] 范围内、Depth 是否正确。这是 Vulkan 引擎开发的标准验证手段。

2. **参考模型视觉对比**：使用 Khronos 官方的 DamagedHelmet.glb / Sponza.glb 作为测试模型，对比渲染截图与 glTF Sample Viewer 的渲染结果（允许色彩空间微小差异）。

3. **IBL 输出验证**：用已知 HDR 作为输入，验证生成的 irradiance/prefiltered 贴图在每个 mip level 的亮度与参考值一致（允许 5% 误差）。

4. **glTF 加载数据测试**：加载已知模型，验证顶点数、材质参数、纹理路径与预期值精确匹配。纯 CPU 测试，可自动化。

### 什么不算好测试

- 测试 Pipeline Layout 的具体 binding index（会随 shader 变化而频繁更新）
- 测试 CommandBuffer 录制顺序（内部实现细节）
- 测试 VMA 分配后的具体 GPU 地址（无意义）

### 自动化测试范围

Phase 2 以手动验证为主（RenderDoc + 视觉对比），仅 Model::Load() 的 CPU 端数据解析部分适合写自动化测试。完整的自动化渲染测试留到 Phase 3 引入 CI 参考截图系统后实现。

## Out of Scope

- Async Compute / 并行录制 — Phase 3 核心卖点
- 后处理链（SSAO/Bloom/TAA/ToneMap）— Phase 3
- GPU Culling — Phase 3
- ImGui 调试面板 — Phase 4
- 异步资源加载（IO 线程 + Transfer Queue）— Phase 3，Phase 2 同步加载即可
- 多光源 PBR — Phase 2 仅需方向光 + IBL 环境光
- Skinning / Animation — 超出项目定位
- Clear Coat / Sheen / Transmission — 不属于标准 PBR
- 移动端实机测试 — Phase 2 在 RTX 4060 桌面端验证

## Further Notes

- 整个 Phase 2 预计新增 ~10 个 .hpp/.cpp 文件 + ~8 个 .slang shader 文件
- GBuffer 的 5 通道格式选择（ADR-0006）已做定量分析（精度差 35 倍，带宽差 40%），面试时可作为 trade-off 案例讲
- Slang Reflection API 的 Descriptor Layout 自动生成是面试加分点——避免了大多数 Vulkan 教程项目的"手写 binding 硬编码"痛点
- IBL 的"首次启动生成+缓存"策略可类比工业引擎的"首次启动编译着色器"
- VSM 的范围截断是故意选择次优解——面试时主动指出"Moment Shadow Map 是更根本的解法"展示技术判断力
