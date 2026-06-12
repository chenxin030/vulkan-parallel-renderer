# ADR-0017: Shader 系统

> 状态：已确定

## 语言

**选择：Slang**

- Module system + Interface-based dispatch（类 C++ 的 shader 代码组织）
- 比 GLSL 更接近现代引擎实践
- 和 Vulkan 贴合度好

## 编译

**选择：CMake 预编译（release）+ Runtime 热重载（debug）**

| 模式 | 做法 |
|------|------|
| Release | `add_custom_command` 调用 `slangc`，输出 SPIR-V binary 到构建目录 |
| Debug | 引擎启动时调用 Slang API 实时编译 `.slang` 文件，支持热重载 |

## SPIR-V 输出

**选择：纯 SPIR-V binary（不含 embedded reflection）**

## Descriptor Layout 生成

**选择：Slang Reflection API**

| 方案 | 评估 |
|------|------|
| 手动硬编码 | C++ 端和 shader 间不同步风险 |
| **Slang Reflection** ✅ | `slang::IComponentType::getLayout()` 自动提取 binding range/type/index |
| SPIRV-Cross | 多一层工具链 |

**面试说法：** "利用 Slang Reflection API 自动生成 Descriptor Layout，避免了手写 VkDescriptorSetLayout 和 shader 代码不同步的问题。"
