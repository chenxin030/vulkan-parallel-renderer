# ADR-0005: 渲染管线

> 状态：已确定

## PBR 方案

**选择：标准 glTF 2.0 Metallic-Roughness + IBL**

| 方案 | 描述 |
|------|------|
| 最小化 | 只读 Base Color，面试会穿帮 |
| **标准 PBR** ✅ | 完整 Metallic-Roughness workflow + HDR 环境贴图 IBL |
| 扩展 PBR | Clear Coat / Sheen — 过度，核心卖点不是材质 |

**理由：** 游戏公司有自己的成熟材质方案，标准实现即可，不班门弄斧。

## IBL 生成策略

**选择：首次启动时检测 + Compute Shader 生成 + 磁盘缓存**

| 方案 | 描述 |
|------|------|
| 手动预生成 | 用外部工具，硬编码加载 |
| 构建时自动生成 | CMake 自定义 target |
| **首次启动检测** ✅ | 检测 assets/ 是否有派生贴图，没有则 compute shader 生成并缓存 |

**理由：** 类比工业引擎的"首次启动编译着色器"，移动端不增加 compute 负担（磁盘缓存跳过生成）。
