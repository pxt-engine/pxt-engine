#include "scene/entity_lifecycle_listener.hpp"

#include "application.hpp"
#include "core/filesystem.hpp"
#include "scene/ecs/entity.hpp"

namespace pxt {
    EntityLifecycleListener::EntityLifecycleListener(Scene& scene) : m_scene(scene) {
        registerCallbacks(m_scene.getRegistry());
    }

    void EntityLifecycleListener::registerCallbacks(entt::registry& registry) {
        registry.on_construct<TransformComponent>().connect<&EntityLifecycleListener::onTransformCreate>(this);
        registry.on_construct<Transform2dComponent>().connect<&EntityLifecycleListener::onTransform2dCreate>(this);

        registry.on_update<MeshComponent>().connect<&EntityLifecycleListener::onMeshUpdate>(this);
        registry.on_construct<MeshComponent>().connect<&EntityLifecycleListener::onMeshUpdate>(this);

        registry.on_update<PropertiesComponent>().connect<&EntityLifecycleListener::onPropertiesUpdate>(this);

        registry.on_construct<MaterialComponent>().connect<&EntityLifecycleListener::onMaterialCreate>(this);
        registry.on_update<MaterialComponent>().connect<&EntityLifecycleListener::onMaterialUpdate>(this);
    }

    void EntityLifecycleListener::validateRenderState(Entity& entity) {
        const bool hasMesh = entity.has<MeshComponent>() && entity.get<MeshComponent>().mesh.isValid();
        const bool hasMaterial = entity.has<MaterialComponent>() && entity.get<MaterialComponent>().material.isValid();

        bool visible = false;
        bool renderable = false;
        if (hasMesh && hasMaterial) {
            const auto& props = entity.get<PropertiesComponent>();
            visible = props.isEditorVisible;
            renderable = props.isRenderable;
        }

        // update tags based on current state
        if (visible) {
            entity.add<VisibilityTag>();
        } else {
            entity.remove<VisibilityTag>();
        }

        if (renderable) {
            entity.add<RenderableTag>();
        } else {
            entity.remove<RenderableTag>();
        }
    }

    void EntityLifecycleListener::onTransformCreate(entt::entity enttEntity) {
        // for now we just check if the entity already has a transform2d,
        // in that case we remove the newly created transform3d component
        Entity entity = {enttEntity, &m_scene};

        if (entity.has<Transform2dComponent>()) {
            // TODO: remove of constructed component cannot be called. use an event based removal
            // entity.remove<TransformComponent>();

            const std::string warningMessage =
                "Entity " + entity.getName() +
                " cannot have both TransformComponent and Transform2dComponent. The "
                "TransformComponent has been removed (not yet, we have to call an event!!!!).";
            core::FileSystem::openWarningModal(warningMessage);
        }
    }

    void EntityLifecycleListener::onTransform2dCreate(entt::entity enttEntity) {
        // for now we just check if the entity already has a transform,
        // in that case we remove the newly created transform2d component
        Entity entity = {enttEntity, &m_scene};

        if (entity.has<TransformComponent>()) {
            // TODO: remove of constructed component cannot be called. use an event based removal
            // entity.remove<Transform2dComponent>();

            const std::string warningMessage =
                "Entity " + entity.getName() +
                " cannot have both Transform2dComponent and TransformComponent. The "
                "Transform2dComponent has been removed (not yet, we have to call an event!!!!).";
            core::FileSystem::openWarningModal(warningMessage);
        }
    }

    void EntityLifecycleListener::onMeshUpdate(entt::entity enttEntity) {
        Entity entity = {enttEntity, &m_scene};

        validateRenderState(entity);
    }

    void EntityLifecycleListener::onPropertiesUpdate(entt::entity enttEntity) {
        Entity entity = {enttEntity, &m_scene};

        validateRenderState(entity);
    }

    void EntityLifecycleListener::onMaterialCreate(entt::entity enttEntity) {
        Entity entity = {enttEntity, &m_scene};

        auto& materialHandle = entity.get<MaterialComponent>().material;

        // we assign the default material on creation if there is no material assigned, more intuitive for the user
        //? maybe we should not expose resource manager here
        if (!materialHandle.isValid()) {
            materialHandle = AssetHandle{Application::get().getResourceManager().s_defaultMaterial->id};
        }

        validateRenderState(entity);
    }

    void EntityLifecycleListener::onMaterialUpdate(entt::entity enttEntity) {
        Entity entity = {enttEntity, &m_scene};

        validateRenderState(entity);
    }
} // namespace pxt