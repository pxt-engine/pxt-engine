#include "graphics/camera_matrices.hpp"

namespace pxt {
    glm::mat4 CameraMath::makeOrthographic(const CameraData& camData) {
        // GLM orthographic for Vulkan (Depth 0 to 1)
        glm::mat4 projectionMatrix =
            glm::orthoRH_ZO(camData.getOrthoLeft(), camData.getOrthoRight(), camData.getOrthoBottom(),
                            camData.getOrthoTop(), camData.getNearPlane(), camData.getFarPlane());

        // Y-flip for Vulkan
        projectionMatrix[1][1] *= -1.0f;

        return projectionMatrix;
    }

    glm::mat4 CameraMath::makePerspective(const CameraData& camData, const float aspect) {
        PXT_ASSERT(glm::abs(aspect - std::numeric_limits<float>::epsilon()) > 0.0f);

        // perspectiveRH_ZO: Right-Handed, Zero-to-One depth (Vulkan standard)
        glm::mat4 projectionMatrix = glm::perspectiveRH_ZO(glm::radians(camData.getFovYDegrees()), aspect,
                                                           camData.getNearPlane(), camData.getFarPlane());

        // Vulkan Y-axis points down, but we want Y-Up in world space
        projectionMatrix[1][1] *= -1.0f;

        return projectionMatrix;
    }

    glm::mat4 CameraMath::makeViewFromDirection(const glm::vec3 position, const glm::vec3 direction,
                                                const glm::vec3 up) {
        // lookAtRH: Right-Handed view matrix
        // Target = position + direction
        return glm::lookAtRH(position, position + direction, up);
    }

    glm::mat4 CameraMath::makeViewFromTarget(const glm::vec3 position, const glm::vec3 target, const glm::vec3 up) {
        return glm::lookAtRH(position, target, up);
    }

    glm::vec3 CameraMath::getCameraPos(const glm::mat4& inverseViewMatrix) { return inverseViewMatrix[3]; }

    void CameraMath::computeOrthonormalBasisFromPitchYaw(glm::vec3& forward, glm::vec3& upDir, glm::vec3& rightDir,
                                                         const float pitch, const float yaw, const bool isWorldYUp) {
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

    glm::vec3 CameraMath::computeWorldPositionFromScreen(const glm::vec2& screenPos, const CameraMatrices& camMatrices,
                                                         const glm::vec2& viewportSize, const float depth) {
        const glm::vec3 nearPoint =
            glm::unProjectZO(glm::vec3(screenPos, 0.f), camMatrices.viewMatrix, camMatrices.projectionMatrix,
                             glm::vec4(0, 0, viewportSize.x, viewportSize.y));

        const glm::vec3 farPoint =
            glm::unProjectZO(glm::vec3(screenPos, 1.f), camMatrices.viewMatrix, camMatrices.projectionMatrix,
                             glm::vec4(0, 0, viewportSize.x, viewportSize.y));

        const glm::vec3 rayDir = glm::normalize(farPoint - nearPoint);

        const glm::vec3 anchorPosition = CameraMath::getCameraPos(camMatrices.inverseViewMatrix) + rayDir * depth;

        return anchorPosition;
    }
} // namespace pxt