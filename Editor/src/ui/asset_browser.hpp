#pragma once

#include "pxtengine.h"

namespace pxt::editor {
    class AssetBrowser {
    public:
        void onUpdateUi(ResourceManager& rm);

    private:
        std::string m_searchFilter = "";
        core::UID m_selectedResource = core::UID::s_invalidId;
    };
}