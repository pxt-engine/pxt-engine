#include "camera_controller.hpp"

using namespace pxt::core;

void CameraController::onUpdate(float deltaTime) {
    auto& transform = get<TransformComponent>();

    // can look with mouse when Space is Hold
    bool isFreeLookModeEnabled = Input::isMouseButtonDown(MouseButton::Button1);

    // --- Keyboard Rotation ---
    glm::vec3 rotate{0};
    if (Input::isKeyDown(KeyCode::RightArrow))
        rotate.y += 1.f;
    if (Input::isKeyDown(KeyCode::LeftArrow))
        rotate.y -= 1.f;
    if (Input::isKeyDown(KeyCode::UpArrow))
        rotate.x -= 1.f;
    if (Input::isKeyDown(KeyCode::DownArrow))
        rotate.x += 1.f;

    if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon()) {
        transform.rotation += m_lookSpeed * deltaTime * glm::normalize(rotate);
    }

    // --- Mouse Movement for Rotation ---
    if (isFreeLookModeEnabled) {
        glm::vec2 offset = Input::getMouseDelta();

        // Invert the Y offset so that moving the mouse up (decreasing y)
        // increases the pitch (rotation.x) and vice versa.
        transform.rotation.x += offset.y * m_mouseSensitivity;
        transform.rotation.y += offset.x * m_mouseSensitivity;

        transform.rotation.x = glm::clamp(transform.rotation.x, -1.5f, 1.5f);
        transform.rotation.y = glm::mod(transform.rotation.y, glm::two_pi<float>());
    }

    // --- Keyboard Translation ---
    glm::vec3 forward;
    float pitch = transform.rotation.x;
    float yaw = transform.rotation.y;

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

    // --- Keyboard Translation ---
    glm::vec3 moveDir{0.f};

    if (isFreeLookModeEnabled) {
        if (Input::isKeyDown(KeyCode::W))
            moveDir += forward;
        if (Input::isKeyDown(KeyCode::S))
            moveDir -= forward;
        if (Input::isKeyDown(KeyCode::D))
            moveDir += rightDir;
        if (Input::isKeyDown(KeyCode::A))
            moveDir -= rightDir;
        if (Input::isKeyDown(KeyCode::E))
            moveDir += worldUp;
        if (Input::isKeyDown(KeyCode::Q))
            moveDir -= worldUp;
    }

    if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) {
        transform.translation += m_moveSpeed * deltaTime * glm::normalize(moveDir);
    }

    // --- Mouse Scroll Zoom (Dolly) ---
    float scrollDeltaY = Input::getState().getScrollDelta().y; // Assuming .y is the vertical wheel
    if (std::abs(scrollDeltaY) > 0.0f) {
        // We move the camera translation along the forward vector
        transform.translation += forward * scrollDeltaY * m_zoomSpeed;
    }

    // update camera view matrix
    auto& camera = get<CameraComponent>().camera;
    camera.setViewDirection(transform.translation, forward, worldUp);
}
