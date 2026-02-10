#pragma once

#include "core/pch.hpp"
#include "core/uid.hpp"

#include "undo/command.hpp"

namespace pxt::editor::commands {

    class CreateEntityCommand : public Command {
    public:
        explicit CreateEntityCommand(const std::string& name, core::UID uid);
        ~CreateEntityCommand() override = default;

        void execute(ExecutionContext& ctx) override;
        void undo(ExecutionContext& ctx) override;

    private:
        core::UID m_uid = core::UID::s_invalidId;
        std::string m_name;
    };

    class CreateEntityFromMeshCommand : public Command {
    public:
        explicit CreateEntityFromMeshCommand(const std::string& name, core::UID uid, AssetHandle mesh);
        ~CreateEntityFromMeshCommand() override = default;

        void execute(ExecutionContext& ctx) override;
        void undo(ExecutionContext& ctx) override;

    private:
        core::UID m_uid = core::UID::s_invalidId;
        std::string m_name;
        AssetHandle m_mesh;
    };

    class DestroyEntityCommand : public Command {
    public:
        explicit DestroyEntityCommand(core::UID uid);
        ~DestroyEntityCommand() override = default;

        void execute(ExecutionContext& ctx) override;
        void undo(ExecutionContext& ctx) override;

    private:
        core::UID m_uid;
    };

    class DuplicateEntityCommand : public Command {
    public:
        explicit DuplicateEntityCommand(core::UID originalUid, core::UID copyUid);
        ~DuplicateEntityCommand() override = default;

        void execute(ExecutionContext& ctx) override;
        void undo(ExecutionContext& ctx) override;

    private:
        core::UID m_originalUid;
        core::UID m_copyUid = core::UID::s_invalidId;
    };

} // namespace pxt::editor::commands