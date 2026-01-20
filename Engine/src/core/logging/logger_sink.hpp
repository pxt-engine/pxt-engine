#pragma once

namespace pxt::core {

    /**
     * @brief Enumeration of log levels.
     */
    enum class LogLevel : uint8_t { Trace = 0, Debug, Info, Warn, Error, Fatal };

    /**
     * @brief Interface for logging sinks that handle log messages.
     */
    struct LoggerSink {
        virtual ~LoggerSink() = default;
        virtual void log(LogLevel level, std::string_view message, const std::source_location loc = {}) = 0;
    };

} // namespace pxt::core