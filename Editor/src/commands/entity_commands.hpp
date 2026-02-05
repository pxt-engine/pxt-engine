#pragma once

#include "core/pch.hpp"
#include "core/uid.hpp"

#include "undo/command.hpp"

namespace pxt::editor::commands {

    class EntityCreateCommand : public Command {
    public:
        EntityCreateCommand(const std::string& name);
        ~EntityCreateCommand() override = default;

        void execute(ExecutionContext& ctx) override;
        void undo(ExecutionContext& ctx) override;

    private:
        core::UID m_uid = core::UID::s_invalidId;
        std::string m_name;
    };

} // namespace pxt::editor::commands