# ADR-0020: C++ 使用规范

> 状态：已确定

## 用这些

| 特性 | 用在哪 | 面试点 |
|------|--------|--------|
| **RAII + Move** | 所有 GPU 资源封装 | "资源生命周期绑定到对象，杜绝泄漏" |
| **`std::optional`** | Device 创建、Extension 检测、Asset 加载 | "明确表达'可能失败'语义，避免异常" |
| **`std::atomic` + memory_order** | ThreadPool 任务队列、Worker 同步 | "release/acquire 语义，lock-free 任务队列" |
| **`std::span`** | Material Bucket 数据传递 | "零拷贝视图，解耦数据源和消费者" |
| **`std::jthread`** | Worker 线程 | "RAII 保证线程安全退出，stop_token 优雅关闭" |
| **`consteval` / `constexpr`** | Descriptor binding index、Shader name hash | "编译期计算，消除运行时查找" |
| **`std::variant`** | 双平台格式选择 | "不同路径同一套代码，不用宏堆条件编译" |

## 不用这些

- **模板元编程黑洞**（SFINAE、CRTP）— 过度设计
- **异常**（`try/catch/throw`）— 游戏引擎禁止，用 `optional` 替代
- **虚函数继承** — 多态用 `variant` 替代，避免虚函数表开销
