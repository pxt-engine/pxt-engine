#pragma once

#include "core/pch.hpp"

namespace pxt {

    namespace {
        constexpr uint32_t ORTHO_LEFT = 0;
        constexpr uint32_t ORTHO_RIGHT = 1;
        constexpr uint32_t ORTHO_TOP = 2;
        constexpr uint32_t ORTHO_BOTTOM = 3;
    } // namespace

    /**
     * @brief CameraData class for storing all camera information.
     */
    class CameraData {
    public:

        void setPerspectiveParams(float fovYDegrees, float zNear, float zFar);

        void setOrthographicParams(float left, float right, float top, float bottom, float zNear, float zFar);

        void setIsPerspective(bool isPerspective) { m_isPerspective = isPerspective; }

        const bool isPerspective() const { return m_isPerspective; }

        float getFovYDegrees() const { return m_fovYDegrees; }

        float getNearPlane() const { return m_zNear; }

        float getFarPlane() const { return m_zFar; }

        float getOrthoLeft() const { return m_orthoParams[ORTHO_LEFT]; }

        float getOrthoRight() const { return m_orthoParams[ORTHO_RIGHT]; }

        float getOrthoTop() const { return m_orthoParams[ORTHO_TOP]; }

        float getOrthoBottom() const { return m_orthoParams[ORTHO_BOTTOM]; }

        void drawCameraUi();

    private:
        float m_fovYDegrees{50.f};
        float m_zNear{0.1f};
        float m_zFar{100.f};

        glm::vec4 m_orthoParams{-1.f, 1.f, -1.f, 1.f}; // left, right, top, bottom

        bool m_isPerspective{true};
    };

} // namespace pxt
