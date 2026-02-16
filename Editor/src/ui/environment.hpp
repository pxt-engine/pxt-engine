#pragma once

#include "core/pch.hpp"

#include "graphics/frame_info.hpp"

namespace pxt::editor {
    class EnvironmentUi {
    public:
        void onUpdateUi(FrameInfo& frameInfo);

    private:
    };
} // namespace pxt::editor