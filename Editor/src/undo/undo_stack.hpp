#pragma once

#include "core/pch.hpp"

#include "undo/command.hpp"

namespace pxt::editor {

    class UndoStack {
    public:
        UndoStack() = default;
        ~UndoStack() = default;

        void executeCommand(Unique<Command> command, const CommandContext& ctx);

        void undo(const CommandContext& ctx);
        void redo(const CommandContext& ctx);
        bool canUndo() const;
        bool canRedo() const;

    private:
        // TODO: limit stack size to prevent excessive memory usage (maybe a circular buffer)
        std::vector<Unique<Command>> m_undoStack;
        std::vector<Unique<Command>> m_redoStack;
    };

} // namespace pxt::editor