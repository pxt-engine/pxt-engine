#pragma once

#include "graphics/view_provider.hpp"
#include "graphics/camera_matrices.hpp"
#include "editor_camera_controller.hpp"

namespace pxt::editor {
    class EditorViewProvider : public IViewProvider {
    public:
        explicit EditorViewProvider(EditorCameraController editorCameraController);

        CameraMatrices getCameraMatrices(float aspectRatio) override;

        const EditorCameraController& getEditorCameraController() const { return m_editorCameraController; };

        void resetState();

        void updateActiveCamera(const core::EngineMode engineMode, const CameraData& editorCameraData, const glm::vec3& editorCameraPosition, const glm::vec2& editorCameraRotation);

        void setCameraNavigationState(CameraNavigationState newCameraNavState) { m_camNavState = newCameraNavState; };

        void setActiveCameraData(const CameraData& camData) { m_activeCameraData = camData; };

        void setActiveCameraPosition(const glm::vec3 position) { m_activeCameraPosition = position; };

        void setActiveCameraRotation(const glm::vec2 rotation) { m_activeCameraRotation = rotation; };

        void setAspectRatioOverride(const float aspectRatioOverride) { 
            m_aspectRatioOverride = aspectRatioOverride;
            m_overrideAspectRatio = true;
        };

        void onUpdateCameraController(float deltaTime);

    private:
        EditorCameraController m_editorCameraController;
        CameraNavigationState m_camNavState{};

        CameraData m_activeCameraData{};
        glm::vec2 m_activeCameraRotation{0.f};
        glm::vec3 m_activeCameraPosition{0.f};

        //! this could be bad design
        //! (to override a method parameter of the interface method "getCameraMatrices" without the caller knowing)
        //! for now its simpler like that.
        // TODO: change this
        float m_aspectRatioOverride = 1.f;
        bool m_overrideAspectRatio = false;
    };
}