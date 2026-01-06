#include "graphics/dummy_view_provider.hpp"

namespace pxt {
    DummyViewProvider::DummyViewProvider() {
        m_dummyCameraData = CameraData();
        m_dummyCameraData.setIsPerspective(true);
        m_dummyCameraData.setPerspectiveParams(50.f, 0.1f, 100.f);

        m_position = glm::vec3(0.5f);
        m_rotation = glm::vec2(0.f);
    }

    CameraMatrices DummyViewProvider::getCameraMatrices(float aspectRatio) { 
        CameraMatrices cm{};

        glm::vec3 forward;
        glm::vec3 upDir;
        glm::vec3 rightDir;

        CameraUtils::buildOrthonormalBasisFromPitchAndYaw(forward, upDir, rightDir, m_rotation.x, m_rotation.y);

        cm.viewMatrix = CameraUtils::setViewDirection(m_position, forward, upDir);
        cm.inverseViewMatrix = glm::inverse(cm.viewMatrix);
        cm.projectionMatrix = CameraUtils::buildPerspective(m_dummyCameraData, aspectRatio);
        cm.inverseProjectionMatrix = glm::inverse(cm.projectionMatrix);

        return cm;
    }
}