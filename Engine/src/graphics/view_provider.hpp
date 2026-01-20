#pragma once

#include "graphics/camera_matrices.hpp"

namespace pxt {
    class IViewProvider {
    public:
        virtual ~IViewProvider() = default;

        // The engine calls this every frame to get the matrices for the renderer
        // The Editor and Game can implement this.
        virtual CameraMatrices getCameraMatrices(float aspectRatio) = 0;
    };
}