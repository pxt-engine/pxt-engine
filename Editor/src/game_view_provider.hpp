#pragma once

#include "graphics/camera_matrices.hpp"
#include "graphics/view_provider.hpp"

namespace pxt::editor {
    class GameViewProvider : public IViewProvider {
    public:
        CameraMatrices getCameraMatrices(float aspectRatio) override;
    };
} // namespace pxt::editor