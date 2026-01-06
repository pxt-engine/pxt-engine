#pragma once

#include "graphics/view_provider.hpp"
#include "graphics/camera_matrices.hpp"

namespace pxt::editor {
    class GameViewProvider : public IViewProvider {
    public:
        CameraMatrices getCameraMatrices(float aspectRatio) override;
    };
}