#pragma once

#include "graphics/camera_matrices.hpp"
#include "graphics/view_provider.hpp"
#include "scene/camera_data.hpp"

namespace pxt {
    class DummyViewProvider : public IViewProvider {
    public:
        explicit DummyViewProvider();

        CameraMatrices getCameraMatrices(float aspectRatio) override;

    private:
        CameraData m_dummyCameraData{};
        glm::vec3 m_position;
        glm::vec2 m_rotation;
    };
} // namespace pxt::editor