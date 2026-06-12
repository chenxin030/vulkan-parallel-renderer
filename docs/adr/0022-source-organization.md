# ADR-0022: 源码组织结构

> 状态：已确定

```
VulkanEngine/
├── CMakeLists.txt
├── CONTEXT.md                              ← 领域知识索引
├── assets/                                 # 模型、纹理、HDR 环境贴图
│   ├── models/                             # glTF/GLB 模型文件
│   └── textures/                           # 纹理 + HDR 环境贴图
├── shaders/                                # Slang 着色器源文件
├── external/                               # 第三方库
├── src/
│   ├── Core/
│   │   ├── Instance / Device               # Vulkan 1.1 + 扩展检测
│   │   ├── Window / Surface                # GLFW3
│   │   ├── SwapChain                       # HDR/SDR 双路径
│   │   ├── CommandPool / CommandBuffer
│   │   ├── Synchronization                 # Timeline Semaphore
│   │   ├── MemoryAllocator                 # VMA L2 自定义池
│   │   └── DynamicRendering                # VK_KHR_dynamic_rendering
│   ├── Renderer/
│   │   ├── GBuffer                         # 5 通道，双平台格式
│   │   ├── PBRPipeline                     # Metallic-Roughness + IBL
│   │   ├── Shadow / CSM                    # PSSM 4 级 + VSM
│   │   ├── PostProcess                     # Bloom/SSAO/TAA/ToneMap/ColorGrading
│   │   ├── AsyncCompute                    # 跨队列调度 + Timeline Semaphore
│   │   └── GPUCulling                      # GPU-Driven Frustum+Occlusion + Multi-Indirect
│   ├── Scene/
│   │   ├── Camera / Transform
│   │   ├── Model / Geometry                # glTF via tinygltf
│   │   └── SceneGraph
│   ├── Threading/
│   │   ├── ThreadPool                      # jthread + atomic + lock-free queue
│   │   ├── CoreDetection                   # P-core 自动检测
│   │   └── MaterialBucketRecorder          # Per-Material Bucket 分发
│   ├── Shaders/                            # Slang 编译管理 + Reflection
│   ├── IO/
│   │   └── AsyncLoader                     # 独立 IO 线程 + Transfer Queue
│   └── Utils/
│       ├── Profiler                        # GPU Timestamps + Tracy 集成
│       ├── ImGuiIntegration
│       └── Log                             # spdlog 封装
└── build/                                  # 构建产物
```
