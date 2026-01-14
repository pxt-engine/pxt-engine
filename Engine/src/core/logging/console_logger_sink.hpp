#pragma once

#include "core/logging/logger_sink.hpp"

// Forward declaration of spdlog classes to keep header clean
namespace spdlog {
    class logger;
}

namespace pxt::core {

    class ConsoleLoggerSink : public LoggerSink {
    public:
        ConsoleLoggerSink();
        virtual ~ConsoleLoggerSink() override = default;

        void log(LogLevel level, std::string_view message, const std::source_location loc = {}) override;

    private:
        Shared<spdlog::logger> m_internalLogger;
    };

} // namespace pxt::core