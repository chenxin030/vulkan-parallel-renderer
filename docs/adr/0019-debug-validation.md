# ADR-0019: 调试与验证

> 状态：已确定

## Validation Feature 选择

| Feature | 开启 | 理由 |
|---------|------|------|
| **Debug Utils 命名** | ✅ Release+Debug | 多线程+多 queue 下无命名=盲调 |
| **Sync Validation** | ✅ Release+Debug | Timeline Semaphore 依赖错误不会 crash，最难查 |
| **GPU-Assisted** | ⚠️ Debug 开 | 检测 indirect draw out-of-bounds |
| **Best Practices** | ⚠️ 跑一次 | 移动端兼容性检查，不需常驻 |
| **自定义回调** | ✅ | 路由到 spdlog，带线程 ID 和时间戳 |

## 对象命名策略

所有 Vulkan 对象 `vkSetDebugUtilsObjectNameEXT`：

- Queue: `"GraphicsQueue"`, `"AsyncComputeQueue"`, `"TransferQueue"`
- CommandBuffer: `"Bucket_MetallicPBR_Worker3"`, `"Bucket_UI_Worker5"`
- Semaphore: `"Timeline_Main"`, `"Sem_CullingReady"`
- Image: `"GBuffer_Albedo"`, `"ShadowMap_Cascade2"`
