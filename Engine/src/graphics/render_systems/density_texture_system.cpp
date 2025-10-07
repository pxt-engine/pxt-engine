#include "graphics/render_systems/density_texture_system.hpp"

namespace PXTEngine {

    // Push constants to control noise generation in the shader
    struct DensityPushConstants {
        float noiseFrequency;
        float worleyExponent;
    };

    // buffer holdig the majorant max
	struct GlobalMajorantBuffer {
		float globalMajorant = 0.0f;
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
        createGlobalMajorantBuffer();
        createDescriptorSets();

        createGenerationPipelineLayout();
        createGenerationPipeline();

		createGlobalMajorantPipelineLayout();
		createGlobalMajorantPipeline();
    }

    DensityTextureRenderSystem::~DensityTextureRenderSystem() {
        vkDestroyPipelineLayout(m_context.getDevice(), m_generationPipelineLayout, nullptr);
		vkDestroyPipelineLayout(m_context.getDevice(), m_globalMajorantPipelineLayout, nullptr);

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

        createSliceImageViews(&m_densitySliceImageView, &m_majorantGridSliceImageView);
    }

    void DensityTextureRenderSystem::createGlobalMajorantBuffer() {
		GlobalMajorantBuffer globalMajorantData{};
		globalMajorantData.globalMajorant = 0.0f;
        
        m_globalMajorantBuffer = createUnique<VulkanBuffer>(
			m_context,
			sizeof(GlobalMajorantBuffer),
            1,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT // we need to see it from the cpu
		);

		m_globalMajorantBuffer->map();
		m_globalMajorantBuffer->writeToBuffer(&globalMajorantData);
		m_globalMajorantBuffer->unmap();
    }

    void DensityTextureRenderSystem::createDescriptorSets() {
        m_descriptorSetLayout = DescriptorSetLayout::Builder(m_context)
            .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT) // Density Texture Output
			.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT) // Majorant Grid Output
			.addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // Global Majorant Buffer
            .build();

        m_descriptorAllocator->allocate(m_descriptorSetLayout->getDescriptorSetLayout(), m_descriptorSet);

        // Update descriptor set immediately since the images don't change
        VkDescriptorImageInfo densityImageInfo = m_densityTexture->getImageInfo(false);
        VkDescriptorImageInfo majorantImageInfo = m_majorantGrid->getImageInfo(false);
		VkDescriptorBufferInfo globalMajorantBufferInfo = m_globalMajorantBuffer->descriptorInfo();

        // TODO: manage this automatically, with the method provided by VulkanImage abstraction
        densityImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        majorantImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        DescriptorWriter(m_context, *m_descriptorSetLayout)
            .writeImage(0, &densityImageInfo)
            .writeImage(1, &majorantImageInfo)
			.writeBuffer(2, &globalMajorantBufferInfo)
            .updateSet(m_descriptorSet);

		// Create descriptor sets for sampling the generated textures in shaders
		m_samplingDescriptorSetLayout = DescriptorSetLayout::Builder(m_context)
			.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT |
              VK_SHADER_STAGE_RAYGEN_BIT_KHR | 
              VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT |
                VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
            .addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
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
			.writeBuffer(2, &globalMajorantBufferInfo)
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

    void DensityTextureRenderSystem::createGenerationPipelineLayout() {
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

        if (vkCreatePipelineLayout(m_context.getDevice(), &pipelineLayoutInfo, nullptr, &m_generationPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create density texture pipeline layout!");
        }
    }

    void DensityTextureRenderSystem::createGenerationPipeline(bool useCompiledSpirvFiles) {
        PXT_ASSERT(m_generationPipelineLayout != nullptr, "Cannot create pipeline before pipeline layout");

        ComputePipelineConfigInfo pipelineConfig{};
        pipelineConfig.pipelineLayout = m_generationPipelineLayout;

        const std::string baseShaderPath = useCompiledSpirvFiles ? SPV_SHADERS_PATH : SHADERS_PATH;
        const std::string filenameSuffix = useCompiledSpirvFiles ? ".spv" : "";
        std::string shaderFilePath = baseShaderPath + m_generationShaderPath + filenameSuffix;

        m_generationPipeline = createUnique<Pipeline>(m_context, shaderFilePath, pipelineConfig);
    }

    void DensityTextureRenderSystem::createGlobalMajorantPipelineLayout() {
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{ m_descriptorSetLayout->getDescriptorSetLayout() };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(m_context.getDevice(), &pipelineLayoutInfo, nullptr, &m_globalMajorantPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create global majorant pipeline layout!");
        }
    }

    void DensityTextureRenderSystem::createGlobalMajorantPipeline(bool useCompiledSpirvFiles) {
        PXT_ASSERT(m_globalMajorantPipelineLayout != nullptr, "Cannot create global majorant pipeline before pipeline layout");

        ComputePipelineConfigInfo pipelineConfig{};
        pipelineConfig.pipelineLayout = m_globalMajorantPipelineLayout;

        const std::string baseShaderPath = useCompiledSpirvFiles ? SPV_SHADERS_PATH : SHADERS_PATH;
        const std::string filenameSuffix = useCompiledSpirvFiles ? ".spv" : "";
        std::string shaderFilePath = baseShaderPath + m_globalMajorantShaderPath + filenameSuffix;

        m_globalMajorantPipeline = createUnique<Pipeline>(m_context, shaderFilePath, pipelineConfig);
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
        m_generationPipeline->bind(commandBuffer);
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            m_generationPipelineLayout,
            0, 1, &m_descriptorSet,
            0, nullptr
        );

        // Push constants to control the noise
        DensityPushConstants pushConstants{};
        pushConstants.noiseFrequency = static_cast<float>(m_noiseFrequency); // Higher value = more detail
        pushConstants.worleyExponent = m_worleyExponent;   // How much the cell-like structure influences the shape

        vkCmdPushConstants(
            commandBuffer,
            m_generationPipelineLayout,
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

		findMaxDensity(commandBuffer);

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

    void DensityTextureRenderSystem::findMaxDensity(VkCommandBuffer commandBuffer) {
        // Bind pipeline and descriptor sets
        m_globalMajorantPipeline->bind(commandBuffer);
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            m_globalMajorantPipelineLayout,
            0, 1, &m_descriptorSet,
            0, nullptr
        );

        // Dispatch compute shaders. One workgroup per majorant grid cell.
        vkCmdDispatch(
            commandBuffer,
            m_majorantGridExtent.width,
            m_majorantGridExtent.height,
            m_majorantGridExtent.depth
        );

        // memory barrier
        VkMemoryBarrier memoryBarrier = {};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        // Source: What the GPU did before the barrier
        memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; // The shader wrote to the buffer
        // Destination: What the CPU will do after the barrier
        memoryBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;    // The host will read the buffer

        // Record the barrier command
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // Stage where writing happened
            VK_PIPELINE_STAGE_HOST_BIT,           // Stage where reading will happen
            0,
            1, &memoryBarrier,
            0, nullptr,
            0, nullptr
        );
    }

    void DensityTextureRenderSystem::reloadShaders() {
        PXT_INFO("Reloading shaders...");
        createGenerationPipeline(false);
    }

    void DensityTextureRenderSystem::postFrameUpdate() {
		// read back the global majorant value
        m_globalMajorantBuffer->map();
        m_globalMajorant = *((float*)m_globalMajorantBuffer->getMappedMemory());
        m_globalMajorantBuffer->unmap();
    }

    void DensityTextureRenderSystem::updateUi() {
        if (ImGui::CollapsingHeader("Volume Noise Settings")) {
            ImGui::Text("Global majorant value: %.2f", m_globalMajorant);

            if (ImGui::SliderInt("Noise Frequency", &m_noiseFrequency, 0, 32)) {
                m_needsRegeneration = true;
            }

            if (ImGui::DragFloat("Worley Weight", &m_worleyExponent, 0.05f, 0.0f, 5.0f)) {
				m_needsRegeneration = true;
            }

            if (ImGui::SliderInt("Density Texture Depth Slice", &m_densitySliceIndex, 0, m_densityTextureExtent.depth - 1)) {
                updateSliceImageViews();
            }

            ImGui::Separator();

            // Button to trigger the regeneration
            if (ImGui::Button("Regenerate Volume")) {
                m_needsRegeneration = true;
            }

            showNoiseTextures();
        }
    }

    void DensityTextureRenderSystem::showNoiseTextures() {
        // Remove window padding so images butt up against window edges
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImVec2 windowSize(200, 200);

		// Density Texture
        ImTextureID noiseTexture = (ImTextureID)m_imGuiDensityDescriptorSet;
        ImGui::Image(noiseTexture, windowSize);

        // Move to the same line (to the right of the previous image)
        ImGui::SameLine();

		// Majorant Grid Texture
        ImTextureID majorantTexture = (ImTextureID)m_imGuiMajorantDescriptorSet;
        ImGui::Image(majorantTexture, windowSize);

        ImGui::PopStyleVar();
    }

    void DensityTextureRenderSystem::createSliceImageViews(VkImageView* densitySliceImageView, VkImageView* majorantSliceImageView) {
        // create slice image view for imgui
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // interpret as 2D
        viewInfo.format = VK_FORMAT_R32_SFLOAT; // or whatever your 3D image format is
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        //define the slice
        viewInfo.subresourceRange.baseArrayLayer = m_densitySliceIndex; // which depth slice
        viewInfo.subresourceRange.layerCount = 1;

        viewInfo.image = m_densityTexture->getVkImage();
        *densitySliceImageView = m_context.createImageView(viewInfo);

        viewInfo.image = m_majorantGrid->getVkImage();
        viewInfo.subresourceRange.baseArrayLayer = m_densitySliceIndex / m_majorantGridExtent.depth; // for majorant grid
        *majorantSliceImageView = m_context.createImageView(viewInfo);
    }

    void DensityTextureRenderSystem::updateSliceImageViews() {
        // create slice image view for imgui
		VkImageView densitySliceImageView, majorantGridSliceImageView;
		createSliceImageViews(&densitySliceImageView, &majorantGridSliceImageView);

        if (m_densitySliceImageView != VK_NULL_HANDLE && m_majorantGridSliceImageView != VK_NULL_HANDLE) {
            // we need to wait for the device to finish
            vkDeviceWaitIdle(m_context.getDevice());

            // then update the descriptor sets for imgui
            VkDescriptorImageInfo densityImageInfo = m_densityTexture->getImageInfo();
            densityImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VkDescriptorImageInfo majorantImageInfo = m_majorantGrid->getImageInfo();
            majorantImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            densityImageInfo.imageView = densitySliceImageView;

            DescriptorWriter(m_context, *m_imGuiDescriptorSetLayout)
                .writeImage(0, &densityImageInfo)
                .updateSet(m_imGuiDensityDescriptorSet);

            // MAJORANT GRID TEXTURE IMGUI
            majorantImageInfo.imageView = majorantGridSliceImageView;

            DescriptorWriter(m_context, *m_imGuiDescriptorSetLayout)
                .writeImage(0, &majorantImageInfo)
                .updateSet(m_imGuiMajorantDescriptorSet);

            // then destroy the old ones
            vkDestroyImageView(m_context.getDevice(), m_densitySliceImageView, nullptr);
            vkDestroyImageView(m_context.getDevice(), m_majorantGridSliceImageView, nullptr);
        }

        // then assign the new ones regardless
        m_densitySliceImageView = densitySliceImageView;
        m_majorantGridSliceImageView = majorantGridSliceImageView;
    }
}