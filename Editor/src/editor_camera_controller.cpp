#include "editor_camera_controller.hpp"

namespace pxt::editor {
    void EditorCameraController::onUpdate(float deltaTime, CameraNavigationState& camState, glm::vec3& rotation,
                                          glm::vec3& position) {
        // --- Keyboard Rotation ---
        if (glm::length2(camState.rotate) > glm::epsilon<float>()) {
            rotation += m_lookSpeed * deltaTime * glm::normalize(camState.rotate);
        }

        // --- Mouse Movement for Rotation ---
        if (camState.freeLookEnabled) {
            // Invert the Y offset so that moving the mouse up (decreasing y)
            // increases the pitch (rotation.x) and vice versa.
            rotation.x += camState.mouseDelta.y * m_mouseSensitivity;
            rotation.y += camState.mouseDelta.x * m_mouseSensitivity;

            rotation.x = glm::clamp(rotation.x, -1.5f, 1.5f);
            rotation.y = glm::mod(rotation.y, glm::two_pi<float>());
        }

        // --- Keyboard Translation ---
        glm::vec3 forward;
        float pitch = rotation.x;
        float yaw = rotation.y;

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

            position += m_moveSpeed * deltaTime * worldMove;
        }

        // --- Mouse Scroll Zoom (Dolly) ---
        if (camState.scrollDelta.y != 0.0f) {
            // We move the camera translation along the forward vector
            position += forward * camState.scrollDelta.y * m_zoomSpeed;
        }
    }
} // namespace pxt::editor