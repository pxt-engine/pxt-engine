#pragma once

#include "pxtengine.h"
#include "editor_texture_registry.hpp"

namespace pxt::editor {
    class SceneHierarchy {
    public:
        SceneHierarchy() = default;
        ~SceneHierarchy() = default;

        void onUpdateUi(FrameInfo& frameInfo, core::UID& selectedEntityId,
                        const EditorTextureRegistry* const editorTextureRegistry);
        void drawSceneEntityList(Scene& scene, core::UID& selectedEntityId,
                                 const EditorTextureRegistry* const editorTextureRegistry);
    };
} // namespace pxt::editor
