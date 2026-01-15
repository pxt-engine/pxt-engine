#pragma once

#include "pxtengine.h"
#include "ui/widgets/space.hpp"

namespace pxt::editor {

    struct EditorLogEntry {
        core::LogLevel level;
        std::string message;
        std::source_location location;
        std::chrono::system_clock::time_point timestamp;
    };

    class EditorConsole {
    public:
        EditorConsole();

        void push(EditorLogEntry entry);
        void clear();

        void onUpdateUi();

    private:
        std::vector<EditorLogEntry> m_entries;
        bool m_autoScrollEnabled = true;
        bool m_scrollToBottom = false;

        ImFont* m_consoleFont = nullptr;
    };

} // namespace pxt::editor