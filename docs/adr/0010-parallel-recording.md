# ADR-0010: 多线程并行录制（A 方案 — 深度）

> 状态：已确定

## 线程模型

**选择：Per-Material Bucket 分发**

| 方案 | 描述 | 评估 |
|------|------|------|
| Per-View 分解 | 每个 shadow cascade + 主视图各一线程 | scaling 差 |
| Per-Draw 分发 | 每个 draw call 发线程，需要 sort | 有争用 |
| **Per-Material Bucket** ✅ | 按材质/管线 sort 后分桶，每个线程录一个桶 | GPU 利用率好，减少 pipeline 切换 |
| Task-Graph 驱动 | DAG 调度器 | 太深，不做 |

## Worker 数量

**选择：8 worker 上限，实测对比 1→4→6→8**

i7-12650H 的混合架构效应：
- P-core 6 个物理核，E-core 4 个
- 预期甜点在 6 worker，8 worker 可能因 E-core 低频 + 超线程争用出现负优化
- **面试可讲：** "实测发现超过 6 worker 后负优化，追踪发现是 E-core 低频 + cache contention"

## P-core 检测

**选择：自动检测 P-core 数量并设为 worker 上限**

- Windows: `GetLogicalProcessorInformation` → EfficiencyClass 区分
- 不依赖用户手动指定，代码自适应

## 量化目标

| 指标 | 测量工具 | 产出 |
|------|---------|------|
| 1→4→6→8 worker scaling curve | Tracy | 加速比表格 |
| P-core vs E-core 负载分布 | Tracy CPU profiling | 线程调度热力图 |
| 录制耗时绝对值 | Vulkan Timestamp Query + Tracy | ms 级对比 |
