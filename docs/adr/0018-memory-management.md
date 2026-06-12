# ADR-0018: 内存管理

> 状态：已确定

## VMA 使用层次

**选择：L2 — 自定义内存池**

| Pool | 用途 | 生命周期 |
|------|------|---------|
| **Static Geometry** | 顶点/索引缓冲 | 永驻显存 |
| **Dynamic Uniform** | Per-frame UBO | host-visible + device-local |
| **Staging** | IO 线程上传缓冲 | host-visible，瞬态 |
| **Render Target** | GBuffer、Shadow Maps、后处理中间纹理 | 帧内复用 |
