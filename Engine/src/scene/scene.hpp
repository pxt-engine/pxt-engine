#pragma once

#include "core/events/event.hpp"
#include "core/obj_picking_id.hpp"
#include "core/pch.hpp"
#include "core/uuid.hpp"

#include "scene/environment.hpp"

namespace pxt {

    class Entity;

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
        ~Scene() = default;

        std::string getName() const { return m_name; }

        void setName(std::string name) { m_name = name; }

        /**
         * @brief Creates a new entity in the scene.
         * @param name Optional name for the entity.
         * @param id Optional UUID for the entity. If not provided, a new UUID is generated.
         * @return The created entity.
         */
        Entity createEntity(const std::string& name = std::string(), core::UUID id = core::UUID(),
                            core::ObjPickingId objPickingId = core::ObjPickingId());

        /**
         * @brief Retrieves an entity by its UUID.
         * @param UUID The UUID of the entity.
         * @return The corresponding entity.
         */
        Entity getEntity(core::UUID uuid);

        /**
         * @brief Retrieves the UUID of an entity based on its object picking ID.
         * @param objPickingId The object picking ID.
         * @return The UUID of the corresponding entity.
         */
        core::UUID getEntityUUIDFromObjPickingId(uint32_t objPickingId);

        /**
         * @brief Retrieves the object picking ID of an entity based on its UUID.
         * @param uuid The UUID of the entity.
         * @return The object picking ID of the corresponding entity.
         */
        uint32_t getObjPickingIdFromEntityUUID(core::UUID uuid);

        /**
         * @brief Destroys an entity and removes it from the scene.
         * @param uuid The uuid of the entity to be destroyed.
         */
        void destroyEntity(core::UUID uuid);

        /**
         * @brief Copy an entity and adds the copy to the scene.
         * @param uuid The uuid of the entity to copy.
         * @return The copied entity.
         */
        Entity duplicateEntity(core::UUID uuid);

        /**
         * @brief Called when the scene starts.
         *
         * Initializes scripts attached to entities.
         */
        void onStart();

        /**
         * @brief Called every frame to update the scene.
         * @param delta Time elapsed since the last update.
         */
        void onUpdate(float delta);

        void onEvent(core::Event& event);

        /**
         * @brief Retrieves all entities that have the specified components.
         * @tparam T Component types to filter entities.
         * @return A view of the entities with the specified components.
         */
        template <typename... T>
        auto getEntitiesWith() {
            return m_registry.view<T...>();
        }

        /**
         * @brief Gets the entity designated as the main camera.
         * @return The main camera entity or an empty entity if none exist.
         */
        std::optional<Entity> getActiveCameraEntity();

        core::UUID getActiveCameraEntityUUID();

        void setActiveCameraEntity(core::UUID newActiveCameraID);

        void updateCamerasAspectRatio(float newAspect);

        std::optional<Entity> tryFindCamera();

        /**
         * @brief Retrieves the environment settings for the scene.
         * @return A shared pointer to the environment settings.
         */
        Shared<Environment> getEnvironment() const { return m_environment; }

    private:
        std::string getUniqueEntityName(const std::string& baseName);

        std::string m_name = "Unnamed-Scene";
        std::unordered_map<core::UUID, entt::entity> m_entityMap;
        std::unordered_map<uint32_t, core::UUID> m_objPickingIdToUUID;
        std::unordered_map<core::UUID, uint32_t> m_uuidToObjPickingId;

        // The entity registry for managing components.
        entt::registry m_registry;

        Shared<Environment> m_environment = createShared<Environment>();
        core::UUID m_activeCameraEntityID = core::UUID::s_invalidId;

        friend class Entity;
    };
} // namespace pxt
