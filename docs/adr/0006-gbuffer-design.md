# ADR-0006: GBuffer 设计

> 状态：已确定

## 通道设计

**选择：标准 5 通道**

| Attachment | 格式（桌面） | 格式（移动端 fallback） | 字节/像素 |
|-----------|-------------|----------------------|----------|
| **Albedo** | `R8G8B8A8_UNORM` (sRGB) | 同桌面 | 4 |
| **Normal** | `R16G16B16A16_SFLOAT` | `A2B10G10R10_UNORM_PACK32` | 8 / 4 |
| **Metal/Rough/AO** | `R8G8B8A8_UNORM` (packed) | 同桌面 | 4 |
| **Motion Vector** | `R16G16_SFLOAT` | 同桌面 | 4 |
| **Depth** | `D32_SFLOAT` | 同桌面 | 4 |
| **总计** | | | 24 / 20 |

## Normal 格式双平台自适应

**定量分析：**

- 10-bit 每通道 = 1024 量化台阶，相邻台阶角度差 ≈ `acos(1 - 2/1024)` ≈ 0.06 弧度 ≈ 3.5°
- 16-bit float 有 10-bit 尾数，量化等效 ≈ 0.001 弧度 → **精度差 35 倍**
- 带宽差：1080p 下 16-bit 写入 16MB/帧，10-bit 写入 8MB/帧 → **40%**

**桌面（4060 8GB VRAM）：** 选 16-bit，带宽管够，精度优先  
**移动端：** 选 10-bit，带宽是命，artifacts 可接受

## Motion Vector 嵌入

**选择：嵌入 GBuffer Pass**

- 在 PBR fragment shader 中多输出一个 attachment
- 少一个独立 pass，比独立 Velocity Buffer Pass 省带宽
