#pragma once

#include "core/pch.hpp"

#include "core/events/event_queue.hpp"
#include "scene/scene.hpp"

namespace pxt::editor {

    /**
     * @brief Context provided to commands during execution and undoing.
     *
     * It contains pointers to relevant systems for command operations.
     */
    struct ExecutionContext {
        core::EventQueue* eventQueue;
        Scene* scene;
    };

    /**
     * @brief Interface for commands that can be executed and undone.
     *
     * Used in the command pattern for undo/redo functionality.
     */
    class Command {
    public:
        virtual ~Command() = default;
        virtual void execute(ExecutionContext& ctx) = 0;
        virtual void undo(ExecutionContext& ctx) = 0;
    };

} // namespace pxt::editor