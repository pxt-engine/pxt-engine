#include "core/logging/logger.hpp"

namespace pxt::core {

    std::vector<Shared<LoggerSink>> Logger::s_sinks;
    std::mutex Logger::s_mutex;

    void Logger::shutdown() {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_sinks.clear();
    }

    void Logger::registerSink(Shared<LoggerSink> sink) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_sinks.push_back(sink);
    }

    void Logger::disconnectSink(Shared<LoggerSink> sink) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_sinks.erase(std::remove(s_sinks.begin(), s_sinks.end(), sink), s_sinks.end());
    }

    void Logger::log(LogLevel level, std::string_view message, const std::source_location loc) {
        std::lock_guard<std::mutex> lock(s_mutex);

        for (auto& sink : s_sinks) {
            sink->log(level, message, loc);
        }
    }

} // namespace pxt::core