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

        createBuffers(swapChainExtent);

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

    void DenoiserRenderSystem::createBuffers(VkExtent2D extent) {
        // Create an accumulation buffer
        // This buffer accumulates raw path-traced samples.
        m_accumulationImage = createUnique<Image>(
            m_context,
            extent,
            VK_FORMAT_R16G16B16A16_SFLOAT, // High precision format for accumulation
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        // Create a history buffer for temporal filtering.
        // This buffer stores the final denoised output of the PREVIOUS frame,
        // and will store the final denoised output of the CURRENT frame.
        m_temporalHistoryImage = createUnique<Image>(
            m_context,
            extent,
            VK_FORMAT_R16G16B16A16_SFLOAT, // High precision for history
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        // Create a temporary buffer for the output of the temporal filter.
        // This serves as input for the spatial filter.
        m_tempTemporalOutputImage = createUnique<Image>(
            m_context,
            extent,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
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

        const std::string baseShaderPath = useCompiledSpirvFiles ? SPV_SHADERS_PATH : SHADERS_PATH;
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

        const std::string baseShaderPath = useCompiledSpirvFiles ? SPV_SHADERS_PATH : SHADERS_PATH;
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

        const std::string baseShaderPath = useCompiledSpirvFiles ? SPV_SHADERS_PATH : SHADERS_PATH;
        const std::string filenameSuffix = useCompiledSpirvFiles ? ".spv" : "";

        std::string shaderFilePath = baseShaderPath + m_spatialShaderPath + filenameSuffix;

        m_spatialFilterPipeline = createUnique<Pipeline>(
            m_context,
            shaderFilePath,
            pipelineConfig
        );
    }


    void DenoiserRenderSystem::denoise(FrameInfo& frameInfo, VkDescriptorImageInfo newFrameImageInfo) {
        m_frameCount++;
        VkCommandBuffer commandBuffer = frameInfo.commandBuffer;

        // Calculate work group dimensions
		const uint32_t workGroupSize = 16;
        const uint32_t workGroupCountX = (m_extent.width + workGroupSize - 1) / workGroupSize;
        const uint32_t workGroupCountY = (m_extent.height + workGroupSize - 1) / workGroupSize;

        // --- Pass 1: Accumulation ---
        // Inputs: newFrameImageInfo (noisy path-traced frame)
        // Output: m_accumulationBuffer (accumulated samples)
        // m_accumulationBuffer is used as both read and write storage image,
        // so its layout should be VK_IMAGE_LAYOUT_GENERAL.

        m_accumulationImage->transitionImageLayout(
            commandBuffer,
            VK_IMAGE_LAYOUT_GENERAL, // Transition to general layout for storage
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
        );

        DescriptorWriter(m_context, *m_accumulationDescriptorSetLayout)
            .writeImage(0, &newFrameImageInfo) // New noisy frame (sampled)
            .writeImage(1, m_accumulationImage->getImageInfo()) // Accumulation buffer (storage)
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

        // Barrier: Ensure accumulation write is complete before temporal filter reads from it
        VkImageMemoryBarrier accumulationBarrier{};
        accumulationBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        accumulationBarrier.image = m_accumulationImage->getImage();
        accumulationBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        accumulationBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        accumulationBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        accumulationBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        accumulationBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        accumulationBarrier.subresourceRange.baseArrayLayer = 0;
        accumulationBarrier.subresourceRange.layerCount = 1;
        accumulationBarrier.subresourceRange.baseMipLevel = 0;
        accumulationBarrier.subresourceRange.levelCount = 1;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &accumulationBarrier
        );

        m_accumulationImage->transitionImageLayout(
            commandBuffer,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		);
        
        m_temporalHistoryImage->transitionImageLayout(
            commandBuffer,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		);

        m_tempTemporalOutputImage->transitionImageLayout(
            commandBuffer,
            VK_IMAGE_LAYOUT_GENERAL, // General layout for storage
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		);

        // --- Pass 2: Temporal Filter ---
        // Inputs: m_accumulationBuffer (from Pass 1), m_historyBuffer (previous frame's final output), newFrameImageInfo (raw)
        // Output: m_tempTemporalOutputImage
        DescriptorWriter(m_context, *m_temporalFilterDescriptorSetLayout)
            .writeImage(0, m_accumulationImage->getImageInfo()) // Accumulation (sampled)
            .writeImage(1, m_temporalHistoryImage->getImageInfo()) // History (sampled)
            .writeImage(2, &newFrameImageInfo) // New noisy frame (sampled)
            .writeImage(3, m_tempTemporalOutputImage->getImageInfo()) // Temporal output (storage)
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

        // Barrier: Ensure temporal filter write is complete before spatial filter reads from it
        VkImageMemoryBarrier temporalBarrier{};
        temporalBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        temporalBarrier.image = m_tempTemporalOutputImage->getImage();
        temporalBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        temporalBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        temporalBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        temporalBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        temporalBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        temporalBarrier.subresourceRange.baseArrayLayer = 0;
        temporalBarrier.subresourceRange.layerCount = 1;
        temporalBarrier.subresourceRange.baseMipLevel = 0;
        temporalBarrier.subresourceRange.levelCount = 1;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &temporalBarrier
        );

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

        // --- Pass 3: Spatial Filter (e.g., Bilateral or Low-Pass) ---
        // Inputs: m_tempTemporalOutputImage (from Pass 2), newFrameImageInfo (raw for guidance)
        // Output: m_historyBuffer (final denoised output for current frame, becomes history for next)
        DescriptorWriter(m_context, *m_spatialFilterDescriptorSetLayout)
            .writeImage(0, m_tempTemporalOutputImage->getImageInfo()) // Temporal output (sampled)
            .writeImage(1, &newFrameImageInfo) // New noisy frame (sampled, for bilateral guidance)
            .writeImage(2, m_temporalHistoryImage->getImageInfo()) // History buffer (storage, final output)
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
        spatialPush.spatialSigmaColor = 0.1f; // Example sigma values, tune these
        spatialPush.spatialSigmaSpace = 2.0f;
        vkCmdPushConstants(
            commandBuffer,
            m_spatialFilterPipelineLayout,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(DenoiserPushConstantData), &spatialPush
        );

        vkCmdDispatch(commandBuffer, workGroupCountX, workGroupCountY, 1);

        // Final Barrier: Ensure spatial filter write is complete if the output is used elsewhere immediately
        // (e.g., for presentation or another pass).
        VkImageMemoryBarrier finalBarrier{};
        finalBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        finalBarrier.image = m_temporalHistoryImage->getImage(); // The final denoised image
        finalBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        finalBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT; // Or VK_ACCESS_TRANSFER_READ_BIT for blitting
        finalBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        finalBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // Ready for sampling next frame or presentation
        finalBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        finalBarrier.subresourceRange.baseArrayLayer = 0;
        finalBarrier.subresourceRange.layerCount = 1;
        finalBarrier.subresourceRange.baseMipLevel = 0;
        finalBarrier.subresourceRange.levelCount = 1;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // Or whatever stage consumes it
            0, 0, nullptr, 0, nullptr, 1, &finalBarrier
        );
    }

    void DenoiserRenderSystem::resetAccumulation() {
        m_frameCount = 0;
        // To truly clear the accumulation buffer, you'd need to dispatch a compute shader
        // that writes zeros to it, or use a vkCmdClearColorImage.
        // For simplicity, resetting m_frameCount often suffices as the accumulation shader
        // can be designed to reset accumulation when frameCount is 1.
    }

} 