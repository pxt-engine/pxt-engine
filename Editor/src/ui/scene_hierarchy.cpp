#include "ui/scene_hierarchy.hpp"
#include "core/events/editor_events.hpp"
#include "ui/widgets/toggle_image_button.hpp"

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
            float iconSize = 0.58 * ImGui::GetTextLineHeight(); // Height of text
            float padding = 5.0f;
            float totalIconArea = (iconSize * 2) + (padding * 3); // Space for 2 icons + 3 gaps

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

            // Eye icon (VisibilityTag Component) ---
            bool isVisible = entity.has<VisibilityTag>();
            std::string eyeIconFile = isVisible ? "eye_icon.png" : "eye_slash_icon.png";

            // TODO: these two icon have to also check for entity validity, probably best if we have some preferences
            //  for eachentity inside a component or something and perform a validity check only when needed, not here
            ImTextureID eyeIcon = (ImTextureID)editorTextureRegistry->get(eyeIconFile);
            const char* eyeIconTooltip = "Hides this entity in the editor viewport";
            if (ui::ToggleImageButton::render(eyeIcon, "##eye-", eyeIconTooltip, true, false, isVisible,
                                              ImVec2(iconSize, iconSize))) {
                // Toggle Visibility Logic
                if (isVisible) {
                    entity.add<VisibilityTag>();
                } else {
                    entity.remove<VisibilityTag>();
                }
            }

            ImGui::SameLine();

            // Renderable icon (RenderableTag Component) ---
            bool isRenderable = entity.has<RenderableTag>();
            std::string cameraIconFile = isRenderable ? "camera_icon.png" : "camera_slash_icon.png";

            ImTextureID renderableIcon = (ImTextureID)editorTextureRegistry->get(cameraIconFile);
            const char* renderableIconTooltip = "Hides this entity in the final render";
            if (ui::ToggleImageButton::render(renderableIcon, "##renderable-", renderableIconTooltip, true, false,
                                              isRenderable, ImVec2(iconSize, iconSize))) {
                // Renderable Toggle Logic
                if (isRenderable) {
                    entity.add<RenderableTag>();
                } else {
                    entity.remove<RenderableTag>();
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