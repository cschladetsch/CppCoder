#include "cppcoder/Logging.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <vector>

namespace cppcoder {

void InitLogging(const std::string& levelName, const std::string& logFilePath) {
    std::vector<spdlog::sink_ptr> sinks;

    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_pattern("%^[%L]%$ %v");
    sinks.push_back(consoleSink);

    if (!logFilePath.empty()) {
        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath, true);
        fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        sinks.push_back(fileSink);
    }

    auto logger = std::make_shared<spdlog::logger>("cppcoder", sinks.begin(), sinks.end());
    logger->flush_on(spdlog::level::warn);
    spdlog::set_default_logger(logger);

    spdlog::level::level_enum level = spdlog::level::from_str(levelName);
    if (level == spdlog::level::off && levelName != "off") {
        // spdlog::level::from_str() silently falls back to "off" for any
        // unrecognised name; treat that as a mistake rather than intent
        // and use "info" instead.
        level = spdlog::level::info;
    }
    spdlog::set_level(level);
}

}  // namespace cppcoder
