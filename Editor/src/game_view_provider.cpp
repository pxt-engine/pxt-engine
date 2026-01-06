#include "game_view_provider.hpp"
#include "pxtengine.h"

namespace pxt::editor {
    CameraMatrices GameViewProvider::getCameraMatrices(float aspectRatio) {
        CameraMatrices cm{};

        if (auto activeGameCameraEntity = Application::get().getScene().getActiveCameraEntity()) {
            Entity cameraEntity = activeGameCameraEntity.value();

            const auto& cameraComp = cameraEntity.get<CameraComponent>();
            const auto& transformComp = cameraEntity.get<TransformComponent>();

            const float finalAspectRatio = cameraComp.useViewportAspectRatio ? aspectRatio : cameraComp.aspectRatio;

            glm::vec3 forward;
            glm::vec3 upDir;
            glm::vec3 rightDir;
            const float pitch = transformComp.rotation.x;
            const float yaw = transformComp.rotation.y;
            CameraMath::computeOrthonormalBasisFromPitchYaw(forward, upDir, rightDir, pitch, yaw);

            // Build view matrix
            cm.viewMatrix = CameraMath::makeViewFromDirection(transformComp.translation, forward, upDir);
            cm.inverseViewMatrix = glm::inverse(cm.viewMatrix);
            // Build projection matrix
            const CameraData& cameraData = cameraComp.cameraData;
            if (cameraData.isPerspective()) {
                cm.projectionMatrix = CameraMath::makePerspective(cameraData, finalAspectRatio);
            } else {
                cm.projectionMatrix = CameraMath::makeOrthographic(cameraData);
            }
            cm.inverseProjectionMatrix = glm::inverse(cm.projectionMatrix);

        } else {
            // TODO: black screen?
            PXT_WARN("No active camera set for GameViewProvider!");
        }

        return cm;
    }
} // namespace pxt::editor