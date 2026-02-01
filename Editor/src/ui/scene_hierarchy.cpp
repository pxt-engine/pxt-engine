#include "ui/scene_hierarchy.hpp"
#include "core/events/editor_events.hpp"
#include "ui/widgets/toggle_button.hpp"

#include "ui/icons_lucide.h"

namespace pxt::editor {
    void SceneHierarchy::onUpdateUi(FrameInfo& frameInfo, core::UID& selectedEntityId,
                                    const EditorTextureRegistry* const editorTextureRegistry) {
        drawSceneEntityList(frameInfo.scene, selectedEntityId, editorTextureRegistry);
    }

    void SceneHierarchy::drawSceneEntityList(Scene& scene, core::UID& selectedEntityId,
                                             const EditorTextureRegistry* const editorTextureRegistry) {
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
            Entity entity{entityHandle, &scene};
            const auto& [idComponent, nameComponent] = view.get<IDComponent, NameComponent>(entityHandle);
            bool selected = (selectedEntityId == idComponent.uid);

            // unique ID scope for this row's interactions (little trick: we cast uint64_t to a pointer since they are
            // the same size)
            ImGui::PushID((void*)(uintptr_t)idComponent.uid);

            // calculate dimensions of the line
            float availableWidth = ImGui::GetContentRegionAvail().x;
            float iconSize = ImGui::GetTextLineHeight(); // Height of text
            float padding = 5.0f;
            float totalIconArea = (iconSize * 2) + padding; // Space for 2 icons + gap

            // render the Selectable.
            // AllowOverlap flag so that icons drawn on top of it can be clicked
            if (ImGui::Selectable(nameComponent.name.c_str(), selected, ImGuiSelectableFlags_AllowOverlap,
                                  ImVec2(availableWidth, 0))) {
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

            // right-align the icons
            // move cursor back to the same line, at the far right minus the icon area
            ImGui::SameLine(availableWidth - totalIconArea);

            auto& propertiesComp = entity.get<PropertiesComponent>();

            // we want zero padding inside icons
            ImVec2 iconInnerPadding = ImVec2(0, 0);
            // we also want them to have no color, just on hover a little brightening
            ImVec4 invisibleColor = ImVec4(0.f, 0.f, 0.f, 0.f);
            ImVec4 hoveredColor = ImVec4(0.3f, 0.3f, 0.3f, 0.3f);

            // Eye icon (VisibilityTag Component) ---
            bool isVisible = propertiesComp.isEditorVisible;

            const char* eyeIconTooltip = "Hides this entity in the editor viewport";
            if (ui::ToggleButton::icon(ICON_LC_EYE, ICON_LC_EYE_CLOSED, "eye-", eyeIconTooltip, true, false, isVisible,
                                       ImVec2(iconSize, iconSize), invisibleColor, invisibleColor)) {
                // Toggle Visibility Logic
                if (isVisible) {
                    entity.update<PropertiesComponent>([](auto& propComp) { propComp.isEditorVisible = true; });
                } else {
                    entity.update<PropertiesComponent>([](auto& propComp) { propComp.isEditorVisible = false; });
                }
            }

            ImGui::SameLine();

            // Renderable icon (RenderableTag Component) ---
            bool isRenderable = propertiesComp.isRenderable;

            const char* renderableIconTooltip = "Hides this entity in the final render";
            if (ui::ToggleButton::icon(ICON_LC_CAMERA, ICON_LC_CAMERA_OFF, "renderable-", renderableIconTooltip, true,
                                       false, isRenderable, ImVec2(iconSize, iconSize), invisibleColor,
                                       invisibleColor)) {
                // Renderable Toggle Logic
                if (isRenderable) {
                    entity.update<PropertiesComponent>([](auto& propComp) { propComp.isRenderable = true; });
                } else {
                    entity.update<PropertiesComponent>([](auto& propComp) { propComp.isRenderable = false; });
                }
            }

            ImGui::PopID();
        }

        // Execute Actions
        if (entityToRemove != core::UID::s_invalidId) {
            scene.destroyEntity(entityToRemove);
            if (selectedEntityId == entityToRemove)
                selectedEntityId = core::UID::s_invalidId;
        }

        if (entityToDuplicate != core::UID::s_invalidId) {
            Entity duplicatedEntity = scene.duplicateEntity(entityToDuplicate);
            selectedEntityId = duplicatedEntity.getUID();
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