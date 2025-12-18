#pragma once

#include "pxtengine.h"

namespace pxt::editor {
    class MainMenuBar {
    public:
        MainMenuBar() = default;
        ~MainMenuBar() = default;
        void onUpdateUi(FrameInfo& frameInfo);

    private:
        void saveSceneModal(Scene& scene);

        bool m_openSaveSceneDialog = false;
    };
} // namespace pxt::editor