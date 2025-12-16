#pragma once

#include "core/pch.hpp"
#include "graphics/context/context.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/descriptors/descriptors.hpp"
#include "graphics/resources/vk_image.hpp"
#include "graphics/frame_info.hpp"

namespace pxt {

    class CompositionRenderSystem {
    public:
        CompositionRenderSystem(
            Context& context,
            DescriptorAllocatorGrowable& descriptorAllocator);

        ~CompositionRenderSystem();

        CompositionRenderSystem(const CompositionRenderSystem&) = delete;
        CompositionRenderSystem& operator=(const CompositionRenderSystem&) = delete;

        void render(
            FrameInfo& frameInfo,
            VulkanImage& sceneColor,
            VulkanImage& objectIdImage,
			VulkanImage& outputImage,
            uint32_t selectedObjectId);

        void reloadShaders();

    private:
        void createNearestSampler();
        void createDescriptorSet();
        void createPipelineLayout();
        void createPipeline(bool useCompiledSpirvFiles = true);

    private:
        Context& m_context;
        DescriptorAllocatorGrowable& m_descriptorAllocator;

        // this dscriptor set will not be needed when samplers
		// are separable from images themselves
		// (we need this DS to hold the images with nearest sampling)
		VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
        Unique<DescriptorSetLayout> m_descriptorSetLayout;

        VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
        Unique<Pipeline> m_pipeline;

        VkSampler m_nearestSampler = VK_NULL_HANDLE;

        const std::string m_shaderPath = "obj_outline.comp";
    };

}
