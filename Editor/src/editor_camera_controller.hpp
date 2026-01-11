#pragma once

#include "camera_nav_state.hpp"
#include "pxtengine.h"

namespace pxt::editor {
    class EditorCameraController {
    public:
        void onUpdate(float deltaTime, CameraNavigationState& camState, glm::vec3& rotation, glm::vec3& position);

    private:
        float m_moveSpeed{1.f};
        float m_lookSpeed{1.75f};

        float m_mouseSensitivity = 0.0025f; // Adjust to taste
        float m_zoomSpeed = 2.0f;
    };
} // namespace pxt::editor