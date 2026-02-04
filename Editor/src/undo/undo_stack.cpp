#include "undo/undo_stack.hpp"

namespace pxt::editor {

    void UndoStack::executeCommand(Unique<Command> command) {
        command->execute({});
        m_undoStack.push_back(std::move(command));
        m_redoStack.clear();
    }

    void UndoStack::undo(const CommandContext& ctx) {
        if (canUndo()) {
            Unique<Command> command = std::move(m_undoStack.back());
            m_undoStack.pop_back();
            command->undo(ctx);
            m_redoStack.push_back(std::move(command));
        }
    }

    void UndoStack::redo(const CommandContext& ctx) {
        if (canRedo()) {
            Unique<Command> command = std::move(m_redoStack.back());
            m_redoStack.pop_back();
            command->execute(ctx);
            m_undoStack.push_back(std::move(command));
        }
    }

    bool UndoStack::canUndo() const { return !m_undoStack.empty(); }

    bool UndoStack::canRedo() const { return !m_redoStack.empty(); }

} // namespace pxt::editor