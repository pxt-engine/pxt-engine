#include "scene/scene.hpp"

#include "core/events/editor_events.hpp"
#include "core/events/event_dispatcher.hpp"
#include "scene/ecs/component.hpp"
#include "scene/ecs/entity.hpp"
#include "scene/script/script.hpp"

namespace pxt {
    Scene::Scene() {
        // put invalid ids into m_objPickingIdToUUID map
        m_objPickingIdToUUID[core::ObjPickingId::s_invalidId] = core::UUID::s_invalidId;
        m_uuidToObjPickingId[core::UUID::s_invalidId] = core::ObjPickingId::s_invalidId;
    }

    std::string Scene::getUniqueEntityName(const std::string& baseName) {
        // Collect all existing names for fast lookup
        std::unordered_set<std::string> existingNames;
        auto view = getEntitiesWith<NameComponent>();
        for (auto entityHandle : view) {
            existingNames.insert(view.get<NameComponent>(entityHandle).name);
        }

        // If base name is free, return it immediately
        if (existingNames.find(baseName) == existingNames.end()) {
            return baseName;
        }

        // Increment until a unique name is found
        int counter = 1;
        std::string candidate;
        do {
            candidate = baseName + " " + std::to_string(counter++);
        } while (existingNames.find(candidate) != existingNames.end());

        return candidate;
    }

    Entity Scene::createEntity(const std::string& name, core::UUID id, core::ObjPickingId objPickingId) {
        Entity entity = {m_registry.create(), this};

        std::string newEntityName = name.empty() ? "Unnamed-Entity" : name;

        // ensure unique name
        newEntityName = getUniqueEntityName(newEntityName);

        entity.add<IDComponent>(id);
        entity.add<ObjPickingIdComponent>(objPickingId);
        entity.add<NameComponent>(newEntityName);

        m_entityMap[entity.getUUID()] = entity;
        m_objPickingIdToUUID[objPickingId.getObjPickingId()] = entity.getUUID();
        m_uuidToObjPickingId[entity.getUUID()] = objPickingId.getObjPickingId();

        return entity;
    }

    Entity Scene::getEntity(core::UUID uuid) {
        PXT_ASSERT(m_entityMap.contains(uuid), "Entity not found in Scene!");

        return {m_entityMap.at(uuid), this};
    }

    core::UUID Scene::getEntityUUIDFromObjPickingId(uint32_t objPickingId) {
        PXT_ASSERT(m_objPickingIdToUUID.contains(objPickingId), "UUID not found in map!");

        return m_objPickingIdToUUID.at(objPickingId);
    }

    uint32_t Scene::getObjPickingIdFromEntityUUID(core::UUID uuid) {
        PXT_ASSERT(m_uuidToObjPickingId.contains(uuid), "ObjPickingId not found in map!");

        return m_uuidToObjPickingId.at(uuid);
    }

    void Scene::destroyEntity(core::UUID uuid) {
        PXT_ASSERT(m_entityMap.contains(uuid), "Trying to destroy non-existent entity!");

        // Reset active camera if it's being deleted
        if (uuid == m_activeCameraEntityID) {
            m_activeCameraEntityID = core::UUID::s_invalidId;
        }

        entt::entity handle = m_entityMap.at(uuid);
        Entity entity = {handle, this};

        // Clean up maps
        const uint32_t pickingId = getObjPickingIdFromEntityUUID(uuid);
        m_objPickingIdToUUID.erase(pickingId);
        m_uuidToObjPickingId.erase(uuid);
        m_entityMap.erase(uuid);

        // Final registry destruction
        m_registry.destroy(handle);
    }

    template <typename... Component>
    static void copyComponentList(ComponentList<Component...>, Entity source, Entity dest) {
        // Fold expression: for each 'Component' in 'ComponentList'
        (
            [&]() {
                if (source.has<Component>()) {
                    dest.add<Component>(source.get<Component>());
                }
            }(),
            ...);
    }

    Entity Scene::duplicateEntity(core::UUID uuid) {
        PXT_ASSERT(m_entityMap.contains(uuid), "Source entity not found!");

        Entity source = getEntity(uuid);

        std::string originalName = source.getName();

        // Check if "(Copy)" already exists in the name
        size_t copyPos = originalName.find(" (Copy)");
        std::string baseName;

        if (copyPos != std::string::npos) {
            // It's already a copy, so just take the part before " (Copy)"
            // This prevents names like "Cube (Copy) (Copy)"
            baseName = originalName.substr(0, copyPos) + " (Copy)";
        } else {
            // First time copying
            baseName = originalName + " (Copy)";
        }

        // getUniqueEntityName will now take "Cube (Copy)" and
        // find the next available: "Cube (Copy) 1", "Cube (Copy) 2", etc.
        std::string uniqueName = getUniqueEntityName(baseName);

        Entity replica = createEntity(uniqueName);

        copyComponentList(AttachableComponents{}, source, replica);

        return replica;
    }

    void Scene::onStart() {
        getEntitiesWith<ScriptComponent>().each([this](auto entity, auto& scriptComponent) {
            scriptComponent.script = scriptComponent.create();
            scriptComponent.script->m_entity = Entity{entity, this};
            scriptComponent.script->onCreate();
        });
    }

    void Scene::onUpdate(float delta) {
        getEntitiesWith<ScriptComponent>().each([=](auto entity, auto& scriptComponent) {
            if (!scriptComponent.script) {
                return;
            }

            scriptComponent.script->onUpdate(delta);
        });
    }

    std::optional<Entity> Scene::getActiveCameraEntity() {
        if (m_activeCameraEntityID == core::UUID::s_invalidId) {
            return std::nullopt;
        }

        Entity activeCameraEntity = getEntity(m_activeCameraEntityID);

        //! this could happen because the scene saves the uuid of the entity
        //! so if the user sets a camera to be active but then removes its
        //! CameraComponent or TransformComponent, this method would return
        //! an entity which does not contain a CameraComponent anymore, leading to bugs.
        if (!activeCameraEntity.has<CameraComponent, TransformComponent>()) {
            // so here we set the active camera to invalid and return nullopt
            setActiveCameraEntity(core::UUID::s_invalidId);

            return std::nullopt;
        }

        return activeCameraEntity;
    }

    core::UUID Scene::getActiveCameraEntityUUID() { return m_activeCameraEntityID; }

    void Scene::setActiveCameraEntity(core::UUID newActiveCameraID) {
        // if the entity exists and does not have a cameraComponent and a TransformComponent it cannot be the active
        // camera
        if (newActiveCameraID != core::UUID::s_invalidId &&
            !getEntity(newActiveCameraID).has<CameraComponent, TransformComponent>()) {
            PXT_WARN("Cannot select Entity \"{}\" as active camera because it does not contain a CameraComponent or "
                     "TransformComponent!",
                     getEntity(newActiveCameraID).getName());

            return;
        }

        m_activeCameraEntityID = newActiveCameraID;
    }

    void Scene::updateCamerasAspectRatio(float newAspect) {
        getEntitiesWith<CameraComponent>().each([=](auto entity, auto& cameraComponent) {
            if (cameraComponent.useViewportAspectRatio) {
                cameraComponent.aspectRatio = newAspect;
            }
        });
    }

    std::optional<Entity> Scene::tryFindCamera() {
        auto view = getEntitiesWith<CameraComponent>();
        if (view.empty()) {
            return std::nullopt;
        }

        return Entity{view.front(), this};
    }

    void Scene::onEvent(core::Event& event) {
        core::EventDispatcher dispatcher(event);

        // if viewport changed we need to update cameras using its aspect ratio
        dispatcher.dispatch<core::ViewportResizeEvent>([this](auto& event) {
            float newAspect = static_cast<float>(event.getWidth()) / static_cast<float>(event.getHeight());
            updateCamerasAspectRatio(newAspect);

            return false;
        });

        getEntitiesWith<ScriptComponent>().each([&](auto entity, auto& scriptComponent) {
            if (!scriptComponent.script) {
                return;
            }

            scriptComponent.script->onEvent(event);
        });
    }
} // namespace pxt