#include "editor_layer.hpp"
#include "core/events/editor_events.hpp"
#include "core/events/imgui_events.hpp"

#include <ImGuizmo.h>

namespace pxt::editor {
    EditorLayer::EditorLayer() : core::Layer("EditorLayer") {}

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
            // we dont care about mouse clicks outside of the viewport (for now)
            if (!m_isViewportHovered)
                return false;

            return onMouseButtonPress(e);
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
        
        Entity selectedEntity = frameInfo.scene.getEntity(m_selectedEntityUUID);
        TransformComponent& transform = selectedEntity.get<TransformComponent>();

        const glm::mat4& view = frameInfo.camera.getViewMatrix();
        const glm::mat4& projection = frameInfo.camera.getProjectionMatrix();
        glm::mat4 modelMatrix = transform.mat4();

        // m_GizmoOperation could be ImGuizmo::TRANSLATE, ROTATE, or SCALE
        static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
        static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::LOCAL);

        // Short-cut keys
        if (!ImGui::IsAnyItemActive()) {
            if (ImGui::IsKeyPressed(ImGuiKey_E))
                mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
            if (ImGui::IsKeyPressed(ImGuiKey_R))
                mCurrentGizmoOperation = ImGuizmo::ROTATE;
            if (ImGui::IsKeyPressed(ImGuiKey_T))
                mCurrentGizmoOperation = ImGuizmo::SCALE;
        }

        ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), mCurrentGizmoOperation,
                             mCurrentGizmoMode, glm::value_ptr(modelMatrix));

        // apply changes back to entity
        if (ImGuizmo::IsUsingAny()) {
            glm::vec3 translation, rotation, scale;
            ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(modelMatrix), glm::value_ptr(translation),
                                                  glm::value_ptr(rotation), glm::value_ptr(scale));

            transform.translation = translation;
            transform.rotation = rotation; // ImGuizmo returns degrees by default
            transform.scale = scale;
        }

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