#pragma once

#include "pxtengine.h"

namespace pxt::editor {
    class AssetBrowser {
    public:
        void onUpdateUi(ResourceManager& rm);

    private:
        std::string m_searchFilter = "";
        core::UUID m_selectedResource = core::UUID::s_invalidId;
    };
}