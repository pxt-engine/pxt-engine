#include "commands/entity_commands.hpp"

#include "core/events/editor_events.hpp"
#include "scene/ecs/entity.hpp"

namespace pxt::editor::commands {

    CreateEntityCommand::CreateEntityCommand(const std::string& name, core::UID uid) : m_name(name), m_uid(uid) {}

    void CreateEntityCommand::execute(ExecutionContext& ctx) {
        ctx.scene->createEntity(m_name, m_uid);

        ctx.eventQueue->queueEvent(core::SelectedEntityChangedEvent(m_uid));

        PXT_INFO("Executed EntityCreateCommand: Created entity \"{}\" with UID {}", m_name, m_uid.toString());
    }

    void CreateEntityCommand::undo(ExecutionContext& ctx) {
        ctx.scene->destroyEntity(m_uid);

        PXT_INFO("Undid EntityCreateCommand: Destroyed entity with UID {}", m_uid.toString());
    }

    DestroyEntityCommand::DestroyEntityCommand(core::UID uid) : m_uid(uid) {}

    void DestroyEntityCommand::execute(ExecutionContext& ctx) {
        ctx.scene->destroyEntity(m_uid);

        PXT_INFO("Executed DestroyEntityCommand: Destroyed entity with UID {}", m_uid.toString());
    }

    void DestroyEntityCommand::undo(ExecutionContext& ctx) {
        // TODO: Restore the entity's previous state
        // we have to store the entity's state before destruction to restore it accurately

        PXT_INFO("Undid DestroyEntityCommand: Restored entity with UID {}", m_uid.toString());
    }

    DuplicateEntityCommand::DuplicateEntityCommand(core::UID originalUid, core::UID copyUid)
        : m_originalUid(originalUid), m_copyUid(copyUid) {}

    void DuplicateEntityCommand::execute(ExecutionContext& ctx) {
        ctx.scene->duplicateEntity(m_originalUid, m_copyUid);

        ctx.eventQueue->queueEvent(core::SelectedEntityChangedEvent(m_copyUid));

        PXT_INFO("Executed DuplicateEntityCommand: Duplicated entity with UID {} to new UID {}",
                 m_originalUid.toString(), m_copyUid.toString());
    }

    void DuplicateEntityCommand::undo(ExecutionContext& ctx) {
        ctx.scene->destroyEntity(m_copyUid);

        PXT_INFO("Undid DuplicateEntityCommand: Destroyed duplicated entity with UID {}", m_copyUid.toString());
    }

} // namespace pxt::editor::commands