#include "graphics/render_systems/composition_render_system.hpp"

namespace pxt {

    struct CompositionPushConstants {
        glm::vec4 outlineColor;
        uint32_t outlineThickness;
    };

    CompositionRenderSystem::CompositionRenderSystem(Context& context, DescriptorAllocatorGrowable& descriptorAllocator)
        : m_context(context), m_descriptorAllocator(descriptorAllocator) {

        m_nearestSampler = VulkanSampler::createSimpleNearestSampler(m_context);

        createDescriptorSet();
        createPipelineLayout();
        createPipeline();
    }

    CompositionRenderSystem::~CompositionRenderSystem() {
        vkDestroyPipelineLayout(m_context.getDevice(), m_pipelineLayout, nullptr);
    }

    void CompositionRenderSystem::createDescriptorSet() {
        m_descriptorSetLayout =
            DescriptorSetLayout::Builder(m_context)
                .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT) // scene
                .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT) // ids
                .addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)          // output
                .build();

        m_descriptorAllocator.allocate(m_descriptorSetLayout->getDescriptorSetLayout(), m_descriptorSet);
    }

    void CompositionRenderSystem::createPipelineLayout() {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(CompositionPushConstants);

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{m_descriptorSetLayout->getDescriptorSetLayout()};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(m_context.getDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) !=
            VK_SUCCESS) {
            throw std::runtime_error("failed to create global majorant pipeline layout!");
        }
    }

    void CompositionRenderSystem::createPipeline(bool useCompiledSpirvFiles) {
        ComputePipelineConfigInfo config{};
        config.pipelineLayout = m_pipelineLayout;

        const std::string base = useCompiledSpirvFiles ? SPV_SHADERS_PATH : SHADERS_PATH + "post/";
        const std::string suffix = useCompiledSpirvFiles ? ".spv" : "";

        m_pipeline = createUnique<Pipeline>(m_context, base + m_shaderPath + suffix, config);
    }

    void CompositionRenderSystem::render(FrameInfo& frameInfo, VulkanImage& sceneColor, VulkanImage& selectionMask,
                                         VulkanImage& outputImage) {

        outputImage.transitionImageLayout(frameInfo.commandBuffer, VK_IMAGE_LAYOUT_GENERAL,
                                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        VkDescriptorImageInfo sceneInfo = sceneColor.getImageInfo(false);
        VkDescriptorImageInfo selectionMaskInfo = selectionMask.getImageInfo(false);
        VkDescriptorImageInfo outputInfo = outputImage.getImageInfo(false);

        sceneInfo.sampler = m_nearestSampler->getHandle();
        selectionMaskInfo.sampler = m_nearestSampler->getHandle();

        DescriptorWriter(m_context, *m_descriptorSetLayout)
            .writeImage(0, &sceneInfo)
            .writeImage(1, &selectionMaskInfo)
            .writeImage(2, &outputInfo)
            .updateSet(m_descriptorSet);

        CompositionPushConstants compPushConstants{};
        compPushConstants.outlineThickness = 2;
        compPushConstants.outlineColor = {1.0f, 0.5f, 0.0f, 1.0f};

        m_pipeline->bind(frameInfo.commandBuffer);
        vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1,
                                &m_descriptorSet, 0, nullptr);

        vkCmdPushConstants(frameInfo.commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(compPushConstants), &compPushConstants);

        const uint32_t gx = (sceneColor.getExtent().width + 7) / 8;
        const uint32_t gy = (sceneColor.getExtent().height + 7) / 8;

        vkCmdDispatch(frameInfo.commandBuffer, gx, gy, 1);

        outputImage.transitionImageLayout(frameInfo.commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }

    void CompositionRenderSystem::reloadShaders() {
        PXT_INFO("Reloading shaders...");
        createPipeline(false);
    }
} // namespace pxt
