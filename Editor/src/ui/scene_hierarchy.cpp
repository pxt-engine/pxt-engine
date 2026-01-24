#include "ui/scene_hierarchy.hpp"
#include "core/events/editor_events.hpp"

namespace pxt::editor {
    void SceneHierarchy::onUpdateUi(FrameInfo& frameInfo, core::UID& selectedEntityId) {
        drawSceneEntityList(frameInfo.scene, selectedEntityId);
    }

    void SceneHierarchy::drawSceneEntityList(Scene& scene, core::UID& selectedEntityId) {
        ImGui::Begin("Scene Entities");

        if (ImGui::Button("Add Entity")) {
            scene.createEntity("New Entity");
        }

        ImGui::Separator();

        auto view = scene.getEntitiesWith<IDComponent, NameComponent>();

        // Track if an entity needs to be deleted or duplicated outside the loop
        // to avoid iterator invalidation issues while iterating the view
        core::UID entityToRemove = core::UID::s_invalidId;
        core::UID entityToDuplicate = core::UID::s_invalidId;

        for (auto entityHandle : view) {
            const auto& [idComponent, nameComponent] = view.get<IDComponent, NameComponent>(entityHandle);
            bool selected = (selectedEntityId == idComponent.uid);

            if (ImGui::Selectable(nameComponent.name.c_str(), selected)) {
                selectedEntityId = idComponent.uid;
            }

            // Popup menu on right click
            if (ImGui::BeginPopupContextItem()) {
                selectedEntityId = idComponent.uid;

                if (ImGui::MenuItem("Copy Entity")) {
                    entityToDuplicate = idComponent.uid;
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Remove Entity")) {
                    entityToRemove = idComponent.uid;
                }

                ImGui::EndPopup();
            }
        }

        // Execute Actions
        if (entityToRemove != core::UID::s_invalidId) {
            scene.destroyEntity(entityToRemove);
            if (selectedEntityId == entityToRemove)
                selectedEntityId = core::UID::s_invalidId;
        }

        if (entityToDuplicate != core::UID::s_invalidId) {
            scene.duplicateEntity(entityToDuplicate);
        }

        // deselect if background clicked
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (!ImGui::IsAnyItemHovered()) {
                selectedEntityId = core::UID::s_invalidId;
            }
        }

        ImGui::End();
    }
} // namespace pxt::editor