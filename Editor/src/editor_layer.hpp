#pragma once

// Precopiled header
#include "core/pch.hpp"

// Engine headers
#include "core/engine_mode.hpp"
#include "core/events/event.hpp"
#include "core/input/input.hpp"
#include "core/layer/layer.hpp"
#include "core/uid.hpp"
#include "graphics/frame_info.hpp"

// Editor headers
#include "camera_nav_state.hpp"
#include "editor_camera_controller.hpp"
#include "editor_texture_registry.hpp"
#include "editor_view_provider.hpp"
#include "game_view_provider.hpp"
#include "ui/asset_browser.hpp"
#include "ui/editor_console.hpp"
#include "ui/entity_inspector.hpp"
#include "ui/environment.hpp"
#include "ui/main_menu_bar.hpp"
#include "ui/scene_hierarchy.hpp"
#include "undo/undo_stack.hpp"

#include "editor_logger_sink.hpp"

#include <ImGuizmo.h>

namespace pxt::editor {
    class EditorLayer : public core::Layer {
    public:
        explicit EditorLayer();
        ~EditorLayer();

        void onBeginFrame(float deltaTime) override;
        void onEvent(core::Event& event) override;
        void onUpdateUi(FrameInfo& frameInfo) override;
        void onPostFrameUpdate(FrameInfo& frameInfo) override;

    private:
        void updateSceneUi(FrameInfo& frameInfo);
        void updateGizmos(FrameInfo& frameInfo);
        void updateViewportOverlayButtons(FrameInfo& frameInfo, float buttonsScale = 0.1f);
        void updateGizmoOverlayButtons(ImGuiWindowFlags windowFlags, float padding, ImVec2 buttonSize);
        core::EngineMode updatePlayPauseButton(ImGuiWindowFlags windowFlags, float padding, ImVec2 buttonSize);

        void buildCommandExecutionContext(FrameInfo* const prevFrameInfo);
        void buildCameraNavigationState();
        void checkUndoRedoInputs();

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

        // this is just a way to not write "core::Input::getState()" everywhere, since we need to read input state in
        // multiple places in the editor layer and also update it
        core::InputState& m_inputState = core::Input::getState();

        glm::vec2 m_lastClickMousePosImGui = {0.0f, 0.0f};

        ExecutionContext m_commandExecutionContext;
        UndoStack m_undoStack{};
        EditorConsole m_editorConsole{};
        SceneHierarchy m_sceneHierarchy{m_undoStack};
        EntityInspector m_entityInspector{};
        MainMenuBar m_mainMenuBar{};
        AssetBrowser m_assetBrowser{};
        EnvironmentUi m_environmentUi{};

        Shared<EditorLoggerSink> m_editorLoggerSink = nullptr;

        core::UID m_selectedEntityUID = core::UID::s_invalidId;
        core::UID m_prevSelectedEntityUID = core::UID::s_invalidId;

        ImVec2 m_sceneImageExtent{0.f, 0.f};
        ImVec2 m_viewportUpperLeftScreenCoord{0.f, 0.f};
        bool m_forceViewportFocus = false;

        ImGuizmo::OPERATION m_currentGizmoOperation{ImGuizmo::TRANSLATE};
        ImGuizmo::MODE m_currentGizmoMode{ImGuizmo::WORLD};
    };
} // namespace pxt::editor