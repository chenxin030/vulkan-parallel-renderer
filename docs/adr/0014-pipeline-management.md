# ADR-0014: Pipeline 管理

> 状态：已确定

## Dynamic State

**选择：4 个 Dynamic State 全开**

- `VK_DYNAMIC_STATE_VIEWPORT`
- `VK_DYNAMIC_STATE_SCISSOR`
- `VK_DYNAMIC_STATE_CULL_MODE`
- `VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE`

## Dynamic Vertex Input

**选择：开启 + 双路径**

- 桌面：`VK_EXT_extended_dynamic_state3` → 不同顶点布局共用 pipeline
- 移动端 fallback：静态 vertex input state
- 减少 pipeline 对象数量
