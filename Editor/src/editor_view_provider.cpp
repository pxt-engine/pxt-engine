#include "editor_view_provider.hpp"

namespace pxt::editor {
    EditorViewProvider::EditorViewProvider(EditorCameraController editorCameraController, glm::vec3& position,
                                           glm::vec3& rotation)
        : m_editorCameraController(editorCameraController), m_activeCameraPosition(&position),
          m_activeCameraRotation(&rotation) {}

    CameraMatrices EditorViewProvider::getCameraMatrices(float aspectRatio) {
        CameraMatrices cm{};

        float finalAspectRatio = m_overrideAspectRatio ? m_aspectRatioOverride : aspectRatio;

        glm::vec3 forward;
        glm::vec3 upDir;
        glm::vec3 rightDir;

        CameraMath::computeOrthonormalBasisFromPitchYaw(forward, upDir, rightDir, m_activeCameraRotation->x,
                                                        m_activeCameraRotation->y);

        cm.viewMatrix = CameraMath::makeViewFromDirection(*m_activeCameraPosition, forward, upDir);
        cm.inverseViewMatrix = glm::inverse(cm.viewMatrix);

        if (m_activeCameraData.isPerspective()) {
            cm.projectionMatrix = CameraMath::makePerspective(m_activeCameraData, finalAspectRatio);
        } else {
            cm.projectionMatrix = CameraMath::makeOrthographic(m_activeCameraData);
        }

        cm.inverseProjectionMatrix = glm::inverse(cm.projectionMatrix);

        return cm;
    }

    void EditorViewProvider::resetState() {
        m_camNavState = CameraNavigationState();
        m_overrideAspectRatio = false;

        m_activeCameraPosition = nullptr;
        m_activeCameraRotation = nullptr;
    }

    void EditorViewProvider::updateActiveCamera(const CameraData& editorCameraData, glm::vec3& editorCameraPosition,
                                                glm::vec3& editorCameraRotation) {
        float aspectRatio = 1.f;
        bool overrideAspectRatio = false;

        // we are in EDIT mode and we need to know if there is an active camera entity.
        // if yes, we use its transform and camera component to update the editor camera
        if (auto activeCam = Application::get().getScene().getActiveCameraEntity()) {
            Entity activeCamEntity = activeCam.value();
            auto& transform = activeCamEntity.get<TransformComponent>();

            m_activeCameraRotation = &transform.rotation;
            m_activeCameraPosition = &transform.translation;

            auto& activeCameraComp = activeCamEntity.get<CameraComponent>();
            m_activeCameraData = activeCameraComp.cameraData;

            if (!activeCameraComp.useViewportAspectRatio) {
                m_overrideAspectRatio = true;
                m_aspectRatioOverride = activeCameraComp.aspectRatio;
            }
        }
        // else we use the editor camera data
        else {
            m_activeCameraRotation = &editorCameraRotation;
            m_activeCameraPosition = &editorCameraPosition;

            m_activeCameraData = editorCameraData;
        }
    }

    void EditorViewProvider::onUpdateCameraController(float deltaTime) {
        m_editorCameraController.onUpdate(deltaTime, m_camNavState, *m_activeCameraRotation, *m_activeCameraPosition);
    }
} // namespace pxt::editor