#pragma once

#include "core/pch.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/context/context.hpp"
#include "graphics/frame_info.hpp"
#include "graphics/descriptors/descriptors.hpp"
#include "graphics/resources/texture_registry.hpp"
#include "graphics/resources/vk_image.hpp"

namespace PXTEngine {

    class DenoiserRenderSystem {
    public:
        DenoiserRenderSystem(Context& context, Shared<DescriptorAllocatorGrowable> descriptorAllocator, VkExtent2D swapChainExtent);
        ~DenoiserRenderSystem();

        DenoiserRenderSystem(const DenoiserRenderSystem&) = delete;
        DenoiserRenderSystem& operator=(const DenoiserRenderSystem&) = delete;

        // The main function to run the denoising pipeline
        void denoise(FrameInfo& frameInfo, Shared<VulkanImage> sceneImage);

        // Utility to clear the accumulation buffer (e.g., on camera move)
        void resetAccumulation();

        void updateImages(VkExtent2D swapChainExtent);
        void reloadShaders();

    private:
        // Helper methods for pipeline setup
        void createImages(VkExtent2D swapChainExtent);
        void createAccumulationPipelineLayout();
        void createTemporalFilterPipelineLayout();
        void createSpatialFilterPipelineLayout();

        void createAccumulationPipeline(bool useCompiledSpirvFiles = true);
        void createTemporalFilterPipeline(bool useCompiledSpirvFiles = true);
        void createSpatialFilterPipeline(bool useCompiledSpirvFiles = true);

        // Helper methods for descriptor sets
        void createAccumulationDescriptorSet();
        void createTemporalFilterDescriptorSet();
        void createSpatialFilterDescriptorSet();

        void copyDenoisedIntoSceneImage(VkCommandBuffer commandBuffer, Shared<VulkanImage> sceneImage);

        Context& m_context;
        Shared<DescriptorAllocatorGrowable> m_descriptorAllocator;

        VkExtent2D m_extent;

        // Compute pipelines for each stage
        Unique<Pipeline> m_accumulationPipeline;
        Unique<Pipeline> m_temporalFilterPipeline;
        Unique<Pipeline> m_spatialFilterPipeline; // For low-pass or bilateral filter

        // Pipeline layouts
        VkPipelineLayout m_accumulationPipelineLayout;
        VkPipelineLayout m_temporalFilterPipelineLayout;
        VkPipelineLayout m_spatialFilterPipelineLayout;

        // Descriptor set layouts
        Unique<DescriptorSetLayout> m_accumulationDescriptorSetLayout{};
        Unique<DescriptorSetLayout> m_temporalFilterDescriptorSetLayout{};
        Unique<DescriptorSetLayout> m_spatialFilterDescriptorSetLayout{};

        // Descriptor sets for binding resources to shaders
        VkDescriptorSet m_accumulationDescriptorSet{};
        VkDescriptorSet m_temporalFilterDescriptorSet{};
        VkDescriptorSet m_spatialFilterDescriptorSet{};

        // Resources needed for denoising
        Unique<VulkanImage> m_accumulationImage;
        Unique<VulkanImage> m_temporalHistoryImage; // For temporal filtering
        Unique<VulkanImage> m_tempTemporalOutputImage;

		// Sampler for images (with nearest filtering)
		VkSampler m_imageSamplerNearest;

        std::string m_accumulationShaderPath = "accumulation.comp";
        std::string m_temporalShaderPath = "temporal.comp";
        std::string m_spatialShaderPath = "spatial.comp";

        uint32_t m_frameCount = 0;
    };
}