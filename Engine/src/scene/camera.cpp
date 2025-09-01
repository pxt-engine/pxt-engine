#include "scene/camera.hpp"

namespace PXTEngine {

    void Camera::setOrthographic() {
        m_projectionMatrix = glm::mat4{1.0f};
        m_projectionMatrix[0][0] = 2.f / (m_orthoParams[ORTHO_RIGHT] - m_orthoParams[ORTHO_LEFT]);
        m_projectionMatrix[1][1] = 2.f / (m_orthoParams[ORTHO_BOTTOM] - m_orthoParams[ORTHO_TOP]);
        m_projectionMatrix[2][2] = 1.f / (m_zFar - m_zNear);
        m_projectionMatrix[3][0] = -(m_orthoParams[ORTHO_RIGHT] + m_orthoParams[ORTHO_LEFT]) / (m_orthoParams[ORTHO_RIGHT] - m_orthoParams[ORTHO_LEFT]);
        m_projectionMatrix[3][1] = -(m_orthoParams[ORTHO_BOTTOM] + m_orthoParams[ORTHO_TOP]) / (m_orthoParams[ORTHO_BOTTOM] - m_orthoParams[ORTHO_TOP]);
        m_projectionMatrix[3][2] = -m_zNear / (m_zFar - m_zNear);
    }

    void Camera::setPerspective(float aspect) {
        PXT_ASSERT(glm::abs(aspect - std::numeric_limits<float>::epsilon()) > 0.0f);

        const float tanHalfFovy = tan(glm::radians(m_fovYDegrees) / 2.f);
        m_projectionMatrix = glm::mat4{0.0f};
        m_projectionMatrix[0][0] = 1.f / (aspect * tanHalfFovy);
        m_projectionMatrix[1][1] = 1.f / (tanHalfFovy);
        m_projectionMatrix[2][2] = m_zFar / (m_zFar - m_zNear);
        m_projectionMatrix[2][3] = 1.f;
        m_projectionMatrix[3][2] = -(m_zFar * m_zNear) / (m_zFar - m_zNear);
    }

    void Camera::setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up) {
        PXT_ASSERT((glm::dot(direction, direction) > std::numeric_limits<float>::epsilon()), "Direction cannot be zero");

        const glm::vec3 w{glm::normalize(direction)};
        const glm::vec3 u{glm::normalize(glm::cross(w, up))};
        const glm::vec3 v{glm::cross(w, u)};

        updateViewMatrix(u, v, w, position);
    }

    void Camera::setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up) {
        setViewDirection(position, target - position, up);
    }

    void Camera::setViewYXZ(glm::vec3 position, glm::vec3 rotation) {
        const float c3 = glm::cos(rotation.z);
        const float s3 = glm::sin(rotation.z);
        const float c2 = glm::cos(rotation.x);
        const float s2 = glm::sin(rotation.x);
        const float c1 = glm::cos(rotation.y);
        const float s1 = glm::sin(rotation.y);
        const glm::vec3 u{(c1 * c3 + s1 * s2 * s3), (c2 * s3), (c1 * s2 * s3 - c3 * s1)};
        const glm::vec3 v{(c3 * s1 * s2 - c1 * s3), (c2 * c3), (c1 * c3 * s2 + s1 * s3)};
        const glm::vec3 w{(c2 * s1), (-s2), (c1 * c2)};
        
        updateViewMatrix(u, v, w, position);
    }

    void Camera::updateViewMatrix(glm::vec3 u, glm::vec3 v, glm::vec3 w, glm::vec3 position) {
        m_viewMatrix = glm::mat4{1.f};
        m_viewMatrix[0][0] = u.x;
        m_viewMatrix[1][0] = u.y;
        m_viewMatrix[2][0] = u.z;
        m_viewMatrix[0][1] = v.x;
        m_viewMatrix[1][1] = v.y;
        m_viewMatrix[2][1] = v.z;
        m_viewMatrix[0][2] = w.x;
        m_viewMatrix[1][2] = w.y;
        m_viewMatrix[2][2] = w.z;
        m_viewMatrix[3][0] = -glm::dot(u, position);
        m_viewMatrix[3][1] = -glm::dot(v, position);
        m_viewMatrix[3][2] = -glm::dot(w, position);

        m_inverseViewMatrix = glm::mat4{1.f};
        m_inverseViewMatrix[0][0] = u.x;
        m_inverseViewMatrix[0][1] = u.y;
        m_inverseViewMatrix[0][2] = u.z;
        m_inverseViewMatrix[1][0] = v.x;
        m_inverseViewMatrix[1][1] = v.y;
        m_inverseViewMatrix[1][2] = v.z;
        m_inverseViewMatrix[2][0] = w.x;
        m_inverseViewMatrix[2][1] = w.y;
        m_inverseViewMatrix[2][2] = w.z;
        m_inverseViewMatrix[3][0] = position.x;
        m_inverseViewMatrix[3][1] = position.y;
        m_inverseViewMatrix[3][2] = position.z;
    }


    void Camera::drawCameraUi() {
        ImGui::Checkbox("Perspective View", &m_isPerspective);
        if (m_isPerspective) {
            ImGui::SliderFloat("Vertical FOV (degrees)", &m_fovYDegrees, 1.0f, 120.0f);
		}
		else {
			ImGui::DragFloat4("Ortho Params (left, right, top, bottom)", glm::value_ptr(m_orthoParams), 0.1f);
		}

		ImGui::SliderFloat("Near Plane", &m_zNear, 0.01f, m_zFar - 0.01f);
		ImGui::SliderFloat("Far Plane", &m_zFar, m_zNear + 0.01f, 1000.0f);
    }

}