#pragma once

#include "core/logging/logger_sink.hpp"
#include "core/memory.hpp"

namespace pxt::core {

    class Logger {
    public:
        /**
         * @brief Shutdown logger system.
         */
        static void shutdown();

        /**
         * @brief Register a new sink for logging.
         * @param sink The sink to register
         */
        static void registerSink(Shared<LoggerSink> sink);

        /**
         * @brief Log a message to all registered sinks.
         * @param level The log level
         * @param message The message to log
         */
        static void log(LogLevel level, std::string_view message, const std::source_location loc = {});

    private:
        static std::vector<Shared<LoggerSink>> s_sinks;

        /**
         * @brief This mutex protects access to the sinks vector.
         */
        static std::mutex s_mutex;
    };
} // namespace pxt::core

namespace pxt {

#define PXT_LOG(level, ...) pxt::core::Logger::log(level, std::format(__VA_ARGS__), std::source_location::current())
#define PXT_TRACE(...) PXT_LOG(pxt::core::LogLevel::Trace, __VA_ARGS__)
#define PXT_DEBUG(...) PXT_LOG(pxt::core::LogLevel::Debug, __VA_ARGS__)
#define PXT_INFO(...) PXT_LOG(pxt::core::LogLevel::Info, __VA_ARGS__)
#define PXT_WARN(...) PXT_LOG(pxt::core::LogLevel::Warn, __VA_ARGS__)
#define PXT_ERROR(...) PXT_LOG(pxt::core::LogLevel::Error, __VA_ARGS__)
#define PXT_FATAL(...) PXT_LOG(pxt::core::LogLevel::Fatal, __VA_ARGS__)

} // namespace pxt