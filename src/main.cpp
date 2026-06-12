#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "Core/Engine.hpp"

int main()
{
    // ── 日志初始化 ──
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("VulkanEngine.log", true);
    file_sink->set_level(spdlog::level::trace);

    auto logger = std::make_shared<spdlog::logger>("VulkanEngine", spdlog::sinks_init_list{console_sink, file_sink});
    logger->set_level(spdlog::level::trace);
    spdlog::set_default_logger(logger);

    spdlog::info("════════════════════════════════");
    spdlog::info("VulkanEngine starting...");
    spdlog::info("════════════════════════════════");

    // ── 引擎初始化 ──
    auto engine = Engine::Create();
    if (!engine)
    {
        spdlog::critical("Failed to initialize engine.");
        return 1;
    }

    // ── 主循环 ──
    engine->Run();

    // ── 清理 ──
    engine.reset();
    spdlog::info("VulkanEngine shut down cleanly.");
    return 0;
}
