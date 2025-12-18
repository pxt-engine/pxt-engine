#pragma once

#include "core/memory.hpp"

#include <spdlog/spdlog.h>

namespace pxt::core {

    class Logger {
    public:
        static void init();

        inline static Shared<spdlog::logger>& get() { return s_logger; }

        inline static void shutdown() {
            if (s_logger) {
                s_logger->flush();  // Flush the logger before dropping it
                spdlog::drop_all(); // Drop all loggers
                s_logger.reset();   // Reset the logger pointer
            }
        }

        ~Logger() { shutdown(); }

    private:
        static Shared<spdlog::logger> s_logger;
    };
} // namespace pxt::core

namespace pxt {

#define PXT_TRACE(...) SPDLOG_LOGGER_TRACE(pxt::core::Logger::get(), __VA_ARGS__)
#define PXT_DEBUG(...) SPDLOG_LOGGER_DEBUG(pxt::core::Logger::get(), __VA_ARGS__)
#define PXT_INFO(...) SPDLOG_LOGGER_INFO(pxt::core::Logger::get(), __VA_ARGS__)
#define PXT_WARN(...) SPDLOG_LOGGER_WARN(pxt::core::Logger::get(), __VA_ARGS__)
#define PXT_ERROR(...) SPDLOG_LOGGER_ERROR(pxt::core::Logger::get(), __VA_ARGS__)
#define PXT_FATAL(...) SPDLOG_LOGGER_CRITICAL(pxt::core::Logger::get(), __VA_ARGS__)

} // namespace pxt