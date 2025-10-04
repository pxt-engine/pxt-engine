#pragma once

#include "core/pch.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/context/context.hpp"
#include "graphics/descriptors/descriptors.hpp"
#include "graphics/resources/texture_registry.hpp"
#include "graphics/resources/vk_image.hpp"

namespace PXTEngine {

    class DensityTextureRenderSystem {
    public:
        DensityTextureRenderSystem(
            Context& context,
            Shared<DescriptorAllocatorGrowable> descriptorAllocator,
            VkExtent3D densityTextureExtent,
            VkExtent3D majorantGridExtent);
        ~DensityTextureRenderSystem();

        DensityTextureRenderSystem(const DensityTextureRenderSystem&) = delete;
        DensityTextureRenderSystem& operator=(const DensityTextureRenderSystem&) = delete;

        // Executes the compute shader to generate the textures
        void generate(VkCommandBuffer commandBuffer);

        // Getters for the generated textures
        const VulkanImage& getDensityTexture() const { return *m_densityTexture; }
        const VulkanImage& getMajorantGrid() const { return *m_majorantGrid; }
        const VkDescriptorSet getSamplingDensitySet() const { return m_samplingDescriptorSet; }
        const Shared<DescriptorSetLayout> getSamplingDensitySetLayout() const { return m_samplingDescriptorSetLayout; }

        bool needsRegeneration() const { return m_needsRegeneration; }
        
        void reloadShaders();

        void updateUi();
        void showNoiseTextures();

    private:
        void createImages();
        void createDescriptorSets();
        void createPipelineLayout();
        void createPipeline(bool useCompiledSpirvFiles = true);

        void createSliceImageViews(VkImageView* densitySliceImageView, VkImageView* majorantSliceImageView);
        void updateSliceImageViews();

        Context& m_context;
        Shared<DescriptorAllocatorGrowable> m_descriptorAllocator;

        VkExtent3D m_densityTextureExtent;
        VkExtent3D m_majorantGridExtent;

        Unique<VulkanImage> m_densityTexture;
        Unique<VulkanImage> m_majorantGrid;
        VkImageView m_densitySliceImageView;
        VkImageView m_majorantGridSliceImageView;

        Unique<DescriptorSetLayout> m_descriptorSetLayout;
		Shared<DescriptorSetLayout> m_samplingDescriptorSetLayout;
        Shared<DescriptorSetLayout> m_imGuiDescriptorSetLayout;
		VkDescriptorSet m_samplingDescriptorSet = VK_NULL_HANDLE;
        VkDescriptorSet m_imGuiMajorantDescriptorSet = VK_NULL_HANDLE;
        VkDescriptorSet m_imGuiDensityDescriptorSet = VK_NULL_HANDLE;
        VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

        VkPipelineLayout m_pipelineLayout;
        Unique<Pipeline> m_pipeline;

        int m_noiseFrequency = 3;
        float m_worleyExponent = 2.0f;
		int m_densitySliceIndex = 0; // For viewing a specific slice in the UI
        bool m_needsRegeneration = true;

        const std::string m_shaderPath = "density_texture.comp";
    };

}