# ADR-0002: 硬件环境

> 状态：已记录

## 开发机配置

| 项目 | 配置 |
|------|------|
| **CPU** | 12th Gen Intel Core i7-12650H |
| **核心** | 6 P-core + 4 E-core = 10 物理核，16 逻辑线程 |
| **GPU** | NVIDIA GeForce RTX 4060 Laptop (8GB VRAM) |
| **OS** | Windows 11 |

## 影响的设计决策

- 桌面路径以 RTX 4060 为基准（支持所有 Vulkan 1.3 扩展）
- 多线程设计基于混合架构（P-core + E-core）：P-core 检测、worker 上限 6–8
- 移动端 fallback 路径仅做代码兼容设计，不做性能优化（无移动端设备可测）
