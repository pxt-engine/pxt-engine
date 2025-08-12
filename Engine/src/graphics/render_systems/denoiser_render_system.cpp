#include "graphics/render_systems/denoiser_render_system.hpp"

namespace PXTEngine {

    // Struct for push constants, if needed
    struct DenoiserPushConstantData {
        uint32_t frameCount;
        // Add more push constants as needed for temporal/spatial filters (e.g., thresholds, filter radii)
        float temporalAlpha;
        float spatialSigmaColor;
        float spatialSigmaSpace;
    };

    DenoiserRenderSystem::DenoiserRenderSystem(Context& context, Shared<DescriptorAllocatorGrowable> descriptorAllocator, VkExtent2D swapChainExtent)
        : m_context(context), m_descriptorAllocator(descriptorAllocator), m_extent(swapChainExtent) {

        createImages(swapChainExtent);

        createAccumulationDescriptorSet();
        createTemporalFilterDescriptorSet();
        createSpatialFilterDescriptorSet(); // For low-pass or bilateral filter

        createAccumulationPipelineLayout();
        createTemporalFilterPipelineLayout();
        createSpatialFilterPipelineLayout();

        createAccumulationPipeline();
        createTemporalFilterPipeline();
        createSpatialFilterPipeline();
    }

    DenoiserRenderSystem::~DenoiserRenderSystem() {
        vkDestroyPipelineLayout(m_context.getDevice(), m_accumulationPipelineLayout, nullptr);
        vkDestroyPipelineLayout(m_context.getDevice(), m_temporalFilterPipelineLayout, nullptr);
        vkDestroyPipelineLayout(m_context.getDevice(), m_spatialFilterPipelineLayout, nullptr);
    }

    void DenoiserRenderSystem::createImages(VkExtent2D extent) {
		// imagecreateinfo for accumulation, history, and temporary output images
		VkImageCreateInfo imageCreateInfo = {};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.extent.width = extent.width;
		imageCreateInfo.extent.height = extent.height;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.flags = 0;
		imageCreateInfo.pNext = nullptr;

		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = VK_NULL_HANDLE; // Will be set later
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;

        // we create a single simpler for every image
		VkSamplerCreateInfo samplerCreateInfo = {};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // Clamp to edge to avoid artifacts on the borders
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // Not used, but set for completeness
		samplerCreateInfo.unnormalizedCoordinates = VK_TRUE; // We use unnormalized coordinates for direct texel access

        m_imageSamplerNearest = m_context.createSampler(samplerCreateInfo);

        // Create an accumulation buffer
        // This buffer accumulates raw path-traced samples.
        m_accumulationImage = createUnique<VulkanImage>(
            m_context,
			imageCreateInfo,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
		imageViewCreateInfo.image = m_accumulationImage->getVkImage();

        m_accumulationImage->
             createImageView(imageViewCreateInfo)
            .setImageSampler(m_imageSamplerNearest);

        // Create a temporary buffer for the output of the temporal filter.
        // This serves as input for the spatial filter.
        m_tempTemporalOutputImage = createUnique<VulkanImage>(
            m_context,
            imageCreateInfo,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        imageViewCreateInfo.image = m_tempTemporalOutputImage->getVkImage();

        m_tempTemporalOutputImage->
            createImageView(imageViewCreateInfo)
            .setImageSampler(m_imageSamplerNearest);

        // Create a history buffer for temporal filtering.
        // This buffer stores the final denoised output of the PREVIOUS frame,
        // and will store the final denoised output of the CURRENT frame.
		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT; // Allow transfer for copying to scene image later

        m_temporalHistoryImage = createUnique<VulkanImage>(
            m_context,
            imageCreateInfo,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        imageViewCreateInfo.image = m_temporalHistoryImage->getVkImage();

        m_temporalHistoryImage->
             createImageView(imageViewCreateInfo)
            .setImageSampler(m_imageSamplerNearest);
    }

    void DenoiserRenderSystem::createAccumulationDescriptorSet() {
        // Binding 0: New noisy frame (read as a sampled image from the path tracer)
        // Binding 1: Accumulation buffer (read/write as a storage image)
        m_accumulationDescriptorSetLayout = DescriptorSetLayout::Builder(m_context)
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .build();

        m_descriptorAllocator->allocate(m_accumulationDescriptorSetLayout->getDescriptorSetLayout(), m_accumulationDescriptorSet);

        // The descriptor set will be updated at runtime with the new frame's image info
        // and the accumulation buffer's image info.
    }

    void DenoiserRenderSystem::createTemporalFilterDescriptorSet() {
        // Binding 0: Accumulation buffer (read as sampled image)
        // Binding 1: History buffer (read as sampled image - previous frame's denoised output)
        // Binding 2: New noisy frame (read as sampled image - for motion detection)
        // Binding 3: Temporary temporal output buffer (write as storage image)
        m_temporalFilterDescriptorSetLayout = DescriptorSetLayout::Builder(m_context)
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .build();

        m_descriptorAllocator->allocate(m_temporalFilterDescriptorSetLayout->getDescriptorSetLayout(), m_temporalFilterDescriptorSet);
    }

    void DenoiserRenderSystem::createSpatialFilterDescriptorSet() {
        // Binding 0: Temporary temporal output buffer (read as sampled image)
        // Binding 1: New noisy frame (read as sampled image - for bilateral color guidance)
        // Binding 2: History buffer (write as storage image - final denoised output for current frame)
        m_spatialFilterDescriptorSetLayout = DescriptorSetLayout::Builder(m_context)
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .build();

        m_descriptorAllocator->allocate(m_spatialFilterDescriptorSetLayout->getDescriptorSetLayout(), m_spatialFilterDescriptorSet);
    }


    void DenoiserRenderSystem::createAccumulationPipelineLayout() {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(DenoiserPushConstantData); // Only frameCount needed here

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{
            m_accumulationDescriptorSetLayout->getDescriptorSetLayout()
        };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(m_context.getDevice(), &pipelineLayoutInfo, nullptr, &m_accumulationPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create accumulation pipeline layout!");
        }
    }

    void DenoiserRenderSystem::createTemporalFilterPipelineLayout() {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(DenoiserPushConstantData); // For frameCount, temporalAlpha

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{
            m_temporalFilterDescriptorSetLayout->getDescriptorSetLayout()
        };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(m_context.getDevice(), &pipelineLayoutInfo, nullptr, &m_temporalFilterPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create temporal filter pipeline layout!");
        }
    }

    void DenoiserRenderSystem::createSpatialFilterPipelineLayout() {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(DenoiserPushConstantData); // For spatialSigmaColor, spatialSigmaSpace

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{
            m_spatialFilterDescriptorSetLayout->getDescriptorSetLayout()
        };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(m_context.getDevice(), &pipelineLayoutInfo, nullptr, &m_spatialFilterPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create spatial filter pipeline layout!");
        }
    }

    void DenoiserRenderSystem::createAccumulationPipeline(bool useCompiledSpirvFiles) {
        PXT_ASSERT(m_accumulationPipelineLayout != nullptr, "Cannot create accumulation pipeline before pipelineLayout");

        ComputePipelineConfigInfo pipelineConfig{};
        pipelineConfig.pipelineLayout = m_accumulationPipelineLayout;

        const std::string baseShaderPath = useCompiledSpirvFiles ? SPV_SHADERS_PATH : SHADERS_PATH + "raytracing/denoising/";
        const std::string filenameSuffix = useCompiledSpirvFiles ? ".spv" : "";

        std::string shaderFilePath = baseShaderPath + m_accumulationShaderPath + filenameSuffix;

        m_accumulationPipeline = createUnique<Pipeline>(
            m_context,
            shaderFilePath,
            pipelineConfig
        );
    }

    void DenoiserRenderSystem::createTemporalFilterPipeline(bool useCompiledSpirvFiles) {
        PXT_ASSERT(m_temporalFilterPipelineLayout != nullptr, "Cannot create temporal filter pipeline before pipelineLayout");

        ComputePipelineConfigInfo pipelineConfig{};
        pipelineConfig.pipelineLayout = m_temporalFilterPipelineLayout;

        const std::string baseShaderPath = useCompiledSpirvFiles ? SPV_SHADERS_PATH : SHADERS_PATH + "raytracing/denoising/";
        const std::string filenameSuffix = useCompiledSpirvFiles ? ".spv" : "";

        std::string shaderFilePath = baseShaderPath + m_temporalShaderPath + filenameSuffix;

        m_temporalFilterPipeline = createUnique<Pipeline>(
            m_context,
            shaderFilePath,
            pipelineConfig
        );
    }

    void DenoiserRenderSystem::createSpatialFilterPipeline(bool useCompiledSpirvFiles) {
        PXT_ASSERT(m_spatialFilterPipelineLayout != nullptr, "Cannot create spatial filter pipeline before pipelineLayout");

        ComputePipelineConfigInfo pipelineConfig{};
        pipelineConfig.pipelineLayout = m_spatialFilterPipelineLayout;

        const std::string baseShaderPath = useCompiledSpirvFiles ? SPV_SHADERS_PATH : SHADERS_PATH + "raytracing/denoising/";
        const std::string filenameSuffix = useCompiledSpirvFiles ? ".spv" : "";

        std::string shaderFilePath = baseShaderPath + m_spatialShaderPath + filenameSuffix;

        m_spatialFilterPipeline = createUnique<Pipeline>(
            m_context,
            shaderFilePath,
            pipelineConfig
        );
    }

    void DenoiserRenderSystem::denoise(FrameInfo& frameInfo, Shared<VulkanImage> sceneImage) {
        m_frameCount++;
        VkCommandBuffer commandBuffer = frameInfo.commandBuffer;

		VkDescriptorImageInfo newFrameImageInfo = sceneImage->getImageInfo();
		newFrameImageInfo.sampler = m_imageSamplerNearest; // Use nearest sampler for denoising

        VkDescriptorImageInfo accumulationImageInfo;
		VkDescriptorImageInfo temporalHistoryImageInfo;
		VkDescriptorImageInfo tempTemporalOutputImageInfo;

        // Calculate work group dimensions
		const uint32_t workGroupSize = 16;
        const uint32_t workGroupCountX = (m_extent.width + workGroupSize - 1) / workGroupSize;
        const uint32_t workGroupCountY = (m_extent.height + workGroupSize - 1) / workGroupSize;

        
        // --- Pass 1: Accumulation ---
        // Inputs: newFrameImageInfo (noisy path-traced frame)
        // Output: m_accumulationImage (accumulated samples)
        // m_accumulationImage is used as both read and write storage image,
        // so its layout should be VK_IMAGE_LAYOUT_GENERAL.
        m_accumulationImage->transitionImageLayout(
            commandBuffer,
            VK_IMAGE_LAYOUT_GENERAL, // Transition to general layout for storage
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
        );

		accumulationImageInfo = m_accumulationImage->getImageInfo(false); // Storage image info

        DescriptorWriter(m_context, *m_accumulationDescriptorSetLayout)
            .writeImage(0, &newFrameImageInfo) // New noisy frame (sampled)
            .writeImage(1, &accumulationImageInfo) // Accumulation buffer (storage)
            .updateSet(m_accumulationDescriptorSet);

        m_accumulationPipeline->bind(commandBuffer);
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            m_accumulationPipelineLayout,
            0, 1, &m_accumulationDescriptorSet,
            0, nullptr
        );

        DenoiserPushConstantData accumulationPush{};
        accumulationPush.frameCount = m_frameCount;
        vkCmdPushConstants(
            commandBuffer,
            m_accumulationPipelineLayout,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(uint32_t), &accumulationPush.frameCount // Only frameCount for this shader
        );

        vkCmdDispatch(commandBuffer, workGroupCountX, workGroupCountY, 1);

        // --- Pass 2: Temporal Filter ---
        // Inputs: m_accumulationImage (from Pass 1), m_temporalHistoryImage (previous frame's final output), newFrameImageInfo (raw)
        // Output: m_tempTemporalOutputImage

        m_accumulationImage->transitionImageLayout(
            commandBuffer,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
        );

        m_temporalHistoryImage->transitionImageLayout(
            commandBuffer,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT, // because it was copied from in the previous frame
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
        );

        m_tempTemporalOutputImage->transitionImageLayout(
            commandBuffer,
            VK_IMAGE_LAYOUT_GENERAL, // General layout for storage
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
        );

		accumulationImageInfo = m_accumulationImage->getImageInfo(); // Sampled image info
		temporalHistoryImageInfo = m_temporalHistoryImage->getImageInfo(); // Sampled image info
		tempTemporalOutputImageInfo = m_tempTemporalOutputImage->getImageInfo(false); // Storage image info

        DescriptorWriter(m_context, *m_temporalFilterDescriptorSetLayout)
            .writeImage(0, &accumulationImageInfo) // Accumulation (sampled)
            .writeImage(1, &temporalHistoryImageInfo) // History (sampled)
            .writeImage(2, &newFrameImageInfo) // New noisy frame (sampled)
            .writeImage(3, &tempTemporalOutputImageInfo) // Temporal output (storage)
            .updateSet(m_temporalFilterDescriptorSet);

        m_temporalFilterPipeline->bind(commandBuffer);
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            m_temporalFilterPipelineLayout,
            0, 1, &m_temporalFilterDescriptorSet,
            0, nullptr
        );

        DenoiserPushConstantData temporalPush{};
        temporalPush.frameCount = m_frameCount;
        temporalPush.temporalAlpha = 0.05f; // Example alpha for blending, tune this
        // You might add a motion detection threshold here too
        vkCmdPushConstants(
            commandBuffer,
            m_temporalFilterPipelineLayout,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(DenoiserPushConstantData), &temporalPush
        );

        vkCmdDispatch(commandBuffer, workGroupCountX, workGroupCountY, 1);

        // --- Pass 3: Spatial Filter (e.g., Bilateral or Low-Pass) ---
        // Inputs: m_tempTemporalOutputImage (from Pass 2), newFrameImageInfo (raw for guidance)
        // Output: m_temporalHistoryImage (final denoised output for current frame, becomes history for next)

        m_tempTemporalOutputImage->transitionImageLayout(
            commandBuffer,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // Ready for sampling in spatial filter
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
        );

        m_temporalHistoryImage->transitionImageLayout(
            commandBuffer,
            VK_IMAGE_LAYOUT_GENERAL, // General layout for storage
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
        );

        tempTemporalOutputImageInfo = m_tempTemporalOutputImage->getImageInfo(); // Sampled image info
        temporalHistoryImageInfo = m_temporalHistoryImage->getImageInfo(false); 

        DescriptorWriter(m_context, *m_spatialFilterDescriptorSetLayout)
            .writeImage(0, &tempTemporalOutputImageInfo) // Temporal output (sampled)
            .writeImage(1, &newFrameImageInfo) // New noisy frame (sampled, for bilateral guidance)
            .writeImage(2, &temporalHistoryImageInfo) // History buffer (storage, final output)
            .updateSet(m_spatialFilterDescriptorSet);

        m_spatialFilterPipeline->bind(commandBuffer);
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            m_spatialFilterPipelineLayout,
            0, 1, &m_spatialFilterDescriptorSet,
            0, nullptr
        );

        DenoiserPushConstantData spatialPush{};
        spatialPush.spatialSigmaColor = 0.1f; // Example sigma values, tune
        spatialPush.spatialSigmaSpace = 2.0f;
        vkCmdPushConstants(
            commandBuffer,
            m_spatialFilterPipelineLayout,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(DenoiserPushConstantData), &spatialPush
        );

        vkCmdDispatch(commandBuffer, workGroupCountX, workGroupCountY, 1);

		// Copy the denoised output to the scene image
		copyDenoisedIntoSceneImage(commandBuffer, sceneImage);
    }

    void DenoiserRenderSystem::copyDenoisedIntoSceneImage(VkCommandBuffer commandBuffer, Shared<VulkanImage> sceneImage) {
		// Transition the denoised image to transfer source layout
		m_temporalHistoryImage->transitionImageLayout(
			commandBuffer,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT
		);
		// Transition the scene image to transfer destination layout
		sceneImage->transitionImageLayout(
			commandBuffer,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT
		);

		// Copy the denoised image to the scene image
		VkImageCopy copyRegion{};
		copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.srcSubresource.mipLevel = 0;
		copyRegion.srcSubresource.baseArrayLayer = 0;
		copyRegion.srcSubresource.layerCount = 1;
		copyRegion.srcOffset = { 0, 0, 0 };
		copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.dstSubresource.mipLevel = 0;
		copyRegion.dstSubresource.baseArrayLayer = 0;
		copyRegion.dstSubresource.layerCount = 1;
		copyRegion.dstOffset = { 0, 0, 0 };
		copyRegion.extent.width = m_extent.width;
		copyRegion.extent.height = m_extent.height;
		copyRegion.extent.depth = 1;
		vkCmdCopyImage(
			commandBuffer,
			m_temporalHistoryImage->getVkImage(),
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			sceneImage->getVkImage(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &copyRegion
		);
    }

    void DenoiserRenderSystem::resetAccumulation() {
        m_frameCount = 0;
        // To truly clear the accumulation buffer, you'd need to dispatch a compute shader
        // that writes zeros to it, or use a vkCmdClearColorImage.
        // For simplicity, resetting m_frameCount often suffices as the accumulation shader
        // can be designed to reset accumulation when frameCount is 1.
    }

    void DenoiserRenderSystem::updateImages(VkExtent2D swapChainExtent) {
        m_extent = swapChainExtent;
        createImages(swapChainExtent);
    }

    void DenoiserRenderSystem::reloadShaders() {
		createAccumulationPipeline(false);
		createTemporalFilterPipeline(false);
		createSpatialFilterPipeline(false);
	}
} 