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

        void onTransformCreate(entt::entity enttEntity);
        
        void onTransform2dCreate(entt::entity enttEntity);
        
        void onMeshUpdate(entt::entity enttEntity);

        void onPropertiesUpdate(entt::entity enttEntity);

        void onMaterialCreate(entt::entity enttEntity);
        void onMaterialUpdate(entt::entity enttEntity);
    };
} // namespace pxt