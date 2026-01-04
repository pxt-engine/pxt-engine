#pragma once

#include "editor_texture_registry.hpp"
#include "pxtengine.h"
#include "ui/entity_inspector.hpp"
#include "ui/main_menu_bar.hpp"
#include "ui/scene_hierarchy.hpp"
#include "editor_camera.hpp"
#include "camera_nav_state.hpp"

#include <ImGuizmo.h>

namespace pxt::editor {
    class EditorLayer : public core::Layer {
    public:
        EditorLayer();

        void onBeginFrame(float deltaTime) override;
        void onEvent(core::Event& event) override;
        void onUpdateUi(FrameInfo& frameInfo) override;

    private:
        void updateSceneUi(FrameInfo& frameInfo);
        void updateGizmos(FrameInfo& frameInfo);
        void updateViewportOverlayButtons(FrameInfo& frameInfo, float buttonsScale = 0.1f);

        ImVec2 getImageSizeWithAspectRatioForImGuiWindow(ImVec2 windowSize, float aspectRatio);
        bool onMouseButtonPress(core::MouseButtonPressEvent& event);
        bool onLeftMouseButtonPress();
        bool doMousePicking();
        bool onKeyPressEvent(core::KeyPressEvent& event);

        const float getViewportAspectRatio() const { return m_sceneImageExtent.x / m_sceneImageExtent.y; };

        Unique<EditorTextureRegistry> m_editorTextureRegistry = nullptr;

        Unique<EditorCamera> m_editorCamera = nullptr;
        CameraNavigationState m_navigationState{};

        bool m_isViewportFocused = false;
        bool m_isViewportHovered = false;

        // we need this to block certain events when interacting with buttons
        bool m_isAnyButtonHovered = false;

        glm::vec2 m_lastClickMousePosImGui = {0.0f, 0.0f};

        SceneHierarchy m_sceneHierarchy{};
        EntityInspector m_entityInspector{};
        MainMenuBar m_mainMenuBar{};

        core::UUID m_selectedEntityUUID = core::UUID::s_invalidId;
        core::UUID m_prevSelectedEntityUUID = core::UUID::s_invalidId;

        ImVec2 m_sceneImageExtent{0.f, 0.f};
        ImVec2 m_viewportUpperLeftScreenCoord{0.f, 0.f};

        ImGuizmo::OPERATION m_currentGizmoOperation{ImGuizmo::TRANSLATE};
        ImGuizmo::MODE m_currentGizmoMode{ImGuizmo::WORLD};
    };
} // namespace pxt::editor