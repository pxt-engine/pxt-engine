#pragma once

#include "editor_texture_registry.hpp"
#include "pxtengine.h"
#include "undo/undo_stack.hpp"

namespace pxt::editor {
    class SceneHierarchy {
    public:
        SceneHierarchy(UndoStack& undoStack) : m_undoStack{undoStack} {}

        ~SceneHierarchy() = default;

        void onUpdateUi(FrameInfo& frameInfo, core::UID& selectedEntityId,
                        const EditorTextureRegistry* const editorTextureRegistry);
        void drawSceneEntityList(Scene& scene, core::UID& selectedEntityId,
                                 const EditorTextureRegistry* const editorTextureRegistry);

    private:
        UndoStack& m_undoStack;
    };
} // namespace pxt::editor
