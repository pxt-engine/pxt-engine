#include "undo/undo_stack.hpp"

namespace pxt::editor {

    void UndoStack::flush() {
        PXT_ASSERT(m_executionContext, "CommandContext not set");

        for (auto& pendingCommand : m_pendingCommands) {
            switch (pendingCommand.operation) {

            case CommandOperation::Execute: {

                pendingCommand.command->execute(*m_executionContext);

                m_undoStack.push_back(std::move(pendingCommand.command));
                m_redoStack.clear();
                break;
            }

            case CommandOperation::Undo: {
                if (!canUndo())
                    break;

                auto cmd = std::move(m_undoStack.back());
                m_undoStack.pop_back();

                cmd->undo(*m_executionContext);
                m_redoStack.push_back(std::move(cmd));
                break;
            }

            case CommandOperation::Redo: {

                if (!canRedo())
                    break;

                auto cmd = std::move(m_redoStack.back());
                m_redoStack.pop_back();

                cmd->execute(*m_executionContext);
                m_undoStack.push_back(std::move(cmd));
                break;
            }

            default:
                std::unreachable();
            }
        }

        m_pendingCommands.clear();
    }

    void UndoStack::submitCommand(Unique<Command> command) {
        m_pendingCommands.push_back({CommandOperation::Execute, std::move(command)});
    }

    void UndoStack::submitUndo() {
        if (!canUndo())
            return;

        m_pendingCommands.push_back({CommandOperation::Undo, nullptr});
    }

    void UndoStack::submitRedo() {
        if (!canRedo())
            return;

        m_pendingCommands.push_back({CommandOperation::Redo, nullptr});
    }

    bool UndoStack::canUndo() const { return !m_undoStack.empty(); }

    bool UndoStack::canRedo() const { return !m_redoStack.empty(); }

} // namespace pxt::editor