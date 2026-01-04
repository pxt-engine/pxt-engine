#include "editor_camera.hpp"

namespace pxt::editor {
    EditorCamera::EditorCamera() : Camera() {}

    void EditorCamera::onUpdate(float deltaTime, CameraNavigationState& camState, float aspectRatio) {
        // --- Keyboard Rotation ---
        if (glm::length2(camState.rotate) > glm::epsilon<float>()) {
            m_rotation += m_lookSpeed * deltaTime * glm::normalize(camState.rotate);
        }

        // --- Mouse Movement for Rotation ---
        if (camState.freeLookEnabled) {
            // Invert the Y offset so that moving the mouse up (decreasing y)
            // increases the pitch (rotation.x) and vice versa.
            m_rotation.x += camState.mouseDelta.y * m_mouseSensitivity;
            m_rotation.y += camState.mouseDelta.x * m_mouseSensitivity;

            m_rotation.x = glm::clamp(m_rotation.x, -1.5f, 1.5f);
            m_rotation.y = glm::mod(m_rotation.y, glm::two_pi<float>());
        }

        // --- Keyboard Translation ---
        glm::vec3 forward;
        float pitch = m_rotation.x;
        float yaw = m_rotation.y;

        forward.x = glm::sin(yaw) * glm::cos(pitch);
        forward.y = -glm::sin(pitch);
        forward.z = -glm::cos(yaw) * glm::cos(pitch);
        forward = glm::normalize(forward);

        const glm::vec3 worldUp{0.f, 1.f, 0.f};

        // Right vector must be perpendicular to Forward and WorldUp
        // In -Z forward, Right should be +X: cross(forward, worldUp) handles this
        const glm::vec3 rightDir = glm::normalize(glm::cross(forward, worldUp));
        // Re-calculate Up to ensure orthonormality
        const glm::vec3 upDir = glm::cross(rightDir, forward);

        if (camState.freeLookEnabled && glm::length2(camState.move) > glm::epsilon<float>()) {
            glm::vec3 localDir = glm::normalize(camState.move);

            glm::vec3 worldMove = localDir.z * forward + localDir.x * rightDir + localDir.y * worldUp;

            m_position += m_moveSpeed * deltaTime * worldMove;
        }

        // --- Mouse Scroll Zoom (Dolly) ---
        if (camState.scrollDelta.y != 0.0f) {
            // We move the camera translation along the forward vector
            m_position += forward * camState.scrollDelta.y * m_zoomSpeed;
        }

        setViewDirection(m_position, forward, worldUp);

        if (camState.isPerspective) {
            setPerspective(aspectRatio);
        } else {
            setOrthographic();
        }
    }
} // namespace pxt::editor