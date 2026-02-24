#pragma once

#include "core/pch.hpp"
#include "graphics/context/context.hpp"
#include "graphics/descriptors/descriptors.hpp"
#include "graphics/descriptors/descriptor_manager.hpp"
#include "graphics/frame_info.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/resources/texture_registry.hpp"
#include "graphics/resources/vk_image.hpp"
#include "graphics/resources/vk_sampler.hpp"

namespace pxt {

    class DenoiserRenderSystem {
    public:
        DenoiserRenderSystem(Context& context, DescriptorManager& descriptorManager,
                             VkExtent2D swapChainExtent);
        ~DenoiserRenderSystem();

        DenoiserRenderSystem(const DenoiserRenderSystem&) = delete;
        DenoiserRenderSystem& operator=(const DenoiserRenderSystem&) = delete;

        // The main function to run the denoising pipeline
        void denoise(FrameInfo& frameInfo, Shared<VulkanImage> sceneImage);

        void update(GlobalUbo& ubo);
        void updateUi();

        void updateImages(VkExtent2D swapChainExtent);
        void reloadShaders();

    private:
        // Helper methods for pipeline setup
        void createSharedSampler();
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
        DescriptorManager& m_descriptorManager;

        VkExtent2D m_extent{0, 0};

        // Compute pipelines for each stage
        Unique<Pipeline> m_accumulationPipeline = nullptr;
        Unique<Pipeline> m_temporalFilterPipeline = nullptr;
        Unique<Pipeline> m_spatialFilterPipeline = nullptr; // For low-pass or bilateral filter

        // Pipeline layouts
        VkPipelineLayout m_accumulationPipelineLayout = VK_NULL_HANDLE;
        VkPipelineLayout m_temporalFilterPipelineLayout = VK_NULL_HANDLE;
        VkPipelineLayout m_spatialFilterPipelineLayout = VK_NULL_HANDLE;

        // Descriptor sets for binding resources to shaders
        DescriptorSetHandle m_accumulationDescriptorSet = core::UID::s_invalidId;
        DescriptorSetHandle m_temporalFilterDescriptorSet = core::UID::s_invalidId;
        DescriptorSetHandle m_spatialFilterDescriptorSet = core::UID::s_invalidId;

        // Resources needed for denoising
        Unique<VulkanImage> m_accumulationImage = nullptr;
        Unique<VulkanImage> m_temporalHistoryImage = nullptr; // For temporal filtering
        Unique<VulkanImage> m_tempTemporalOutputImage = nullptr;

        // Sampler for images (with nearest filtering)
        Shared<VulkanSampler> m_imageSamplerNearest = nullptr;

        std::string m_accumulationShaderPath = "accumulation.comp";
        std::string m_temporalShaderPath = "temporal.comp";
        std::string m_spatialShaderPath = "spatial_gaussian_2d.comp";

        uint32_t m_maxAccumulationFrames = UINT_MAX;
        uint32_t m_accumulationCount = 0;
        uint32_t m_frameCount = 0;

        // for UI tuning
        float m_temporalAlpha = 0.65f;
        uint32_t m_spatialKernelRadius = 2;
        float m_spatialSigmaColor = 0.1f;
        float m_spatialSigmaSpace = 0.35f;

        bool m_isAccumulationEnabled = true;
        bool m_isTemporalEnabled = true;
        bool m_isSpatialEnabled = true;
    };
} // namespace pxt