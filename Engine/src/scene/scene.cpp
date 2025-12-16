#include "scene/scene.hpp"

#include "scene/ecs/component.hpp"
#include "scene/ecs/entity.hpp"
#include "scene/script/script.hpp"

namespace pxt {
    Scene::Scene() {
        // put invalid ids into m_objPickingIdToUUID map
		m_objPickingIdToUUID[core::ObjPickingId::s_invalidId] = core::UUID::s_invalidId;
		m_uuidToObjPickingId[core::UUID::s_invalidId] = core::ObjPickingId::s_invalidId;
    }

    Entity Scene::createEntity(const std::string& name, core::UUID id, core::ObjPickingId objPickingId) {
        Entity entity = { m_registry.create(), this };

        entity.add<IDComponent>(id);
		entity.add<ObjPickingIdComponent>(objPickingId);
        entity.add<NameComponent>(name.empty() ? "Unnamed-Entity" : name);

        m_entityMap[entity.getUUID()] = entity;
		m_objPickingIdToUUID[objPickingId.getObjPickingId()] = entity.getUUID();
		m_uuidToObjPickingId[entity.getUUID()] = objPickingId.getObjPickingId();
        
        return entity;
    }

    Entity Scene::getEntity(core::UUID uuid) {
        PXT_ASSERT(m_entityMap.contains(uuid), "Entity not found in Scene!");

        return { m_entityMap.at(uuid), this };
    }

    core::UUID Scene::getEntityUUIDFromObjPickingId(uint32_t objPickingId) {
        PXT_ASSERT(m_objPickingIdToUUID.contains(objPickingId), "UUID not found in map!");

        return m_objPickingIdToUUID.at(objPickingId);
    }

    uint32_t Scene::getObjPickingIdFromEntityUUID(core::UUID uuid) {
        PXT_ASSERT(m_uuidToObjPickingId.contains(uuid), "ObjPickingId not found in map!");

        return m_uuidToObjPickingId.at(uuid);
    }

    void Scene::destroyEntity(Entity entity) {
        m_entityMap.erase(entity.getUUID());
		m_objPickingIdToUUID.erase(entity.getObjPickingId());
		m_uuidToObjPickingId.erase(entity.getUUID());
        m_registry.destroy(entity);
    }

    Entity Scene::getMainCameraEntity() {
        auto cameraEntities = m_registry.view<CameraComponent, TransformComponent>();
        
        for (auto entity : cameraEntities) {
            if (!cameraEntities.get<CameraComponent>(entity).isMainCamera) continue;
            
            return { entity, this };
        }

        return {};
    }

    void Scene::onStart() {
        getEntitiesWith<ScriptComponent>().each([this](auto entity, auto& scriptComponent) {
            scriptComponent.script = scriptComponent.create();
            scriptComponent.script->m_entity = Entity{ entity, this };
            scriptComponent.script->onCreate();
        });
    }

    void Scene::onUpdate(float delta) {
        getEntitiesWith<ScriptComponent>().each([=](auto entity, auto& scriptComponent) {
            
            scriptComponent.script->onUpdate(delta);
            
        });
    }
}