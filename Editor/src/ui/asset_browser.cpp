#include "ui/asset_browser.hpp"
#include "ui/drag_and_drop.hpp"

#include "ui/icons_lucide.h"

namespace pxt::editor {
    void AssetBrowser::onUpdateUi(ResourceManager& rm) {
        ImGui::Begin("Asset Browser");

        static ImGuiTextFilter simpleFilter;
        simpleFilter.Draw(ICON_LC_SEARCH " Search");

        // iterate through data provided by resource manager
        for (auto& [uid, resource] : rm.getAllResources()) {
            if (simpleFilter.PassFilter(resource->alias.c_str())) {
                if (ImGui::Selectable(resource->alias.c_str(), m_selectedResource == uid)) {
                    m_selectedResource = uid;
                }

                // check if the item is being dragged
                DragAndDrop::EnginePayload payload = {uid, DragAndDrop::PayloadSource::AssetBrowser,
                                                      resource->getType()};

                DragAndDrop::dragDropSource(payload, resource->alias);
            }
        }

        // TODO: move import ui logic here!!!
        /*
         // handle import
        if (ImGui::Button("Import New...")) {
            m_showImportModal = true;
        }

        if (m_showImportModal) {
            drawImportModal(rm);
        }

        */

        ImGui::End();
    }
} // namespace pxt::editor