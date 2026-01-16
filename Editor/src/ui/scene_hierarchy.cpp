#include "ui/scene_hierarchy.hpp"
#include "core/events/editor_events.hpp"

namespace pxt::editor {
    void SceneHierarchy::onUpdateUi(FrameInfo& frameInfo, core::UUID& selectedEntityId) {
        drawSceneEntityList(frameInfo.scene, selectedEntityId);
    }

    void SceneHierarchy::drawSceneEntityList(Scene& scene, core::UUID& selectedEntityId) {
        ImGui::Begin("Scene Entities");

        if (ImGui::Button("Add Entity")) {
            scene.createEntity("New Entity");
        }

        ImGui::Separator();

        auto view = scene.getEntitiesWith<IDComponent, NameComponent>();

        // Track if an entity needs to be deleted or duplicated outside the loop
        // to avoid iterator invalidation issues while iterating the view
        core::UUID entityToRemove = core::UUID::s_invalidId;
        core::UUID entityToDuplicate = core::UUID::s_invalidId;

        for (auto entityHandle : view) {
            const auto& [idComponent, nameComponent] = view.get<IDComponent, NameComponent>(entityHandle);
            bool selected = (selectedEntityId == idComponent.uuid);

            if (ImGui::Selectable(nameComponent.name.c_str(), selected)) {
                selectedEntityId = idComponent.uuid;
            }

            // Popup menu on right click
            if (ImGui::BeginPopupContextItem()) {
                selectedEntityId = idComponent.uuid;

                if (ImGui::MenuItem("Copy Entity")) {
                    entityToDuplicate = idComponent.uuid;
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Remove Entity")) {
                    entityToRemove = idComponent.uuid;
                }

                ImGui::EndPopup();
            }
        }

        // Execute Actions
        if (entityToRemove != core::UUID::s_invalidId) {
            scene.destroyEntity(entityToRemove);
            if (selectedEntityId == entityToRemove)
                selectedEntityId = core::UUID::s_invalidId;
        }

        if (entityToDuplicate != core::UUID::s_invalidId) {
            scene.duplicateEntity(entityToDuplicate);
        }

        // deselect if background clicked
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (!ImGui::IsAnyItemHovered()) {
                selectedEntityId = core::UUID::s_invalidId;
            }
        }

        ImGui::End();
    }
} // namespace pxt::editor