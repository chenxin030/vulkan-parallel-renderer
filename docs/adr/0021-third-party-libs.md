# ADR-0021: 第三方库

> 状态：已确定

| 库 | 用途 | 集成方式 |
|----|------|---------|
| **GLFW3** | 窗口/输入 | external/ |
| **GLM** | 数学库 | external/ |
| **VMA** | GPU 内存分配 | external/ 单头文件 |
| **stb_image** | 纹理加载 | 不需要单独引入（tinygltf 自带） |
| **tinygltf** | glTF/GLB 模型加载 | external/ |
| **Dear ImGui** | 调试 UI / 性能面板 | external/ |
| **spdlog** | 日志 | external/ |
| **Slang** | Shader 编译 | 系统安装或 external/ |
