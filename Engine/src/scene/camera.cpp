#include "scene/camera.hpp"

namespace pxt {
    void Camera::setPerspectiveParams(float fovYDegrees, float zNear, float zFar) {
        m_fovYDegrees = fovYDegrees;
        m_zNear = zNear;
        m_zFar = zFar;
    }

    void Camera::setOrthographicParams(float left, float right, float top, float bottom, float zNear, float zFar) {
        m_orthoParams[ORTHO_LEFT] = left;
        m_orthoParams[ORTHO_RIGHT] = right;
        m_orthoParams[ORTHO_TOP] = top;
        m_orthoParams[ORTHO_BOTTOM] = bottom;
        m_zNear = zNear;
        m_zFar = zFar;
    }

    void Camera::setOrthographic() {
        // GLM orthographic for Vulkan (Depth 0 to 1)
        m_projectionMatrix =
            glm::orthoRH_ZO(m_orthoParams[ORTHO_LEFT], m_orthoParams[ORTHO_RIGHT], m_orthoParams[ORTHO_BOTTOM],
                            m_orthoParams[ORTHO_TOP], m_zNear, m_zFar);

        // Y-flip for Vulkan
        m_projectionMatrix[1][1] *= -1.0f;
    }

    void Camera::setPerspective(float aspect) {
        PXT_ASSERT(glm::abs(aspect - std::numeric_limits<float>::epsilon()) > 0.0f);

        // perspectiveRH_ZO: Right-Handed, Zero-to-One depth (Vulkan standard)
        m_projectionMatrix = glm::perspectiveRH_ZO(glm::radians(m_fovYDegrees), aspect, m_zNear, m_zFar);

        // Vulkan Y-axis points down, but we want Y-Up in world space
        m_projectionMatrix[1][1] *= -1.0f;

        m_inverseProjectionMatrix = glm::inverse(m_projectionMatrix);
    }

    void Camera::setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up) {
        // lookAtRH: Right-Handed view matrix
        // Target = position + direction
        m_viewMatrix = glm::lookAtRH(position, position + direction, up);
        m_inverseViewMatrix = glm::inverse(m_viewMatrix);
    }

    void Camera::setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up) {
        m_viewMatrix = glm::lookAtRH(position, target, up);
        m_inverseViewMatrix = glm::inverse(m_viewMatrix);
    }

    void Camera::drawCameraUi() {
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