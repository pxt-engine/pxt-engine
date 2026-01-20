#include "core/logging/console_logger_sink.hpp"

#include "core/logging/log_helper.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace pxt::core {

    constexpr char const* LOGGER_NAME = "PXT";

    // %l for the log level (trace, debug, info, warn, error, critical)
    // %n for the logger name
    // %v for the log message
    // %t for thread ID
    // %s for the source file -> file.cpp
    // %@ for source file and line number -> file.cpp:123
    constexpr char const* LOG_PATTERN = "%^[%H:%M:%S] [%!] [%l]%$ %v";

    ConsoleLoggerSink::ConsoleLoggerSink(LogLevel level) {
        // Create the internal spdlog sink
        auto sink = createShared<spdlog::sinks::stdout_color_sink_mt>();

        // Create a dedicated logger for this sink
        m_internalLogger = createShared<spdlog::logger>(LOGGER_NAME, sink);
        m_internalLogger->set_pattern(LOG_PATTERN);

        spdlog::level::level_enum spdlogLevel = pxtToSpdlogLevel(level);
        m_internalLogger->set_level(spdlogLevel);
    }

    void ConsoleLoggerSink::log(LogLevel level, std::string_view message, const std::source_location loc) {
        spdlog::level::level_enum spdlogLevel = pxtToSpdlogLevel(level);

        // Check if the location is valid (file_name will be empty if default initialized)
        if (loc.file_name()[0] != '\0') {
            std::string cleanedFunc = std::string(cleanFunctionName(loc.function_name()));

            spdlog::source_loc spdLoc{loc.file_name(), static_cast<int>(loc.line()), cleanedFunc.c_str()};
            m_internalLogger->log(spdLoc, spdlogLevel, message);
        } else {
            // Log without source info
            m_internalLogger->log(spdlogLevel, message);
        }
    }

} // namespace pxt::core
