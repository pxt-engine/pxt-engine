#include "editor_layer.hpp"
#include "core/events/editor_events.hpp"
#include "core/events/imgui_events.hpp"
#include "ui/widgets/mode_selector_image_button.hpp"

#include <glm/gtx/matrix_decompose.hpp> // will use it in the future for gizmos

namespace pxt::editor {
    EditorLayer::EditorLayer() : core::Layer("EditorLayer") {
        m_editorTextureRegistry = createUnique<EditorTextureRegistry>();
    }

    void EditorLayer::onEvent(core::Event& event) {
        core::EventDispatcher dispatcher(event);

        dispatcher.dispatch<core::SelectedEntityChangedEvent>([this](core::SelectedEntityChangedEvent& e) {
            m_selectedEntityUUID = e.getSelectedEntityUUID();
            return false;
        });

        // I/O Events

        // here we have to decide which events to forward (not handled) if
        // the viewport is not focused (for now everything regarding inputs is blocked)
        if (!m_isViewportFocused) {
            core::Input::getState().reset();
            return;
        }

        dispatcher.dispatch<core::MouseButtonPressEvent>([this](core::MouseButtonPressEvent& e) {
            // we dont care about mouse clicks outside of the viewport for object picking
            // and we do not want to interfere with other ui elements
            if (!m_isViewportHovered || m_isAnyButtonHovered)
                return false;

            return onMouseButtonPress(e);
        });

        dispatcher.dispatch<core::KeyPressEvent>([this](core::KeyPressEvent& e) {
            if (!m_isViewportHovered)
                return false;
            return onKeyPressEvent(e);
        });
    }

    bool EditorLayer::onMouseButtonPress(core::MouseButtonPressEvent& event) {
        // handle mouse button press events here
        m_lastClickMousePosImGui = core::Input::getState().getMousePositionImGui();

        float x = m_lastClickMousePosImGui.x - m_viewportUpperLeftScreenCoord.x;
        float y = m_lastClickMousePosImGui.y - m_viewportUpperLeftScreenCoord.y;

        uint32_t px = static_cast<uint32_t>(std::clamp(x, 0.0f, (float)(m_sceneImageExtent.x - 1)));
        uint32_t py = static_cast<uint32_t>(std::clamp(y, 0.0f, (float)(m_sceneImageExtent.y - 1)));

        // PXT_INFO("Mouse Button Pressed at position: ({}, {}) (Realtive to upper-left corner of viewport)",
        //	px, py);

        Application::get().queueEvent(core::PickObjectAtEvent(px, py));

        return false;
    }

    bool EditorLayer::onKeyPressEvent(core::KeyPressEvent& event) {
        // handle key press events here
        switch (event.getKeyCode()) {
        case core::KeyCode::Number1:
            m_currentGizmoOperation = ImGuizmo::TRANSLATE;
            break;
        case core::KeyCode::Number2:
            m_currentGizmoOperation = ImGuizmo::ROTATE;
            break;
        case core::KeyCode::Number3:
            m_currentGizmoOperation = ImGuizmo::SCALE;
            break;
        case core::KeyCode::Number4:
            if (m_currentGizmoMode == ImGuizmo::WORLD) {
                m_currentGizmoMode = ImGuizmo::LOCAL;
            } else {
                m_currentGizmoMode = ImGuizmo::WORLD;
            }
            break;
        default:
            // propagate event if key not handled
            return true;
            break;
        }
        return false;
    }

    void EditorLayer::onUpdateUi(FrameInfo& frameInfo) {
        // first update scene hierarchy ui (an entity might be selected)
        m_sceneHierarchy.onUpdateUi(frameInfo, m_selectedEntityUUID);

        // only fire event if selection changed
        if (m_prevSelectedEntityUUID != m_selectedEntityUUID) {
            Application::get().queueEvent(core::SelectedEntityChangedEvent(m_selectedEntityUUID));
            m_prevSelectedEntityUUID = m_selectedEntityUUID;
        }

        // then update entity inspector ui
        m_entityInspector.onUpdateUi(frameInfo, m_selectedEntityUUID);

        // main menu bar
        m_mainMenuBar.onUpdateUi(frameInfo);

        // maybe viewport class in the future?
        updateSceneUi(frameInfo);
    }

    void EditorLayer::updateSceneUi(FrameInfo& frameInfo) {
        m_isAnyButtonHovered = false;

        ImTextureID scene = (ImTextureID)frameInfo.sceneDescriptorSet;

        // we push a style var to remove the viewpoer window padding
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Viewport");

        m_isViewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_None);
        m_isViewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        m_viewportUpperLeftScreenCoord = ImGui::GetCursorScreenPos();

        ImVec2 viewportSize = ImGui::GetContentRegionAvail();

        // check if window size has changed
        if (viewportSize.x != m_sceneImageExtent.x || viewportSize.y != m_sceneImageExtent.y) {
            uint32_t width = static_cast<uint32_t>(std::max(1.0f, viewportSize.x));
            uint32_t height = static_cast<uint32_t>(std::max(1.0f, viewportSize.y));

            m_sceneImageExtent = ImVec2(static_cast<float>(width), static_cast<float>(height));

            Application::get().queueEvent(
                core::ImGuiViewportResizeEvent(static_cast<uint32_t>(width), static_cast<uint32_t>(height)));
        }

        ImGui::Image(scene, m_sceneImageExtent);

        // this has to be called inside the window where ImGuizmo is used
        updateGizmos(frameInfo);

        updateViewportOverlayButtons(frameInfo, 0.8);

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void EditorLayer::updateGizmos(FrameInfo& frameInfo) {
        // nothing selected
        if (m_selectedEntityUUID == core::UUID::s_invalidId)
            return;

        // ImGuizmo::BeginFrame() is called right after ImGui::NewFrame() in UiRenderLayer

        ImGuizmo::SetDrawlist();

        ImGuizmo::SetRect(m_viewportUpperLeftScreenCoord.x, m_viewportUpperLeftScreenCoord.y, m_sceneImageExtent.x,
                          m_sceneImageExtent.y);

        ImGuizmo::SetGizmoSizeClipSpace(0.2f);
        ImGuizmo::SetOrthographic(false);
        // prevents the gizmo from "flipping" when looked from different angles
        // if flipped, an axis is decorated with black dots
        ImGuizmo::AllowAxisFlip(false);

        Entity selectedEntity = frameInfo.scene.getEntity(m_selectedEntityUUID);
        TransformComponent& transform = selectedEntity.get<TransformComponent>();

        // copy - we need to modify them to adhere opengl standards (rh, y up)
        const glm::mat4& gizmoView = frameInfo.camera.getViewMatrix();
        glm::mat4 gizmoProj = frameInfo.camera.getProjectionMatrix();
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

    void EditorLayer::updateViewportOverlayButtons(FrameInfo& frameInfo, float buttonsScale) {
        const ImVec2 buttonSize = ImVec2(90.0f * buttonsScale, 90.0f * buttonsScale);
        const float padding = 10.0f;

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                       ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground;

        // Top-right of viewport
        ImVec2 windowPos(m_viewportUpperLeftScreenCoord.x + m_sceneImageExtent.x - padding,
                         m_viewportUpperLeftScreenCoord.y + padding);

        ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::Begin("ViewportOverlayButtons", nullptr, windowFlags);

        // check for viewport focus/hover
        m_isViewportFocused |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        m_isViewportHovered |= ImGui::IsWindowHovered(ImGuiHoveredFlags_None);
        m_isAnyButtonHovered |= ImGui::IsWindowHovered(ImGuiHoveredFlags_None);

        ImTextureID translateIcon = (ImTextureID)m_editorTextureRegistry->get("translation_gizmo.png");
        ImTextureID scaleIcon = (ImTextureID)m_editorTextureRegistry->get("scale_gizmo.png");
        ImTextureID rotateIcon = (ImTextureID)m_editorTextureRegistry->get("rotation_gizmo.png");
        ImTextureID worldIcon = (ImTextureID)m_editorTextureRegistry->get("world_mode_gizmo.png");

        ui::ModeSelectorImageButton::render(translateIcon, "##translate-gizmo", "Translate (1)", ImGuizmo::TRANSLATE,
                                            m_currentGizmoOperation, buttonSize);

        ImGui::SameLine(0.f, 0.f);
        ui::ModeSelectorImageButton::render(rotateIcon, "##rotate-gizmo", "Rotate (2)", ImGuizmo::ROTATE,
                                            m_currentGizmoOperation, buttonSize);
        ImGui::SameLine(0.f, 0.f);
        ui::ModeSelectorImageButton::render(scaleIcon, "##scale-gizmo", "Scale (3)", ImGuizmo::SCALE,
                                            m_currentGizmoOperation, buttonSize);

        ImGui::SameLine(0.f, 10.f);
        ui::ModeSelectorImageButton::render(worldIcon, "##world-mode-gizmo", "World Mode (4)", ImGuizmo::WORLD,
                                            m_currentGizmoMode, buttonSize);

        ImGui::End();
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