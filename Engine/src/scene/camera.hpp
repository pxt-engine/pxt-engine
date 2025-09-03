#pragma once

#include "core/pch.hpp"

namespace PXTEngine {

    namespace {
		constexpr uint32_t ORTHO_LEFT = 0;
		constexpr uint32_t ORTHO_RIGHT = 1;
		constexpr uint32_t ORTHO_TOP = 2;
		constexpr uint32_t ORTHO_BOTTOM = 3;
    }

    /**
     * @brief Camera class for handling perspective and orthographic projections.
     */
    class Camera {
    public:
        /**
         * @brief Sets the camera projection to a perspective projection.
         * 
         * @param aspect The aspect ratio (width / height).
         */
        void setPerspective(float aspect);

        /**
         * @brief Sets the camera projection to an orthographic projection.
         */
        void setOrthographic();

        /**
         * @brief Sets the camera view matrix based on a direction vector.
         * 
         * @param position The camera position in world coordinates.
         * @param direction The direction the camera is facing.
         * @param up The up vector, defaults to (0,-1,0).
         */
        void setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3{0.f, -1.f, 0.f});

        /**
         * @brief Sets the camera view matrix to look at a specific target.
         * 
         * @param position The camera position in world coordinates.
         * @param target The target position to look at.
         * @param up The up vector, defaults to (0,-1,0).
         */
        void setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up = glm::vec3{0.f, -1.f, 0.f});

        /**
         * @brief Sets the camera view matrix using YXZ Euler angles.
         * 
         * @param position The camera position in world coordinates.
         * @param rotation Euler angles representing the camera's orientation.
         */
        void setViewYXZ(glm::vec3 position, glm::vec3 rotation);

        /**
         * @brief Retrieves the projection matrix.
         * 
         * @return The projection matrix.
         */
        const glm::mat4& getProjectionMatrix() const { return m_projectionMatrix; }

        /**
         * @brief Retrieves the view matrix.
         * 
         * @return The view matrix.
         */
        const glm::mat4& getViewMatrix() const { return m_viewMatrix; }

        /**
         * @brief Retrieves the inverse view matrix.
         * 
         * @return The inverse view matrix.
         */
        const glm::mat4& getInverseViewMatrix() const { return m_inverseViewMatrix; }

        /**
         * @brief Retrieves the camera position from the inverse view matrix.
         * 
         * @return The camera position in world coordinates.
         */
        const glm::vec3 getPosition() const { return glm::vec3(m_inverseViewMatrix[3]); }

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
        /**
         * @brief Updates the camera's view matrix based on provided basis vectors.
         * 
         * @param u The right vector of the camera.
         * @param v The up vector of the camera.
         * @param w The forward vector of the camera.
         * @param position The camera position in world coordinates.
         */
        void updateViewMatrix(glm::vec3 u, glm::vec3 v, glm::vec3 w, glm::vec3 position);

        glm::mat4 m_projectionMatrix{1.f}; 
        glm::mat4 m_viewMatrix{1.f}; 
        glm::mat4 m_inverseViewMatrix{1.f};

		float m_fovYDegrees{ 50.f };
		float m_zNear{ 0.1f };
		float m_zFar{ 100.f };

		glm::vec4 m_orthoParams{ -1.f, 1.f, -1.f, 1.f }; // left, right, top, bottom

		bool m_isPerspective{ true };
    };

}
