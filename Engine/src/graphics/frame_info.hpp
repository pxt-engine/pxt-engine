#pragma once

#include "core/pch.hpp"
#include "scene/camera.hpp"
#include "scene/scene.hpp"

namespace PXTEngine {
    struct PointLight {
        glm::vec4 position{};
        glm::vec4 color{}; // w is intensity
    };

    struct GlobalUbo {
        glm::mat4 projection{1.f};
        glm::mat4 view{1.f};
        glm::mat4 inverseView{1.f};
        glm::vec4 ambientLightColor{0.67f, 0.85f, 0.9f, .02f};
        PointLight pointLights[MAX_LIGHTS];
        int numLights;
        uint32_t frameCount;
    };

    struct FrameInfo {
        int frameIndex;
        float frameTime;
        VkCommandBuffer commandBuffer;
        Camera& camera;
        VkDescriptorSet globalDescriptorSet;
        Scene& scene;
        VkFence frameFence;        // The fence signaled when the command buffer is complete
        VkSemaphore imageAvailable; // The semaphore signaled when the image is available
        VkSemaphore renderFinished; // The semaphore signaled when rendering is done
    };
}