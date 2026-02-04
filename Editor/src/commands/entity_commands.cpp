#include "commands/entity_commands.hpp"

#include "scene/ecs/entity.hpp"

namespace pxt::editor::commands {

    EntityCreateCommand::EntityCreateCommand(const std::string& name) : m_name(name) {}

    void EntityCreateCommand::execute(const CommandContext& ctx) {
        Entity entity = ctx.scene->createEntity(m_name);

        m_uid = entity.getUID();

        PXT_INFO("Executed EntityCreateCommand: Created entity \"{}\" with UID {}", m_name, m_uid.toString());
    }

    void EntityCreateCommand::undo(const CommandContext& ctx) {
        ctx.scene->destroyEntity(m_uid);

        PXT_INFO("Undid EntityCreateCommand: Destroyed entity with UID {}", m_uid.toString());
    }

} // namespace pxt::editor::commands