#pragma once

#include "pxtengine.h"

namespace pxt::editor {
    class SceneHierarchy {
    public:
        SceneHierarchy() = default;
        ~SceneHierarchy() = default;

        void onUpdateUi(FrameInfo& frameInfo, core::UID& selectedEntityId);
        void drawSceneEntityList(Scene& scene, core::UID& selectedEntityId);
    };
} // namespace pxt::editor
