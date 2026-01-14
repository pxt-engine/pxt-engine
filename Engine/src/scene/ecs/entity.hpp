#pragma once

#include "core/pch.hpp"
#include "scene/ecs/component.hpp"
#include "scene/scene.hpp"

namespace pxt {

    class Entity {
    public:
        Entity() = default;

        Entity(entt::entity entity, Scene* scene) : m_enttEntity(entity), m_scene(scene) {}

        operator entt::entity() const { return m_enttEntity; }

        operator bool() const { return m_scene->m_registry.valid(m_enttEntity); }

        /**
         * @brief Check if entity has a component
         *
         * @tparam Components type
         * @return true if entity has component, false otherwise
         */
        template <typename... Components>
        bool has() {
            return m_scene->m_registry.all_of<Components...>(m_enttEntity);
        }

        /**
         * @brief Check if entity has any of the provided components
         *
         * @tparam Component type
         * @return true if entity has any of the provided components, false otherwise
         */
        template <typename... Components>
        bool hasAny() {
            return m_scene->m_registry.any_of<Components...>(m_enttEntity);
        }

        /**
         * @brief Get a component from entity
         *
         * @tparam Component type
         * @return Reference to component
         */
        template <typename Component>
        Component& get() {
            PXT_ASSERT(has<Component>(), "Entity does not have component");

            return m_scene->m_registry.get<Component>(m_enttEntity);
        }

        /**
         * @brief Add a component to entity
         *
         * Self referential method to allow chaining of add calls
         *
         * @tparam Component type
         * @return Reference to entity
         */
        template <typename Component, typename... Args>
        Entity& add(Args&&... args) {
            m_scene->m_registry.emplace<Component>(m_enttEntity, std::forward<Args>(args)...);
            return *this;
        }

        /**
         * @brief Add a component to entity and return a reference to it
         *
         * @tparam Component type
         * @return Reference to component
         */
        template <typename Component, typename... Args>
        Component& addAndGet(Args&&... args) {
            return m_scene->m_registry.emplace<Component>(m_enttEntity, std::forward<Args>(args)...);
        }

        /**
         * @brief Remove a component from entity
         *
         * @tparam Component type
         */
        template <typename Component>
        void remove() {
            PXT_STATIC_ASSERT((!std::is_same_v<Component, IDComponent>), "Cannot remove ID component");
            PXT_ASSERT(has<Component>(), "Entity does not have component");

            m_scene->m_registry.remove<Component>(m_enttEntity);
        }

        /**
         * @brief Get the UUID of the entity
         *
         * @return UUID of the entity
         */
        core::UUID getUUID() { return get<IDComponent>().uuid; }

        /**
         * @brief Get the name of the entity
         *
         * @return name of the entity
         */
        std::string getName() { return get<NameComponent>().name; }

        /**
         * @brief Get the Object Picking ID of the entity
         *
         * @return Object Picking ID of the entity
         */
        uint32_t getObjPickingId() { return get<ObjPickingIdComponent>().objPickingId.getObjPickingId(); }

        std::optional<std::reference_wrapper<Scene>> tryGetScene() {
            if (!m_scene) {
                return std::nullopt;
            }
            return std::ref(*m_scene);
        }

    private:
        entt::entity m_enttEntity{entt::null};
        Scene* m_scene = nullptr;
    };

} // namespace pxt
