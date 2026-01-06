#include "graphics/camera_matrices.hpp"

namespace pxt {
    glm::mat4 CameraUtils::buildOrthographic(CameraData& camData) {
        // GLM orthographic for Vulkan (Depth 0 to 1)
        glm::mat4 projectionMatrix =
            glm::orthoRH_ZO(camData.getOrthoLeft(), camData.getOrthoRight(), camData.getOrthoBottom(),
                            camData.getOrthoTop(), camData.getNearPlane(), camData.getFarPlane());

        // Y-flip for Vulkan
        projectionMatrix[1][1] *= -1.0f;

        return projectionMatrix;
    }

    glm::mat4 CameraUtils::buildPerspective(CameraData& camData, float aspect) {
        PXT_ASSERT(glm::abs(aspect - std::numeric_limits<float>::epsilon()) > 0.0f);

        // perspectiveRH_ZO: Right-Handed, Zero-to-One depth (Vulkan standard)
        glm::mat4 projectionMatrix =
            glm::perspectiveRH_ZO(glm::radians(camData.getFovYDegrees()), aspect, camData.getNearPlane(), camData.getFarPlane());

        // Vulkan Y-axis points down, but we want Y-Up in world space
        projectionMatrix[1][1] *= -1.0f;

        return projectionMatrix;
    }

    glm::mat4 CameraUtils::setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up) {
        // lookAtRH: Right-Handed view matrix
        // Target = position + direction
        return glm::lookAtRH(position, position + direction, up);
    }

    glm::mat4 CameraUtils::setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up) {
        return glm::lookAtRH(position, target, up);
    }

    glm::vec3 CameraUtils::getCameraPos(const glm::mat4& inverseViewMatrix) { 
        return inverseViewMatrix[3];
    }

    void CameraUtils::buildOrthonormalBasisFromPitchAndYaw(glm::vec3& forward, glm::vec3& upDir, glm::vec3& rightDir,
                                                           float pitch, float yaw, bool isWorldYUp) {
        forward.x = glm::sin(yaw) * glm::cos(pitch);
        forward.y = -glm::sin(pitch);
        forward.z = -glm::cos(yaw) * glm::cos(pitch);
        forward = glm::normalize(forward);

        float sign = isWorldYUp ? 1.f : -1.f;
        const glm::vec3 worldUp{0.f, sign * 1.f, 0.f};

        // Right vector must be perpendicular to Forward and WorldUp
        // In -Z forward, Right should be +X: cross(forward, worldUp) handles this
        rightDir = glm::normalize(glm::cross(forward, worldUp));
        // Re-calculate Up to ensure orthonormality
        upDir = glm::cross(rightDir, forward);
    }
}