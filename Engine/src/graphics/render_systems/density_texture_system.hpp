#pragma once

#include "core/pch.hpp"
#include "graphics/context/context.hpp"
#include "graphics/descriptors/descriptors.hpp"
#include "graphics/descriptors/descriptor_manager.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/resources/texture_registry.hpp"
#include "graphics/resources/vk_image.hpp"
#include <graphics/resources/vk_buffer.hpp>
#include "graphics/frame_info.hpp"

namespace pxt {

    class DensityTextureRenderSystem {
    public:
        DensityTextureRenderSystem(Context& context, DescriptorManager& descriptorManager,
                                   VkExtent3D densityTextureExtent, VkExtent3D majorantGridExtent);
        ~DensityTextureRenderSystem();

        DensityTextureRenderSystem(const DensityTextureRenderSystem&) = delete;
        DensityTextureRenderSystem& operator=(const DensityTextureRenderSystem&) = delete;

        // Executes the compute shader to generate the textures
        void generate(FrameInfo& frameInfo);

        // Getters for the generated textures
        const VulkanImage& getDensityTexture() const { return *m_densityTexture; }

        const VulkanImage& getMajorantGrid() const { return *m_majorantGrid; }

        const VkDescriptorSet getSamplingDensitySet(const uint32_t frameIndex) const { return m_descriptorManager.getDescriptorSet(m_samplingDescriptorSet, frameIndex); }

        const DescriptorSetLayout& getSamplingDensitySetLayout() const { return m_descriptorManager.getLayout(m_samplingDescriptorSet); }

        bool needsRegeneration() const { return m_needsRegeneration; }

        void reloadShaders();
        void postFrameUpdate(VkFence frameFence);

        void updateUi(FrameInfo& frameInfo);
        void showNoiseTextures(const uint32_t frameIndex);

    private:
        void createImages();
        void createGlobalMajorantBuffer();
        void resetGlobalMajorantBuffer();
        void createDescriptorSets();

        void createGenerationPipelineLayout();
        void createGenerationPipeline(bool useCompiledSpirvFiles = true);

        void createGlobalMajorantPipelineLayout();
        void createGlobalMajorantPipeline(bool useCompiledSpirvFiles = true);

        void createSliceImageViews(VkImageView& densitySliceImageView, VkImageView& majorantSliceImageView);
        void updateSliceImageViews();

        void findMaxDensity(VkCommandBuffer commandBuffer, uint32_t frameIndex);

        Context& m_context;
        DescriptorManager& m_descriptorManager;

        VkExtent3D m_densityTextureExtent;
        VkExtent3D m_majorantGridExtent;

        Unique<VulkanImage> m_densityTexture;
        Unique<VulkanImage> m_majorantGrid;
        VkImageView m_densitySliceImageView;
        VkImageView m_majorantGridSliceImageView;

        DescriptorSetHandle m_samplingDescriptorSet = core::UID::s_invalidId;
        DescriptorSetHandle m_imGuiMajorantDescriptorSet = core::UID::s_invalidId;
        DescriptorSetHandle m_imGuiDensityDescriptorSet = core::UID::s_invalidId;
        DescriptorSetHandle m_descriptorSet = core::UID::s_invalidId;

        VkPipelineLayout m_generationPipelineLayout;
        Unique<Pipeline> m_generationPipeline;
        VkPipelineLayout m_globalMajorantPipelineLayout;
        Unique<Pipeline> m_globalMajorantPipeline;

        Unique<VulkanBuffer> m_globalMajorantBuffer;

        float m_globalMajorant = 0.0f;

        int m_noiseFrequency = 3;
        float m_worleyExponent = 2.0f;
        glm::vec4 m_fbmWeights = {0.625f, 0.25f, 0.125f, 0.0f};

        int m_densitySliceIndex = 0; // For viewing a specific slice in the UI
        bool m_needsRegeneration = true;
        bool m_hasRigeneratedThisFrame = false;

        const std::string m_generationShaderPath = "density_texture.comp";
        const std::string m_globalMajorantShaderPath = "global_majorant.comp";
    };

} // namespace pxt