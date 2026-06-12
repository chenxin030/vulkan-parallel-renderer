# ADR-0008: 后处理链

> 状态：已确定

## Pass 顺序

```
GBuffer → [SSAO] → Lighting → [VSM Filter] → [Bloom Chain] → [TAA] → Tone Mapping → Color Grading → Swapchain
```

## 各 Pass 详情

| Pass | 实现方式 | 是否放 Async Compute |
|------|---------|---------------------|
| **SSAO** | Compute Shader | ✅ |
| **VSM Filter** | Compute Shader | ✅ |
| **Bloom** | Compute Shader（降采样+模糊+合成） | ✅ |
| **TAA** | Graphics/Compute | ❌（依赖当前帧所有结果） |
| **Tone Mapping** | Compute/Fullscreen Quad | ❌（管线末尾，无并行空间） |
| **Color Grading** | 3D LUT 采样 | ❌ |

## Color Grading

- 3D LUT 纹理映射 RGB→RGB
- 支持艺术家自定义滤镜/电影调色/风格化 look
- 实现成本低，面试加分

## TAA Motion Vector

**选择：嵌入 GBuffer Pass**

- GBuffer 设计时预留 Motion Vector attachment
- 需要前一帧 `modelViewProjection` 矩阵对比
