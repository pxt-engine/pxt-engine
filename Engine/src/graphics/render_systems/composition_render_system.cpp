#include "graphics/render_systems/composition_render_system.hpp"

namespace pxt {

    struct CompositionPushConstants {
        glm::vec4 selectedObjectColor;
        glm::vec4 outlineColor;
        uint32_t outlineThickness;
    };

    CompositionRenderSystem::CompositionRenderSystem(Context& context, DescriptorManager& descriptorManager)
        : m_context(context), m_descriptorManager(descriptorManager) {

        m_nearestSampler = VulkanSampler::createSimpleNearestSampler(m_context);

        createDescriptorSet();
        createPipelineLayout();
        createPipeline();
    }

    CompositionRenderSystem::~CompositionRenderSystem() {
        vkDestroyPipelineLayout(m_context.getDevice(), m_pipelineLayout, nullptr);
    }

    void CompositionRenderSystem::createDescriptorSet() {
        std::vector<DescriptorEntry> bindings = {
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 1},
            {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 1},
            {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 1},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1}
        };
        
        m_descriptorSet = m_descriptorManager.createSet(bindings);
    }

    void CompositionRenderSystem::createPipelineLayout() {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(CompositionPushConstants);

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{m_descriptorManager.getRawLayout(m_descriptorSet)};

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

    void CompositionRenderSystem::render(FrameInfo& frameInfo, VulkanImage& sceneColor, VulkanImage& selectionMask, VulkanImage& objIdsImage,
                                         VulkanImage& outputImage, uint32_t selectedObjPickingId) {

        outputImage.transitionImageLayout(frameInfo.commandBuffer, VK_IMAGE_LAYOUT_GENERAL,
                                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        VkDescriptorImageInfo sceneInfo = sceneColor.getImageInfo(false);
        VkDescriptorImageInfo selectionMaskInfo = selectionMask.getImageInfo(false);
        VkDescriptorImageInfo objIdsInfo = objIdsImage.getImageInfo(false);
        VkDescriptorImageInfo outputInfo = outputImage.getImageInfo(false);

        sceneInfo.sampler = m_nearestSampler->getHandle();
        selectionMaskInfo.sampler = m_nearestSampler->getHandle();
        objIdsInfo.sampler = m_nearestSampler->getHandle();

        m_descriptorManager.submitUpdateSingle(m_descriptorSet, 0, sceneInfo);
        m_descriptorManager.submitUpdateSingle(m_descriptorSet, 1, selectionMaskInfo);
        m_descriptorManager.submitUpdateSingle(m_descriptorSet, 2, objIdsInfo);
        m_descriptorManager.submitUpdateSingle(m_descriptorSet, 3, outputInfo);
        
        m_descriptorManager.flushUpdatesForSet(m_descriptorSet, frameInfo.frameIndex);

        CompositionPushConstants compPushConstants{};
        compPushConstants.selectedObjectColor = core::ObjPickingId::getColorVec4FromId(selectedObjPickingId);
        compPushConstants.outlineThickness = 2;
        compPushConstants.outlineColor = {1.0f, 0.5f, 0.0f, 1.0f};

        m_pipeline->bind(frameInfo.commandBuffer);
        vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1,
                                m_descriptorManager.getDescriptorSetPtr(m_descriptorSet, frameInfo.frameIndex), 0,
                                nullptr);

        vkCmdPushConstants(frameInfo.commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(compPushConstants), &compPushConstants);

        const uint32_t gx = (sceneColor.getExtent().width + 15) / 16;
        const uint32_t gy = (sceneColor.getExtent().height + 15) / 16;

        vkCmdDispatch(frameInfo.commandBuffer, gx, gy, 1);

        outputImage.transitionImageLayout(frameInfo.commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }

    void CompositionRenderSystem::reloadShaders() {
        PXT_INFO("Reloading shaders...");
        createPipeline(false);
    }
} // namespace pxt
