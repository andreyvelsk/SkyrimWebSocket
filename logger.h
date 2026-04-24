#pragma once

#include <spdlog/sinks/basic_file_sink.h>

namespace logger = SKSE::log;

// Initialises file-based logging at the requested level.
// Pass spdlog::level::off (the default) to suppress all log output — no file is created.
// For debug/trace the logger flushes on every write so the log is always
// up-to-date on disk, which is critical when trying to reproduce crashes.
inline void SetupLog(spdlog::level::level_enum level = spdlog::level::off)
{
    if (level == spdlog::level::off) {
        spdlog::set_level(spdlog::level::off);
        return;
    }

    auto logsFolder = SKSE::log::log_directory();
    if (!logsFolder) SKSE::stl::report_and_fail("SKSE log_directory not provided, logs disabled.");

    auto pluginName  = SKSE::PluginDeclaration::GetSingleton()->GetName();
    auto logFilePath = *logsFolder / std::format("{}.log", pluginName);

    auto fileLoggerPtr = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
    auto loggerPtr     = std::make_shared<spdlog::logger>("log", std::move(fileLoggerPtr));

    spdlog::set_default_logger(std::move(loggerPtr));
    spdlog::set_level(level);
    // Flush on every message regardless of level — ensures the last lines are
    // always on disk if the process crashes before a normal flush.
    spdlog::flush_on(spdlog::level::trace);
}
