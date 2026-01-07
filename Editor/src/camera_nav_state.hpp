#pragma once

#include "core/pch.hpp"

namespace pxt::editor {
    struct CameraNavigationState {
        // MODES
        bool freeLookEnabled = false;

        // views
        bool isPerspective = true;

        // RAW INPUTS
        glm::vec2 mouseDelta{0.0f};
        glm::vec2 scrollDelta{0.0f};

        // For these, we add +1 if we register +values and we subtract
        // -1 if input maps to a negative axis direction, adding them
        // yields the final movement intent

        // example bindings for free look
        //.X = right (+D / -A)
        // Y = up    (+E / -Q)
        // Z = forward (+W / -S)
        glm::vec3 move{0.0f};

        //. X = pitch, Y = yaw, Z = tilt
        glm::vec3 rotate{0.0f};
    };
} // namespace pxt::editor