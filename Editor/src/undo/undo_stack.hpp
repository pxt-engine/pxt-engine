#pragma once

#include "core/pch.hpp"

#include "undo/command.hpp"

namespace pxt::editor {

    enum class CommandOperation { Execute, Undo, Redo };

    struct PendingCommand {
        CommandOperation operation;

        // Valid only if operation is EXECUTE
        Unique<Command> command;
    };

    class UndoStack {
    public:
        UndoStack() = default;
        ~UndoStack() = default;

        void setExecutionContext(ExecutionContext* ctx) { m_executionContext = ctx; }

        void flush();

        void submitCommand(Unique<Command> command);
        void submitUndo();
        void submitRedo();

        bool canUndo() const;
        bool canRedo() const;

    private:
        ExecutionContext* m_executionContext;

        // TODO: limit stack size to prevent excessive memory usage (maybe a circular buffer)
        std::vector<Unique<Command>> m_undoStack;
        std::vector<Unique<Command>> m_redoStack;

        std::vector<PendingCommand> m_pendingCommands;
    };

} // namespace pxt::editor