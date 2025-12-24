#include "camera_controller.hpp"

using namespace pxt::core;

void CameraController::onUpdate(float deltaTime) {
    auto& transform = get<TransformComponent>();

    // can look with mouse when Space is Hold
    bool is_mouse_move_enabled = Input::isKeyDown(KeyCode::Space);

    // --- Keyboard Rotation ---
    glm::vec3 rotate{0};
    if (Input::isKeyDown(KeyCode::RightArrow))
        rotate.y -= 1.f;
    if (Input::isKeyDown(KeyCode::LeftArrow))
        rotate.y += 1.f;
    if (Input::isKeyDown(KeyCode::UpArrow))
        rotate.x -= 1.f;
    if (Input::isKeyDown(KeyCode::DownArrow))
        rotate.x += 1.f;

    if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon()) {
        transform.rotation += m_lookSpeed * deltaTime * glm::normalize(rotate);
    }

    // --- Mouse Movement for Rotation ---
    if (is_mouse_move_enabled) {
        glm::vec2 offset = Input::getMouseDelta();

        // Invert the Y offset so that moving the mouse up (decreasing y)
        // increases the pitch (rotation.x) and vice versa.
        transform.rotation.x += offset.y * m_mouseSensitivity;
        transform.rotation.y += -offset.x * m_mouseSensitivity;

        transform.rotation.x = glm::clamp(transform.rotation.x, -1.5f, 1.5f);
        transform.rotation.y = glm::mod(transform.rotation.y, glm::two_pi<float>());
    }

    // --- Keyboard Translation ---
    // In Right-Handed coordinates:
    // Yaw = rotation.y, Pitch = rotation.x
    glm::vec3 forward;
    forward.x = glm::sin(transform.rotation.y) * glm::cos(transform.rotation.x);
    forward.y = glm::sin(-transform.rotation.x); // Pitch (negative because looking up is positive X)
    forward.z = glm::cos(transform.rotation.y) * glm::cos(transform.rotation.x);
    forward = glm::normalize(forward);

    const glm::vec3 worldUp{0.f, 1.f, 0.f}; // Y-Up Standard
    const glm::vec3 rightDir = glm::normalize(glm::cross(forward, worldUp));
    const glm::vec3 upDir = glm::cross(rightDir, forward);

    // --- Keyboard Translation ---
    glm::vec3 moveDir{0.f};
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

    if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) {
        transform.translation += m_moveSpeed * deltaTime * glm::normalize(moveDir);
    }

    // update camera view matrix
    auto& camera = get<CameraComponent>().camera;
    camera.setViewDirection(transform.translation, forward, worldUp);
}
