# ADR-0024: 项目阶段计划

> 状态：进行中

## 第一阶段：骨架跑通

1. CMakeLists.txt — 集成所有第三方库
2. 最小 Vulkan 启动：Instance → Device（1.1 + 扩展检测）→ Surface → SwapChain → 清屏 → Present
3. Dynamic Rendering 画第一个三角形
4. Frame Pipelining（B）：2 in-flight frame 结构
5. 集成 spdlog + Tracy + Debug Utils 命名

## 第二阶段：渲染管线

6. GBuffer 5 通道 + 双平台格式自适应
7. PBR Metallic-Roughness Pipeline + glTF 模型加载
8. IBL（HDR→Irradiance/Prefiltered/LUT → 首次启动生成 + 缓存）
9. CSM 4 级 + PSSM + VSM + 范围截断

## 第三阶段：后处理 + 多线程

10. SSAO → Bloom → TAA → Tone Mapping → Color Grading
11. GPU Culling（Frustum + Occlusion + Multi-Indirect Buffer）
12. Async Compute（C）：Timeline Semaphore + 跨队列调度
13. 并行录制（A）：ThreadPool + P-core 检测 + Material Bucket 分发
14. 异步加载（D）：IO 线程 + Transfer Queue
15. **量化测试**：Nsight + Tracy + 产出 1→4→6→8 worker scaling curve + GPU overlap 数据

## 第四阶段：打磨

16. ImGui 调试面板 + 实时性能监控
17. 双平台 fallback 路径完善与测试
18. 写记录文档 + 性能数据整理
19. 简历项目描述撰写 + 面试准备
