#pragma once

#include "core/pch.hpp"

#include "undo/command.hpp"

namespace pxt::editor {

    class UndoStack {
    public:
        UndoStack() = default;
        ~UndoStack() = default;

        void executeCommand(Unique<Command> command);

        void undo();
        void redo();
        bool canUndo() const;
        bool canRedo() const;

    private:
        // TODO: limit stack size to prevent excessive memory usage (maybe a circular buffer)
        std::vector<Unique<Command>> m_undoStack;
        std::vector<Unique<Command>> m_redoStack;
    };

} // namespace pxt::editor