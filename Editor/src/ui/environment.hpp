#pragma once

#include "core/pch.hpp"

#include "graphics/frame_info.hpp"

#include "graphics/resources/vk_skybox.hpp"

namespace pxt::editor {
    class EnvironmentUi {
    public:
        void onUpdateUi(FrameInfo& frameInfo);

    private:
        void drawSkybox(Shared<VulkanSkybox> skybox);
    };
} // namespace pxt::editor