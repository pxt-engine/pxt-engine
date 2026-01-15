#pragma once

#include "core/pch.hpp"

#include "core/logging/logger_sink.hpp"

#include "ui/editor_console.hpp"

namespace pxt::editor {

    class EditorLoggerSink : public core::LoggerSink {
    public:
        EditorLoggerSink(EditorConsole& console, core::LogLevel level = core::LogLevel::Trace);
        virtual ~EditorLoggerSink() override = default;

        void log(core::LogLevel level, std::string_view message, const std::source_location loc = {}) override;

    private:
        EditorConsole& m_console;
        core::LogLevel m_logLevel;
    };

} // namespace pxt::editor