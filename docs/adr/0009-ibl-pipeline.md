# ADR-0009: IBL 管线

> 状态：已确定

## 贴图列表

| 贴图 | 生成方式 | 说明 |
|------|---------|------|
| HDR 环境贴图 | 加载 `.hdr` 文件 | 源资产 |
| Irradiance Map | 首次启动 compute shader 生成 + 缓存 | 漫反射卷积 |
| Prefiltered Env Map | 同上 | 不同 roughness 级别的镜面反射卷积 |
| BRDF LUT | 同上（128×128） | GGX 重要性采样积分 |

**BRDF LUT 不单独离线生成** — 128×128 很小，compute shader 生成一次即可，也纳入首次启动检测逻辑。
