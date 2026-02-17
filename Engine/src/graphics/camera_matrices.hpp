#pragma once

#include "scene/camera_data.hpp"

namespace pxt {
    struct CameraMatrices {
        glm::mat4 projectionMatrix{1.f};
        glm::mat4 viewMatrix{1.f};
        glm::mat4 inverseViewMatrix{1.f};
        glm::mat4 inverseProjectionMatrix{1.f};
    };

    class CameraMath {
    public:
        [[nodiscard]] static glm::mat4 makeOrthographic(const CameraData& camData);

        [[nodiscard]] static glm::mat4 makePerspective(const CameraData& camData, float aspect);

        [[nodiscard]] static glm::mat4 makeViewFromDirection(const glm::vec3 position, const glm::vec3 direction,
                                                             const glm::vec3 up);

        [[nodiscard]] static glm::mat4 makeViewFromTarget(const glm::vec3 position, const glm::vec3 target,
                                                          const glm::vec3 up);

        [[nodiscard]] static glm::vec3 getCameraPos(const glm::mat4& inverseViewMatrix);

        static void computeOrthonormalBasisFromPitchYaw(glm::vec3& forward, glm::vec3& upDir, glm::vec3& rightDir,
                                                        const float pitch, const float yaw,
                                                        const bool isWorldYUp = true);

        [[nodiscard]] static glm::vec3 computeWorldPositionFromScreen(const glm::vec2& screenPos,
                                                                      const CameraMatrices& camMatrices,
                                                                      const glm::vec2& viewportSize, const float depth);
    };
} // namespace pxt