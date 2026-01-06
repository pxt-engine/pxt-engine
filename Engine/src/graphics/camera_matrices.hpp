#pragma once

#include "scene/camera_data.hpp"

namespace pxt {
    struct CameraMatrices {
        glm::mat4 projectionMatrix{1.f};
        glm::mat4 viewMatrix{1.f};
        glm::mat4 inverseViewMatrix{1.f};
        glm::mat4 inverseProjectionMatrix{1.f};
    };

    class CameraUtils {
    public:
        [[nodiscard]] static glm::mat4 buildOrthographic(CameraData& camData);

        [[nodiscard]] static glm::mat4 buildPerspective(CameraData& camData, float aspect);

        [[nodiscard]] static glm::mat4 setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up);

        [[nodiscard]] static glm::mat4 setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up);

        [[nodiscard]] static glm::vec3 getCameraPos(const glm::mat4& inverseViewMatrix);

        static void buildOrthonormalBasisFromPitchAndYaw(glm::vec3& forward, glm::vec3& upDir, glm::vec3& rightDir,
                                                         float pitch, float yaw, bool isWorldYUp = true);
    };
}