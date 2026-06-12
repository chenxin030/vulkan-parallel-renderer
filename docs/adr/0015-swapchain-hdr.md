# ADR-0015: SwapChain 与 HDR

> 状态：已确定

## Present Mode

**选择：桌面 MAILBOX / 移动端 FIFO**

| Mode | 桌面 | 移动端 |
|------|------|--------|
| **MAILBOX** ✅ | 主流，三缓冲无撕裂 | 极少支持（省电策略） |
| **FIFO** ✅ | 最兼容 | 唯一强制支持的模式 |
| IMMEDIATE | 需测帧率 | 基本不支持 |

## Surface Format

**选择：桌面 HDR / 移动端 SDR 双路径**

| 平台 | Format | Color Space |
|------|--------|------------|
| 桌面 | `R16G16B16A16_SFLOAT` | `HDR10_ST2084_EXT` |
| 移动端 | `R8G8B8A8_SRGB` | `SRGB_NONLINEAR_KHR` |

## Tone Mapping 双路径

| 平台 | 作用 | 示例算法 |
|------|------|---------|
| 桌面 HDR | HDR→HDR 重映射（亮度重分布） | 自定义 |
| 移动端 SDR | HDR→SDR 压缩 | ACES / Reinhard |
