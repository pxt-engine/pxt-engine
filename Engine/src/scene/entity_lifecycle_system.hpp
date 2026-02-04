#pragma once

#include "core/pch.hpp"

namespace pxt {
    class Scene; // forward declaration
    class Entity;

    class EntityLifecycleSystem {
    public:
        explicit EntityLifecycleSystem(Scene& scene);

    private:
        Scene& m_scene;

        void registerCallbacks(entt::registry& registry);

        void validateRenderState(Entity& entity);

        void onTransformCreate(entt::registry& registry, entt::entity enttEntity);
        
        void onTransform2dCreate(entt::registry& registry, entt::entity enttEntity);
        
        void onMeshUpdate(entt::registry& registry, entt::entity enttEntity);

        void onPropertiesUpdate(entt::registry& registry, entt::entity enttEntity);

        void onMaterialCreate(entt::registry& registry, entt::entity enttEntity);
        void onMaterialUpdate(entt::registry& registry, entt::entity enttEntity);
    };
} // namespace pxt