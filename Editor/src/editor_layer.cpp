#include "editor_layer.hpp"

#include "core/events/editor_events.hpp"
#include "core/events/engine_state_events.hpp"
#include "editor_logger_sink.hpp"
#include "ui/widgets/dismissable_badge.hpp"
#include "ui/widgets/mode_selector_image_button.hpp"
#include "ui/widgets/toggle_image_button.hpp"

#include <glm/gtx/matrix_decompose.hpp> // will use it in the future for gizmos

#define DEFAULT_EDITOR_CAMERA_POS glm::vec3(1.565f, 1.702f, 4.293f);
#define DEFAULT_EDITOR_CAMERA_ROT glm::vec3(0.3825064f, 5.950683, 0.0f);

namespace pxt::editor {
    EditorLayer::EditorLayer() : core::Layer("EditorLayer") {
        core::Logger::registerSink(createShared<EditorLoggerSink>(m_editorConsole));

        m_editorTextureRegistry = createUnique<EditorTextureRegistry>();

        // we set up the editorViewProvider with the last saved camera view, if any
        std::optional<Entity> activeCameraEntityOpt = Application::get().getScene().getActiveCameraEntity();
        if (activeCameraEntityOpt.has_value()) {
            Entity activeCameraEntity = activeCameraEntityOpt.value();
            auto& transform = activeCameraEntity.get<TransformComponent>();

            m_editorCameraPosition = transform.translation;
            m_editorCameraRotation = transform.rotation;
        } else {
            // default view when opening editor
            m_editorCameraPosition = DEFAULT_EDITOR_CAMERA_POS;
            m_editorCameraRotation = DEFAULT_EDITOR_CAMERA_ROT;
        }

        m_editorViewProvider.setActiveCameraPosition(m_editorCameraPosition);
        m_editorViewProvider.setActiveCameraRotation(m_editorCameraRotation);

        Application::get().setViewProvider(&m_editorViewProvider);
    }

    void EditorLayer::onBeginFrame(float deltaTime) {
        // reset state
        m_navigationState = {};
        m_editorViewProvider.resetState();

        // here we have to obtain the corrext camera data based on engine mode
        // if we are not in EDIT mode, we simply return for now
        if (m_engineMode != core::EngineMode::EDIT) {
            return;
        }

        // update active camera information inside the provider
        m_editorViewProvider.updateActiveCamera(m_editorCameraData, m_editorCameraPosition, m_editorCameraRotation);

        // we want focus and hover to accept user input
        if (!m_inputState.isViewportFocused || !m_inputState.isViewportHovered) {
            return;
        }

        buildCameraNavigationState();

        m_editorViewProvider.setCameraNavigationState(m_navigationState);
        // here we update the active camera data with the editor controller
        m_editorViewProvider.onUpdateCameraController(deltaTime);
    }

    void EditorLayer::buildCameraNavigationState() {
        const auto& input = core::Input::getState();
        m_navigationState.freeLookEnabled =
            input.isMouseButtonDown(core::RightMouseButton) || input.isKeyDown(core::KeyCode::Space);

        m_navigationState.mouseDelta = input.getMouseDelta();
        m_navigationState.scrollDelta = input.getScrollDelta();

        m_navigationState.move = {
            (input.isKeyDown(core::KeyCode::D) ? 1.f : 0.f) - (input.isKeyDown(core::KeyCode::A) ? 1.f : 0.f),
            (input.isKeyDown(core::KeyCode::E) ? 1.f : 0.f) - (input.isKeyDown(core::KeyCode::Q) ? 1.f : 0.f),
            (input.isKeyDown(core::KeyCode::W) ? 1.f : 0.f) - (input.isKeyDown(core::KeyCode::S) ? 1.f : 0.f)};

        m_navigationState.rotate = {(input.isKeyDown(core::KeyCode::DownArrow) ? 1.f : 0.f) -
                                        (input.isKeyDown(core::KeyCode::UpArrow) ? 1.f : 0.f),
                                    (input.isKeyDown(core::KeyCode::RightArrow) ? 1.f : 0.f) -
                                        (input.isKeyDown(core::KeyCode::LeftArrow) ? 1.f : 0.f),
                                    (input.isKeyDown(core::KeyCode::Keypad4) ? 1.f : 0.f) -
                                        (input.isKeyDown(core::KeyCode::Keypad6) ? 1.f : 0.f)};
    }

    void EditorLayer::onEvent(core::Event& event) {
        core::EventDispatcher dispatcher(event);

        dispatcher.dispatch<core::SelectedEntityChangedEvent>([this](core::SelectedEntityChangedEvent& e) {
            m_selectedEntityUID = e.getSelectedEntityUID();
            return false;
        });

        dispatcher.dispatch<core::EngineModeChangedEvent>([this](core::EngineModeChangedEvent& e) {
            m_engineMode = e.getNewEngineMode();

            auto& engine = Application::get();
            switch (m_engineMode) {
            case (core::EngineMode::EDIT):
                engine.setViewProvider(&m_editorViewProvider);
                break;
            case (core::EngineMode::PLAY):
                engine.setViewProvider(&m_gameViewProvider);
                break;
            default:
                PXT_WARN("Editor received unsupported engine mode change event: {}",
                         core::engineModeToString(m_engineMode));
                break;
            }

            return false;
        });

        // I/O Events

        //! here we block i/o events if the viewport is not focused or we are not in EDIT mode
        if (!m_inputState.isViewportFocused || m_engineMode != core::EngineMode::EDIT) {
            return;
        }

        dispatcher.dispatch<core::MouseButtonPressEvent>([this](core::MouseButtonPressEvent& e) {
            // we dont care about mouse clicks outside of the viewport
            if (!m_inputState.isViewportHovered)
                return false;

            return onMouseButtonPress(e);
        });

        dispatcher.dispatch<core::KeyPressEvent>([this](core::KeyPressEvent& e) {
            // we return also if user is using free look mode
            if (!m_inputState.isViewportHovered || m_navigationState.freeLookEnabled)
                return false;
            return onKeyPressEvent(e);
        });
    }

    bool EditorLayer::onMouseButtonPress(core::MouseButtonPressEvent& event) {
        switch (event.getMouseButton()) {
        case core::MouseButton::Button0:
            return onLeftMouseButtonPress();
        default:
            break;
        }

        return false;
    }

    bool EditorLayer::onLeftMouseButtonPress() {
        // we do not want to interfere with other ui elements
        if (!m_inputState.isCursorOverUI) {
            return doMousePicking();
        }

        return false;
    }

    bool EditorLayer::doMousePicking() {
        // handle mouse button press events here
        m_lastClickMousePosImGui = core::Input::getState().getMousePositionImGui();

        float x = m_lastClickMousePosImGui.x - m_viewportUpperLeftScreenCoord.x;
        float y = m_lastClickMousePosImGui.y - m_viewportUpperLeftScreenCoord.y;

        uint32_t px = static_cast<uint32_t>(std::clamp(x, 0.0f, (float)(m_sceneImageExtent.x - 1)));
        uint32_t py = static_cast<uint32_t>(std::clamp(y, 0.0f, (float)(m_sceneImageExtent.y - 1)));

        // x PXT_INFO("Mouse Button Pressed at position: ({}, {}) (Realtive to upper-left corner of viewport)",
        // x	px, py);

        Application::get().queueEvent(core::PickObjectAtEvent(px, py));

        return false;
    }

    bool EditorLayer::onKeyPressEvent(core::KeyPressEvent& event) {
        // handle key press events here
        switch (event.getKeyCode()) {
        case core::KeyCode::W:
            m_currentGizmoOperation = ImGuizmo::TRANSLATE;
            break;
        case core::KeyCode::E:
            m_currentGizmoOperation = ImGuizmo::ROTATE;
            break;
        case core::KeyCode::R:
            m_currentGizmoOperation = ImGuizmo::SCALE;
            break;
        case core::KeyCode::Q:
            m_currentGizmoOperation = ImGuizmo::BOUNDS; // selection tool
            break;
        case core::KeyCode::T:
            if (m_currentGizmoMode == ImGuizmo::WORLD) {
                m_currentGizmoMode = ImGuizmo::LOCAL;
            } else {
                m_currentGizmoMode = ImGuizmo::WORLD;
            }
            break;
        default:
            // propagate event if key not handled
            return false;
            break;
        }
        return false;
    }

    void EditorLayer::onUpdateUi(FrameInfo& frameInfo) {
        ResourceManager& rm = Application::get().getResourceManager();

        // first update scene hierarchy ui (an entity might be selected)
        m_sceneHierarchy.onUpdateUi(frameInfo, m_selectedEntityUID, m_editorTextureRegistry.get());

        // only fire event if selection changed
        if (m_prevSelectedEntityUID != m_selectedEntityUID) {
            Application::get().queueEvent(core::SelectedEntityChangedEvent(m_selectedEntityUID));
            m_prevSelectedEntityUID = m_selectedEntityUID;
        }

        // then update entity inspector ui
        m_entityInspector.onUpdateUi(frameInfo, m_selectedEntityUID);

        // main menu bar
        m_mainMenuBar.onUpdateUi(frameInfo);

        m_editorConsole.onUpdateUi();

        m_assetBrowser.onUpdateUi(rm);

        //? maybe viewport class in the future?
        updateSceneUi(frameInfo);

        core::Input::getState().isViewportFocused = m_inputState.isViewportFocused;
        core::Input::getState().isViewportHovered = m_inputState.isViewportHovered;
        core::Input::getState().isCursorOverUI = m_inputState.isCursorOverUI;
    }

    void EditorLayer::updateSceneUi(FrameInfo& frameInfo) {
        m_inputState.isCursorOverUI = false;

        ImTextureID scene = (ImTextureID)frameInfo.sceneDescriptorSet;

        // we push a style var to remove the viewpoer window padding
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Viewport");

        m_inputState.isViewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_None);
        m_inputState.isViewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        m_viewportUpperLeftScreenCoord = ImGui::GetCursorScreenPos();

        ImVec2 viewportSize = ImGui::GetContentRegionAvail();

        // check if window size has changed
        if (viewportSize.x != m_sceneImageExtent.x || viewportSize.y != m_sceneImageExtent.y) {
            uint32_t width = static_cast<uint32_t>(std::max(1.0f, viewportSize.x));
            uint32_t height = static_cast<uint32_t>(std::max(1.0f, viewportSize.y));

            m_sceneImageExtent = ImVec2(static_cast<float>(width), static_cast<float>(height));

            Application::get().queueEvent(
                core::ViewportResizeEvent(static_cast<uint32_t>(width), static_cast<uint32_t>(height)));
        }

        ImGui::Image(scene, m_sceneImageExtent);

        // If nothing selected, selection tool is active, we are not in edit mode
        // or selected entity does not contain Transform Components: DO NOT show gizmos.
        // We currently use ImGuizmo::BOUNDS as "selection tool" placeholder
        bool canRenderGizmo =
            m_selectedEntityUID != core::UID::s_invalidId && m_currentGizmoOperation != ImGuizmo::BOUNDS &&
            m_engineMode == core::EngineMode::EDIT &&
            frameInfo.scene.getEntity(m_selectedEntityUID).hasAny<Transform2dComponent, TransformComponent>();

        if (canRenderGizmo) {
            // this has to be called inside the window where ImGuizmo is used
            updateGizmos(frameInfo);
        }

        updateViewportOverlayButtons(frameInfo, 0.065f);

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void EditorLayer::updateGizmos(FrameInfo& frameInfo) {
        // ImGuizmo::BeginFrame() is called right after ImGui::NewFrame() in UiRenderLayer

        ImGuizmo::SetDrawlist();

        ImGuizmo::SetRect(m_viewportUpperLeftScreenCoord.x, m_viewportUpperLeftScreenCoord.y, m_sceneImageExtent.x,
                          m_sceneImageExtent.y);

        ImGuizmo::SetGizmoSizeClipSpace(0.2f);
        ImGuizmo::SetOrthographic(false);
        // prevents the gizmo from "flipping" when looked from different angles
        // if flipped, an axis is decorated with black dots
        ImGuizmo::AllowAxisFlip(false);

        Entity selectedEntity = frameInfo.scene.getEntity(m_selectedEntityUID);
        TransformComponent& transform = selectedEntity.get<TransformComponent>();

        // copy - we need to modify them to adhere opengl standards (rh, y up)
        const glm::mat4& gizmoView = frameInfo.cameraMatrices.viewMatrix;
        glm::mat4 gizmoProj = frameInfo.cameraMatrices.projectionMatrix;
        gizmoProj[1][1] *= -1; // flip Y for ImGuizmo (it expects GL style projection matrix)

        glm::mat4 modelMatrix = transform.mat4();

        if (m_currentGizmoOperation == ImGuizmo::SCALE) {
            m_currentGizmoMode = ImGuizmo::LOCAL; // scale always in local mode
        }

        ImGuizmo::Manipulate(glm::value_ptr(gizmoView), glm::value_ptr(gizmoProj), m_currentGizmoOperation,
                             m_currentGizmoMode, glm::value_ptr(modelMatrix));

        // apply changes back to entity
        if (ImGuizmo::IsUsingAny()) {
            glm::vec3 translation, rotation, scale;

            ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(modelMatrix), glm::value_ptr(translation),
                                                  glm::value_ptr(rotation), glm::value_ptr(scale));
            transform.translation = translation;
            transform.rotation = glm::radians(rotation); // ImGuizmo returns degrees by default
            transform.scale = scale;
        }
    }

    void EditorLayer::updateGizmoOverlayButtons(ImGuiWindowFlags windowFlags, float padding, ImVec2 buttonSize) {
        // Top-right of viewport
        ImVec2 windowPos(m_viewportUpperLeftScreenCoord.x + m_sceneImageExtent.x - padding,
                         m_viewportUpperLeftScreenCoord.y + padding);

        ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::Begin("ViewportOverlayGizmoButtons", nullptr, windowFlags);

        // check for viewport focus/hover
        m_inputState.isViewportFocused |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        m_inputState.isViewportHovered |= ImGui::IsWindowHovered(ImGuiHoveredFlags_None);
        m_inputState.isCursorOverUI |= ImGui::IsWindowHovered(ImGuiHoveredFlags_None);

        ImTextureID selectIcon = (ImTextureID)m_editorTextureRegistry->get("selection_tool.png");

        ImTextureID translateIcon = (ImTextureID)m_editorTextureRegistry->get("translation_gizmo.png");
        ImTextureID scaleIcon = (ImTextureID)m_editorTextureRegistry->get("scale_gizmo.png");
        ImTextureID rotateIcon = (ImTextureID)m_editorTextureRegistry->get("rotation_gizmo.png");

        ImTextureID worldIcon = (ImTextureID)m_editorTextureRegistry->get("world_mode_gizmo.png");

        // TODO: Replace ImGuizmo::BOUNDS with a custom enum when new tools are added
        ui::ModeSelectorImageButton::render(selectIcon, "##selection-tool", "Selection Tool (Q)", ImGuizmo::BOUNDS,
                                            m_currentGizmoOperation, buttonSize);

        ImGui::SameLine(0.f, 10.f);

        ui::ModeSelectorImageButton::render(translateIcon, "##translate-gizmo", "Translate (W)", ImGuizmo::TRANSLATE,
                                            m_currentGizmoOperation, buttonSize);

        ImGui::SameLine(0.f, 0.f);
        ui::ModeSelectorImageButton::render(rotateIcon, "##rotate-gizmo", "Rotate (E)", ImGuizmo::ROTATE,
                                            m_currentGizmoOperation, buttonSize);
        ImGui::SameLine(0.f, 0.f);
        ui::ModeSelectorImageButton::render(scaleIcon, "##scale-gizmo", "Scale (R)", ImGuizmo::SCALE,
                                            m_currentGizmoOperation, buttonSize);

        ImGui::SameLine(0.f, 10.f);
        ui::ToggleImageButton::render(worldIcon, "##world-mode-gizmo", "World Mode (T)", ImGuizmo::WORLD,
                                      ImGuizmo::LOCAL, m_currentGizmoMode, buttonSize);

        ImGui::End();
    }

    core::EngineMode EditorLayer::updatePlayPauseButton(ImGuiWindowFlags windowFlags, float padding,
                                                        ImVec2 buttonSize) {
        core::EngineMode newEngineMode = m_engineMode;

        // Top-left of viewport
        ImVec2 windowPos =
            ImVec2(m_viewportUpperLeftScreenCoord.x + padding, m_viewportUpperLeftScreenCoord.y + padding);

        ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(0.0f, 0.0f));
        ImGui::Begin("ViewportOverlayPlayButton", nullptr, windowFlags);

        // check for viewport focus/hover
        m_inputState.isViewportFocused |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        m_inputState.isViewportHovered |= ImGui::IsWindowHovered(ImGuiHoveredFlags_None);
        m_inputState.isCursorOverUI |= ImGui::IsWindowHovered(ImGuiHoveredFlags_None);

        ImTextureID playIcon;
        std::string tooltip;
        if (m_engineMode == core::EngineMode::EDIT) {
            playIcon = (ImTextureID)m_editorTextureRegistry->get("play_button.png");
            tooltip = "Play";
        } else {
            playIcon = (ImTextureID)m_editorTextureRegistry->get("pause_button.png");
            tooltip = "Pause";
        }

        ui::ToggleImageButton::render(playIcon, "##play-button", tooltip.c_str(), core::EngineMode::PLAY,
                                      core::EngineMode::EDIT, newEngineMode, buttonSize);

        // render a camera tag if there is an active camera in EDIT MODE
        // TODO: separate these ui methods for the overlay better, needs refactoring. too many ifs depending on engine
        // mode.
        Scene& scene = Application::get().getScene();
        auto activeCameraEntityOpt = scene.getActiveCameraEntity();
        if (activeCameraEntityOpt.has_value() && m_engineMode == core::EngineMode::EDIT) {
            Entity activeCameraEntity = activeCameraEntityOpt.value();
            std::string activeCameraName = activeCameraEntity.get<NameComponent>().name;

            // if we clicked we have to dismiss the badge and set active camera to invalid.
            //! here we pass true for now because if we are inside this block we already checked for it to be open
            //! (there is an active camera) but we do not want to change a bool member variable (for example).
            //! we want to call "setActiveCameraEntity" to set no active camera entities.
            bool open = true;
            ImTextureID cameraIcon = (ImTextureID)m_editorTextureRegistry->get("camera_tag_icon.png");
            if (ui::DismissableBadge::renderWithIcon(activeCameraName.c_str(), &open, cameraIcon,
                                                     ImVec2(buttonSize.x * 2.5f, 30.f))) {
                scene.setActiveCameraEntity(core::UID::s_invalidId);
            }
        }

        ImGui::End();

        return newEngineMode;
    }

    void EditorLayer::updateViewportOverlayButtons(FrameInfo& frameInfo, float buttonsScale) {
        const float minButtonSize = 24.0f;
        const float minViewportExtent = std::min(m_sceneImageExtent.x, m_sceneImageExtent.y);
        ImVec2 buttonSize = ImVec2(minViewportExtent * buttonsScale, minViewportExtent * buttonsScale);
        buttonSize.x = std::max(buttonSize.x, minButtonSize);
        buttonSize.y = std::max(buttonSize.y, minButtonSize);
        const float padding = 10.0f;

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                       ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground;

        // -- PLAY / STOP BUTTON --
        core::EngineMode newEngineMode = updatePlayPauseButton(windowFlags, padding, buttonSize);

        // if the user clicked the button we request a mode change to the engine
        if (newEngineMode != m_engineMode) {
            Application::get().queueEvent<core::RequestEngineModeChangeEvent>(
                core::RequestEngineModeChangeEvent(newEngineMode));
        }

        // if we are not in EDIT mode we shouldn't render gizmo etc...
        if (m_engineMode != core::EngineMode::EDIT) {
            return;
        }

        // -- GIZMOS --
        updateGizmoOverlayButtons(windowFlags, padding, buttonSize);
    }

    ImVec2 EditorLayer::getImageSizeWithAspectRatioForImGuiWindow(ImVec2 windowSize, float aspectRatio) {
        ImVec2 ratioedExtent = {0, 0};

        // Calculate the width if the image fills the height
        float widthBasedOnHeight = windowSize.y * aspectRatio;

        // If filling the height makes the width exceed the window's width,
        // then the image must fill the width instead.
        if (widthBasedOnHeight > windowSize.x) {
            ratioedExtent.x = windowSize.x;
            ratioedExtent.y = windowSize.x / aspectRatio;
        } else {
            // Otherwise, fill the height
            ratioedExtent.x = widthBasedOnHeight;
            ratioedExtent.y = windowSize.y;
        }

        return ratioedExtent;
    }
} // namespace pxt::editor