#pragma once

#include "pxtengine.h"
#include "camera_nav_state.hpp"

namespace pxt::editor {
    class EditorCamera : public Camera {
    public:
        explicit EditorCamera();
        ~EditorCamera() = default;

        void onUpdate(float deltaTime, CameraNavigationState& camState, float aspectRatio);

        const glm::vec3& getPosition() const { return m_position; }
        const glm::vec2& getRotation() const { return m_rotation; } // pitch (x), yaw (y)

    private:
        glm::vec3 m_position{0.f, 0.f, 5.f};
        glm::vec2 m_rotation{0.f}; // (x) = pitch, (y) = yaw

        float m_moveSpeed{1.f};
        float m_lookSpeed{1.75f};

        glm::vec2 m_lastMousePos{0.f, 0.f};
        bool m_firstMouse = true;
        float m_mouseSensitivity = 0.0025f; // Adjust to taste
        float m_zoomSpeed = 2.0f;
    };
}