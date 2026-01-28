#pragma once

#include "pxtengine.h"
#include "ui/drag_and_drop.hpp"

namespace pxt::editor {
    class ResourceSlot {
    public:
        static bool render(const char* label, DragAndDrop::EnginePayload& outPayload, ResourceManager& rm) {
            bool changed = false;

            // get the name of the current asset for the button label
            std::string buttonText = "None (Empty)";
            if (outPayload.id != core::UID::s_invalidId) {
                if (Shared<Resource> resource = rm.get(outPayload.id)) {
                    buttonText = "Drop here: " + resource->alias;
                }
            }

            ImGui::PushID(label); // Prevent ID conflicts if using multiple slots

            // A square-ish button to act as the "Target Icon"
            if (ImGui::Button(buttonText.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                ImGui::OpenPopup("AssetSelectionPopup");
            }

            // handle Drag and Drop Target
            if (DragAndDrop::dragDropTarget(outPayload, outPayload.type, outPayload.source)) {
                changed = true;
            }

            // handle Manual Selection Popup
            if (ImGui::BeginPopup("AssetSelectionPopup")) {
                static ImGuiTextFilter filter;

                filter.Draw("Search");

                if (ImGui::Selectable("Clear Slot")) {
                    outPayload.id = core::UID::s_invalidId;
                    changed = true;
                }

                // List all resources of the specified type
                for (auto& resource : rm.getResourcesByType(outPayload.type)) {
                    // Filter by name and type
                    if (filter.PassFilter(resource->alias.c_str())) {
                        if (ImGui::Selectable(resource->alias.c_str(), outPayload.id == resource->id)) {
                            outPayload.id = resource->id;
                            changed = true;
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }

                ImGui::EndPopup();
            }

            ImGui::PopID();
            return changed;
        }
    };
} // namespace pxt::editor