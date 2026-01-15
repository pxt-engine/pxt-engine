#include "editor_logger_sink.hpp"

namespace pxt::editor {

    EditorLoggerSink::EditorLoggerSink(EditorConsole& console, core::LogLevel level)
        : m_console(console), m_logLevel(level) {}

    void EditorLoggerSink::log(core::LogLevel level, std::string_view message, const std::source_location loc) {
        if (level < m_logLevel)
            return;

        EditorLogEntry entry;
        entry.level = level;
        entry.location = loc;
        entry.message = std::string(message);
        entry.timestamp = std::chrono::system_clock::now();

        m_console.push(entry);
    }

} // namespace pxt::editor