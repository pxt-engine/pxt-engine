#include "scene/camera_data.hpp"

namespace pxt {
    void CameraData::setPerspectiveParams(float fovYDegrees, float zNear, float zFar) {
        m_fovYDegrees = fovYDegrees;
        m_zNear = zNear;
        m_zFar = zFar;
    }

    void CameraData::setOrthographicParams(float left, float right, float top, float bottom, float zNear, float zFar) {
        m_orthoParams[ORTHO_LEFT] = left;
        m_orthoParams[ORTHO_RIGHT] = right;
        m_orthoParams[ORTHO_TOP] = top;
        m_orthoParams[ORTHO_BOTTOM] = bottom;
        m_zNear = zNear;
        m_zFar = zFar;
    }

    void CameraData::drawCameraUi() {
        ImGui::Checkbox("Perspective View", &m_isPerspective);
        if (m_isPerspective) {
            ImGui::SliderFloat("Vertical FOV (degrees)", &m_fovYDegrees, 1.0f, 120.0f);
        } else {
            ImGui::DragFloat4("Ortho Params (left, right, top, bottom)", glm::value_ptr(m_orthoParams), 0.1f);
        }

        ImGui::SliderFloat("Near Plane", &m_zNear, 0.01f, m_zFar - 0.01f);
        ImGui::SliderFloat("Far Plane", &m_zFar, m_zNear + 0.01f, 1000.0f);
    }

} // namespace pxt