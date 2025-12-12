#pragma once

#include "core/pch.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/context/context.hpp"
#include "graphics/descriptors/descriptors.hpp"
#include "graphics/resources/vk_image.hpp"
#include "graphics/resources/vk_buffer.hpp"
#include "graphics/frame_info.hpp"
#include "graphics/render_pass.hpp"
#include "graphics/frame_buffer.hpp"
#include "graphics/renderer.hpp"

namespace pxt {

    class ObjectPickingSystem {
    public:
        ObjectPickingSystem(
            Context& context,
            Shared<DescriptorAllocatorGrowable> descriptorAllocator,
			DescriptorSetLayout& globalSetLayout,
            VkExtent2D sceneImageExtent);
        ~ObjectPickingSystem();

        ObjectPickingSystem(const ObjectPickingSystem&) = delete;
        ObjectPickingSystem& operator=(const ObjectPickingSystem&) = delete;

        void render(FrameInfo& frameInfo, Renderer& renderer, u32vec2 mousePixelCoords);

        void updateImage(VkExtent2D newExtent);
        void reloadShaders();
        [[nodiscard]] uint32_t postFrameUpdate(VkFence frameFence);

    private:
        void createRenderPass();
        void createSceneColorIdsImage();
        void createOffscreenDepthResources();
        void createOffscreenFrameBuffer();

		void createPixelColorBuffer();

        void createPipelineLayout(DescriptorSetLayout& globalSetLayout);
        void createPipeline(bool useCompiledSpirvFiles = true);

        Context& m_context;
        Shared<DescriptorAllocatorGrowable> m_descriptorAllocator = nullptr;

        Unique<RenderPass> m_offscreenRenderPass = nullptr;
        Unique<FrameBuffer> m_offscreenFb = nullptr;

        // TODO: these images should be unique, but the framebuffer wrapper class
		// currently requires shared pointers -> change that class to receive references
        Shared<VulkanImage> m_sceneWithColorIds = nullptr;
		VkExtent2D m_sceneExtent;
        VkFormat m_offscreenColorFormat = VK_FORMAT_R8G8B8A8_UNORM;
        Shared<VulkanImage> m_offscreenDepthImage = nullptr;

        Unique<DescriptorSetLayout> m_descriptorSetLayout = nullptr;
        VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

        VkPipelineLayout m_pipelineLayout;
        Unique<Pipeline> m_pipeline = nullptr;

        Unique<VulkanBuffer> m_selectedPixelColorBuffer = nullptr;

        std::array<const std::string, 2> m_shaderFilePaths = {
            "obj_picking_shader.vert",
            "obj_picking_shader.frag"
        };
    };

}