# ADR-0003: 核心卖点与深度方向

> 状态：已确定

## 深度展开（2 个主菜）

- **A: 并行命令录制** — Per-Material Bucket 分发 + P-core 自适应线程池
- **C: Async Compute** — 跨 graphics/compute 队列并行调度 + Timeline Semaphore

## 简要提及（2 个配菜）

- **B: Frame Pipelining** — 2 帧 in-flight
- **D: 异步资源加载** — 独立 IO 线程 + Transfer Queue

## 理由

- 并行录制和 Async Compute 最能体现 Vulkan 多队列/多线程优势
- 两者都能量化测量（Tracy + Nsight Graphics），有数据支撑面试
- 2 个主菜深度足够，不需要面面俱到
