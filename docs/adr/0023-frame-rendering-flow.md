# ADR-0023: 一帧渲染流程

> 状态：已确定

```
Frame N 开始
│
├── [CPU 主线程]
│   ├── 等待上一帧 submit 完成 (Timeline Semaphore)
│   ├── 更新 Scene Graph / Transform
│   ├── 检测 IO 线程完成的异步加载，注册新资源
│   ├── 分发 GPU Culling Dispatch ──→ [Async Compute Queue]
│   ├── Material Bucket 分发 → Worker 线程池开始并行录制
│   └── 主线程等待所有 worker 完成，收集 command buffers
│
├── [CPU Worker 线程] (并行录制，per-material bucket)
│   ├── Worker 0~7: 各录各自 bucket 的 draw calls
│   └── 每个 worker 管理自己的 descriptor buffer/pool
│
├── [IO 线程]
│   ├── 处理异步加载请求队列
│   ├── 读取文件 → 解析 glTF → 创建 staging buffer
│   ├── 通过 Transfer Queue 上传到 device-local
│   └── 完成后标记资源"就绪"
│
├── [GPU Graphics Queue]
│   ├── CSM Pass 0~3 (Shadow Maps)
│   ├── ← wait Timeline Semaphore (GPU Culling 完成)
│   ├── GBuffer Pass (间接绘制 via Multi-Indirect Buffer)
│   ├── ← wait Timeline Semaphore (SSAO 完成)
│   ├── ← wait Timeline Semaphore (VSM Filter 完成)
│   ├── Lighting Pass (全屏 Quad，消费 GBuffer)
│   ├── ← wait Timeline Semaphore (Bloom 完成)
│   ├── TAA
│   ├── Tone Mapping (HDR→HDR 重映射 / HDR→SDR)
│   ├── Color Grading (3D LUT)
│   └── ImGui Overlay
│
├── [GPU Async Compute Queue]
│   ├── GPU Culling (Frustum + Occlusion) → 写入 Multi-Indirect Buffer
│   ├── VSM Filter (4 级 CSM 的方差滤波)
│   ├── SSAO
│   └── Bloom Downsample/Blur/Combine
│
├── [GPU Transfer Queue]
│   └── IO 线程驱动的纹理/几何数据上传
│
└── Present → Frame N+1 开始
```
