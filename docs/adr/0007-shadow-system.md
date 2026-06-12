# ADR-0007: 阴影系统

> 状态：已确定

## 选择：4 级 PSSM CSM + VSM + 范围截断

| 决策点 | 选择 | 理由 |
|--------|------|------|
| **级联数** | 4 级 | 开放场景主流选择 |
| **Split 方式** | PSSM（λ 混合） | 近处质量和远处稳定性之间的可调平衡 |
| **Filtering** | VSM（方差阴影贴图） | 支持 separable blur，比 PCF 大 kernel 更高效 |
| **Light Bleeding** | 范围截断 | 性价比最高，面试可讲"知道 Moment Shadow Map 是更根本解法" |
| **分辨率** | 1024×1024 per cascade | 贴合主流方向，不过度 |
