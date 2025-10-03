#include "graphics/render_systems/density_texture_system.hpp"

namespace PXTEngine {

    // Push constants to control noise generation in the shader
    struct DensityPushConstants {
        float noiseFrequency;
        float worleyWeight;
        float perlinWeight;
    };

    DensityTextureRenderSystem::DensityTextureRenderSystem(
        Context& context,
        Shared<DescriptorAllocatorGrowable> descriptorAllocator,
        VkExtent3D densityTextureExtent,
        VkExtent3D majorantGridExtent)
        : m_context(context),
        m_descriptorAllocator(descriptorAllocator),
        m_densityTextureExtent(densityTextureExtent),
        m_majorantGridExtent(majorantGridExtent) {

        // The workgroup size in the shader is fixed (e.g., 8x8x8).
        // The density texture dimensions must be a multiple of the majorant grid dimensions.
        PXT_ASSERT(m_densityTextureExtent.width % m_majorantGridExtent.width == 0, "Width mismatch");
        PXT_ASSERT(m_densityTextureExtent.height % m_majorantGridExtent.height == 0, "Height mismatch");
        PXT_ASSERT(m_densityTextureExtent.depth % m_majorantGridExtent.depth == 0, "Depth mismatch");

        createImages();
        createDescriptorSets();
        createPipelineLayout();
        createPipeline();
    }

    DensityTextureRenderSystem::~DensityTextureRenderSystem() {
        vkDestroyPipelineLayout(m_context.getDevice(), m_pipelineLayout, nullptr);

        vkDestroyImageView(m_context.getDevice(), m_densitySliceImageView, nullptr);
        vkDestroyImageView(m_context.getDevice(), m_majorantGridSliceImageView, nullptr);
    }

    void DensityTextureRenderSystem::createImages() {
        // Create info for the 3D density texture
        VkImageCreateInfo densityImageInfo{};
        densityImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        densityImageInfo.imageType = VK_IMAGE_TYPE_3D;
        densityImageInfo.format = VK_FORMAT_R32_SFLOAT; // Single channel for density
        densityImageInfo.extent = m_densityTextureExtent;
        densityImageInfo.mipLevels = 1;
        densityImageInfo.arrayLayers = 1;
        densityImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        densityImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        densityImageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        densityImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        densityImageInfo.flags = VK_IMAGE_CREATE_2D_VIEW_COMPATIBLE_BIT_EXT; // to view slices for debug

        m_densityTexture = createUnique<VulkanImage>(
            m_context,
            densityImageInfo,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        VkImageViewCreateInfo imageViewCreateInfo{};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
        imageViewCreateInfo.format = VK_FORMAT_R32_SFLOAT; // Must match the image format
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;

        m_densityTexture->createImageView(imageViewCreateInfo);

        // Create info for the 3D majorant grid texture
        VkImageCreateInfo majorantImageInfo = densityImageInfo;
        majorantImageInfo.extent = m_majorantGridExtent;

        m_majorantGrid = createUnique<VulkanImage>(
            m_context,
            majorantImageInfo,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        m_majorantGrid->createImageView(imageViewCreateInfo);

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_NEAREST;
		samplerInfo.minFilter = VK_FILTER_NEAREST;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.maxAnisotropy = 1.0f;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		
		m_densityTexture->createSampler(samplerInfo);
		m_majorantGrid->createSampler(samplerInfo);

        // create slice image view for imgui
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // interpret as 2D
        viewInfo.format = VK_FORMAT_R32_SFLOAT; // or whatever your 3D image format is
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        //define the slice
        viewInfo.subresourceRange.baseArrayLayer = 0; // which depth slice
        viewInfo.subresourceRange.layerCount = 1;

        viewInfo.image = m_densityTexture->getVkImage();
        m_densitySliceImageView = m_context.createImageView(viewInfo);

        viewInfo.image = m_majorantGrid->getVkImage();
        m_majorantGridSliceImageView = m_context.createImageView(viewInfo);
    }

    void DensityTextureRenderSystem::createDescriptorSets() {
        m_descriptorSetLayout = DescriptorSetLayout::Builder(m_context)
            .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT) // Density Texture Output
			.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT) // Majorant Grid Output
            .build();

        m_descriptorAllocator->allocate(m_descriptorSetLayout->getDescriptorSetLayout(), m_descriptorSet);

        // Update descriptor set immediately since the images don't change
        VkDescriptorImageInfo densityImageInfo = m_densityTexture->getImageInfo(false);
        VkDescriptorImageInfo majorantImageInfo = m_majorantGrid->getImageInfo(false);

        // TODO: manage this automatically, with the method provided by VulkanImage abstraction
        densityImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        majorantImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        DescriptorWriter(m_context, *m_descriptorSetLayout)
            .writeImage(0, &densityImageInfo)
            .writeImage(1, &majorantImageInfo)
            .updateSet(m_descriptorSet);

		// Create descriptor sets for sampling the generated textures in shaders
		m_samplingDescriptorSetLayout = DescriptorSetLayout::Builder(m_context)
			.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT |
              VK_SHADER_STAGE_RAYGEN_BIT_KHR | 
              VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT |
                VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                VK_SHADER_STAGE_RAYGEN_BIT_KHR)
			.build();

		m_descriptorAllocator->allocate(m_samplingDescriptorSetLayout->getDescriptorSetLayout(), m_samplingDescriptorSet);

        // TODO: manage this automatically, with the method provided by VulkanImage abstraction
        // Update descriptor set immediately since the images don't change
        densityImageInfo = m_densityTexture->getImageInfo();
        densityImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        majorantImageInfo = m_majorantGrid->getImageInfo();
        majorantImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		DescriptorWriter(m_context, *m_samplingDescriptorSetLayout)
			.writeImage(0, &densityImageInfo)
            .writeImage(1, &majorantImageInfo)
			.updateSet(m_samplingDescriptorSet);

        // Create descriptor sets for sampling the generated textures within ImGui
        m_imGuiDescriptorSetLayout = DescriptorSetLayout::Builder(m_context)
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();

        // DENSITY TEXTURE IMGUI
        densityImageInfo.imageView = m_densitySliceImageView;

        m_descriptorAllocator->allocate(m_imGuiDescriptorSetLayout->getDescriptorSetLayout(), m_imGuiDensityDescriptorSet);

        DescriptorWriter(m_context, *m_imGuiDescriptorSetLayout)
            .writeImage(0, &densityImageInfo)
            .updateSet(m_imGuiDensityDescriptorSet);

        // MAJORANT GRID TEXTURE IMGUI
        majorantImageInfo.imageView = m_majorantGridSliceImageView;

        m_descriptorAllocator->allocate(m_imGuiDescriptorSetLayout->getDescriptorSetLayout(), m_imGuiMajorantDescriptorSet);

        DescriptorWriter(m_context, *m_imGuiDescriptorSetLayout)
            .writeImage(0, &majorantImageInfo)
            .updateSet(m_imGuiMajorantDescriptorSet);
    }

    void DensityTextureRenderSystem::createPipelineLayout() {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(DensityPushConstants);

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{ m_descriptorSetLayout->getDescriptorSetLayout() };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(m_context.getDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create density texture pipeline layout!");
        }
    }

    void DensityTextureRenderSystem::createPipeline(bool useCompiledSpirvFiles) {
        PXT_ASSERT(m_pipelineLayout != nullptr, "Cannot create pipeline before pipeline layout");

        ComputePipelineConfigInfo pipelineConfig{};
        pipelineConfig.pipelineLayout = m_pipelineLayout;

        const std::string baseShaderPath = useCompiledSpirvFiles ? SPV_SHADERS_PATH : SHADERS_PATH;
        const std::string filenameSuffix = useCompiledSpirvFiles ? ".spv" : "";
        std::string shaderFilePath = baseShaderPath + m_shaderPath + filenameSuffix;

        m_pipeline = createUnique<Pipeline>(m_context, shaderFilePath, pipelineConfig);
    }

    void DensityTextureRenderSystem::generate(VkCommandBuffer commandBuffer) {
        // Transition images to GENERAL layout for storage image access
        m_densityTexture->transitionImageLayout(
            commandBuffer,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
        );
        m_majorantGrid->transitionImageLayout(
            commandBuffer,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
        );

        // Bind pipeline and descriptor sets
        m_pipeline->bind(commandBuffer);
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            m_pipelineLayout,
            0, 1, &m_descriptorSet,
            0, nullptr
        );

        // Push constants to control the noise
        DensityPushConstants pushConstants{};
        pushConstants.noiseFrequency = m_noiseFrequency; // Higher value = more detail
        pushConstants.worleyWeight = m_worleyWeight;   // How much the cell-like structure influences the shape
        pushConstants.perlinWeight = m_perlinWeight;   // How much classic turbulence is applied

        vkCmdPushConstants(
            commandBuffer,
            m_pipelineLayout,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(DensityPushConstants),
            &pushConstants
        );

        // Dispatch compute shaders. One workgroup per majorant grid cell.
        vkCmdDispatch(
            commandBuffer,
            m_majorantGridExtent.width,
            m_majorantGridExtent.height,
            m_majorantGridExtent.depth
        );

        // TODO: move this into a separate function with the ability to specify
        // which stage to wait for (dstStage), could be RT or FRAGMENT depending on
        // RT enabled or not.
        // Transition images to SHADER READ ONLY OPTIMAL layout
        m_densityTexture->transitionImageLayout(
            commandBuffer,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR
        );
        m_majorantGrid->transitionImageLayout(
            commandBuffer,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR
        );

        m_needsRegeneration = false;
    }

    void DensityTextureRenderSystem::updateUi() {
        if (ImGui::CollapsingHeader("Volume Noise Settings")) {
            ImGui::Text("Adjust noise parameters and regenerate the volume.");

            // Sliders for each parameter
            ImGui::DragFloat("Noise Frequency", &m_noiseFrequency, 0.1f, 0.1f, 32.0f);
            ImGui::DragFloat("Worley Weight", &m_worleyWeight, 0.05f, 0.0f, 5.0f);
            ImGui::DragFloat("Perlin Weight", &m_perlinWeight, 0.05f, 0.0f, 2.0f);

            ImGui::Separator();

            // Button to trigger the regeneration
            if (ImGui::Button("Regenerate Volume")) {
                m_needsRegeneration = true;
            }

            showNoiseTextures();
        }
    }

    void DensityTextureRenderSystem::showNoiseTextures() {
        ImTextureID noiseTexture = (ImTextureID) m_imGuiDensityDescriptorSet;

        // we push a style var to remove the viewpoer window padding
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Density Texture");

        // we see the size of the window and we make the image fit the window with an aspect ratio
        ImVec2 windowSize = ImGui::GetContentRegionAvail();

        // Calculate the horizontal and vertical offsets for centering
        float titleBarSize = ImGui::GetFrameHeight() * 2;
        float offsetX = (windowSize.x - m_densityTextureExtent.width) * 0.5f;
        float offsetY = (windowSize.y - m_densityTextureExtent.height + titleBarSize) * 0.5f;

        // Move the cursor to the calculated position
        // ImGui::SetCursorPos() sets the next drawing position relative to the top-left of the *content region*.
        ImGui::SetCursorPos(ImVec2(offsetX, offsetY));

        ImGui::Image(noiseTexture, windowSize);
        ImGui::End();
        ImGui::PopStyleVar();

        // for majorant grid
        ImTextureID majorantTexture = (ImTextureID) m_imGuiMajorantDescriptorSet;

        // we push a style var to remove the viewpoer window padding
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Density Majorant Grid Texture");

        // we see the size of the window and we make the image fit the window with an aspect ratio
        windowSize = ImGui::GetContentRegionAvail();

        // Calculate the horizontal and vertical offsets for centering
        titleBarSize = ImGui::GetFrameHeight() * 2;
        offsetX = (windowSize.x - m_densityTextureExtent.width) * 0.5f;
        offsetY = (windowSize.y - m_densityTextureExtent.height + titleBarSize) * 0.5f;

        // Move the cursor to the calculated position
        // ImGui::SetCursorPos() sets the next drawing position relative to the top-left of the *content region*.
        ImGui::SetCursorPos(ImVec2(offsetX, offsetY));

        ImGui::Image(majorantTexture, windowSize);
        ImGui::End();
        ImGui::PopStyleVar();
    }

}