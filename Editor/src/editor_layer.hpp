#pragma once

#include "camera_nav_state.hpp"
#include "core/engine_mode.hpp"
#include "editor_camera_controller.hpp"
#include "editor_texture_registry.hpp"
#include "editor_view_provider.hpp"
#include "game_view_provider.hpp"
#include "pxtengine.h"

#include "editor_texture_registry.hpp"
#include "ui/entity_inspector.hpp"
#include "ui/main_menu_bar.hpp"
#include "ui/scene_hierarchy.hpp"

#include <ImGuizmo.h>

namespace pxt::editor {
    class EditorLayer : public core::Layer {
    public:
        explicit EditorLayer();

        void onBeginFrame(float deltaTime) override;
        void onEvent(core::Event& event) override;
        void onUpdateUi(FrameInfo& frameInfo) override;

    private:
        void updateSceneUi(FrameInfo& frameInfo);
        void updateGizmos(FrameInfo& frameInfo);
        void updateViewportOverlayButtons(FrameInfo& frameInfo, float buttonsScale = 0.1f);
        void updateGizmoOverlayButtons(ImGuiWindowFlags windowFlags, float padding, ImVec2 buttonSize);
        core::EngineMode updatePlayPauseButton(ImGuiWindowFlags windowFlags, float padding, ImVec2 buttonSize);

        void buildCameraNavigationState();

        ImVec2 getImageSizeWithAspectRatioForImGuiWindow(ImVec2 windowSize, float aspectRatio);
        bool onMouseButtonPress(core::MouseButtonPressEvent& event);
        bool onLeftMouseButtonPress();
        bool doMousePicking();
        bool onKeyPressEvent(core::KeyPressEvent& event);

        const float getViewportAspectRatio() const { return m_sceneImageExtent.x / m_sceneImageExtent.y; };

        core::EngineMode m_engineMode = core::EngineMode::EDIT;

        Unique<EditorTextureRegistry> m_editorTextureRegistry = nullptr;

        CameraNavigationState m_navigationState{};
        CameraData m_editorCameraData{};
        glm::vec3 m_editorCameraRotation{0.f};
        glm::vec3 m_editorCameraPosition{0.f};

        EditorViewProvider m_editorViewProvider{EditorCameraController(), m_editorCameraPosition,
                                                m_editorCameraRotation};
        GameViewProvider m_gameViewProvider;

        core::InputState& m_inputState = core::Input::getState();

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