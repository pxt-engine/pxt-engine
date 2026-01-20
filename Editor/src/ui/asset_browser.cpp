#include "ui/asset_browser.hpp"
#include "ui/drag_and_drop.hpp"

namespace pxt::editor {
    void AssetBrowser::onUpdateUi(ResourceManager& rm) {
        ImGui::Begin("Asset Browser");

        static ImGuiTextFilter simpleFilter;
        simpleFilter.Draw("Search");

        // iterate through data provided by resource manager
        for (auto& [uuid, resource] : rm.getAllResources()) {
            if (simpleFilter.PassFilter(resource->alias.c_str())) {
                if (ImGui::Selectable(resource->alias.c_str(), m_selectedResource == uuid)) {
                    m_selectedResource = uuid;
                }

                // check if the item is being dragged
                DragAndDrop::EnginePayload payload = {
                    uuid, 
                    DragAndDrop::PayloadSource::AssetBrowser,
                    resource->getType()
                };

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
}