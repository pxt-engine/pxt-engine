#pragma once

#include "core/filesystem.hpp"
#include "core/uid.hpp"
#include "resources/resource.hpp"

namespace pxt::editor {
    static const char* UNIVERSAL_PAYLOAD_ID = "UNIVERSAL_PAYLOAD_ID";

    struct DragAndDrop {

        enum class PayloadSource { AssetBrowser };

        struct EnginePayload {
            core::UID id;        // The UID
            PayloadSource source; // Where it started
            Resource::Type type;  // What it is
        };

        /*
         *@brief Begin a drag-and-drop source with the given payload and preview text.
         *
         * @param payload The payload to be dragged.
         * @param previewText The text to display as a preview during the drag operation.
         */
        static void dragDropSource(const EnginePayload& payload, const std::string& previewText) {
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload(UNIVERSAL_PAYLOAD_ID, &payload, sizeof(EnginePayload));

                // Visual feedback
                ImGui::TextUnformatted(previewText.c_str());
                ImGui::EndDragDropSource();
            }
        }

        /*
         *@brief Accept a drag-and-drop payload and validate its type and source.
         *
         * @param outPayload Reference to store the accepted payload.
         * @param expectedType The expected resource type of the payload.
         * @param expectedSource The expected source of the payload.
         *
         * @return true if a valid payload was accepted; false otherwise.
         */
        static bool dragDropTarget(EnginePayload& outPayload, Resource::Type expectedType,
                                   PayloadSource expectedSource) {
            if (ImGui::BeginDragDropTarget()) {
                // peek at the payload without accepting it yet
                if (const ImGuiPayload* payload =
                        ImGui::AcceptDragDropPayload(UNIVERSAL_PAYLOAD_ID, ImGuiDragDropFlags_AcceptBeforeDelivery)) {
                    EnginePayload* data = (EnginePayload*)payload->Data;

                    // perform the validation logic
                    bool isValid = data->type == expectedType && data->source == expectedSource;
                    if (isValid) {
                        // when the user drops the item
                        if (payload->IsDelivery()) {
                            outPayload = *data;
                            ImGui::EndDragDropTarget();
                            return true;
                        }
                    } else {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_NotAllowed);

                        ImGui::EndDragDropTarget();
                        return false;
                    }
                }

                // this is needed in case the payload was accepted but not delivered yet
                ImGui::EndDragDropTarget();
                return false;
            }

            return false;
        }
    };
} // namespace pxt::editor