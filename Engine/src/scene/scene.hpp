#pragma once

#include "core/events/event.hpp"
#include "core/obj_picking_id.hpp"
#include "core/pch.hpp"
#include "core/uid.hpp"
#include "scene/ecs/component.hpp"

#include "scene/environment.hpp"

namespace pxt {

    class Entity;
    class EntityLifecycleSystem;

    /**
     * @class Scene
     * @brief Manages a collection of entities and their components.
     *
     * This class serves as a container for entities and provides functionality for entity creation,
     * retrieval, and destruction. It also manages entity updates and scripting behavior.
     */
    class Scene {
    public:
        Scene();
        ~Scene();

        std::string getName() const { return m_name; }

        void setName(std::string name) { m_name = name; }

        /**
         * @brief Creates a new entity in the scene.
         * @param name Optional name for the entity.
         * @param id Optional UID for the entity. If not provided, a new UID is generated.
         * @return The created entity.
         */
        Entity createEntity(const std::string& name = std::string(), core::UID id = core::UID(),
                            core::ObjPickingId objPickingId = core::ObjPickingId());

        /**
         * @brief Retrieves an entity by its UID.
         * @param UID The UID of the entity.
         * @return The corresponding entity.
         */
        Entity getEntity(core::UID uid);

        /**
         * @brief Retrieves the UID of an entity based on its object picking ID.
         * @param objPickingId The object picking ID.
         * @return The UID of the corresponding entity.
         */
        core::UID getEntityUIDFromObjPickingId(uint32_t objPickingId);

        /**
         * @brief Retrieves the object picking ID of an entity based on its UID.
         * @param uid The UID of the entity.
         * @return The object picking ID of the corresponding entity.
         */
        uint32_t getObjPickingIdFromEntityUID(core::UID uid);

        /**
         * @brief Destroys an entity and removes it from the scene.
         * @param uid The uid of the entity to be destroyed.
         */
        void destroyEntity(core::UID uid);

        /**
         * @brief Copy an entity and adds the copy to the scene.
         * @param uid The uid of the entity to copy.
         * @param copyUid Optional UID for the copied entity. If not provided, a new UID is generated.
         * @return The copied entity.
         */
        Entity duplicateEntity(core::UID uid, core::UID copyUid = core::UID());

        /**
         * @brief Called when the scene starts.
         *
         * Initializes scripts attached to entities.
         */
        void onStart();

        /**
         * @brief Update a component of an entity using a provided function.
         *
         * @tparam Component type
         * @tparam Func function type
         * @param entity The entity whose component is to be updated.
         * @param func The function to apply to the component.
         */
        template <typename Component, typename Func>
        void updateComponent(entt::entity entity, Func&& func) {
            // .patch<Component> takes the entity and one or more functions
            // to apply to the component, and then signals the listeners of the update
            m_registry.patch<Component>(entity, std::forward<Func>(func));
        }

        /**
         * @brief Called every frame to update the scene.
         * @param delta Time elapsed since the last update.
         */
        void onUpdate(float delta);

        void onEvent(core::Event& event);

        template <typename... T>
        auto getEntitiesWith() {
            return m_registry.view<T...>();
        }

        /**
         * @brief Retrieves all entities that have the specified components.
         * @tparam T Component types to filter entities.
         * @return A view of the entities with the specified components.
         */
        template <typename... T>
        auto getEntitiesWith(ComponentList<T...>) {
            return m_registry.view<T...>();
        }

        /**
         * @brief Gets the entity designated as the main camera.
         * @return The main camera entity or an empty entity if none exist.
         */
        std::optional<Entity> getActiveCameraEntity();

        core::UID getActiveCameraEntityUID();

        void setActiveCameraEntity(core::UID newActiveCameraID);

        void updateCamerasAspectRatio(float newAspect);

        std::optional<Entity> tryFindCamera();

        /**
         * @brief Retrieves the environment settings for the scene.
         * @return A shared pointer to the environment settings.
         */
        Shared<Environment> getEnvironment() const { return m_environment; }

        std::string getUniqueEntityName(const std::string& baseName);

    protected:
        entt::registry& getRegistry() { return m_registry; };

    private:
        std::string m_name = "Unnamed-Scene";
        std::unordered_map<core::UID, entt::entity> m_entityMap;
        std::unordered_map<uint32_t, core::UID> m_objPickingIdToUID;
        std::unordered_map<core::UID, uint32_t> m_uidToObjPickingId;

        // The entity registry for managing components.
        entt::registry m_registry;

        Unique<EntityLifecycleSystem> m_entityLifecycleSystem;

        Shared<Environment> m_environment = createShared<Environment>();
        core::UID m_activeCameraEntityID = core::UID::s_invalidId;

        friend class Entity;
        friend class EntityLifecycleSystem;
    };
} // namespace pxt
