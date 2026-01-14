#include "core/logging/file_logger_sink.hpp"

#include "core/logging/log_helper.hpp"

#include <spdlog/sinks/basic_file_sink.h>

namespace pxt::core {

    constexpr char const* LOGGER_NAME = "PXT";
    constexpr char const* LOG_PATH = "logs/PXT.log";

    // %l for the log level (trace, debug, info, warn, error, critical)
    // %n for the logger name
    // %v for the log message
    // %t for thread ID
    // %s for the source file -> file.cpp
    // %@ for source file and line number -> file.cpp:123
    constexpr char const* LOG_PATTERN = "%^[%H:%M:%S] [%!] [%l]%$ %v";

    FileLoggerSink::FileLoggerSink(LogLevel level) {
        // Create the internal spdlog file sink (true = truncate file on start)
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(LOG_PATH, true);

        m_internalLogger = std::make_shared<spdlog::logger>(LOGGER_NAME, sink);
        m_internalLogger->set_pattern(LOG_PATTERN);

        spdlog::level::level_enum spdlogLevel = pxtToSpdlogLevel(level);
        m_internalLogger->set_level(spdlogLevel);
    }

    void FileLoggerSink::log(LogLevel level, std::string_view message, const std::source_location loc) {
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
