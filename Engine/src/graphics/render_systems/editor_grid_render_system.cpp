#include "graphics/render_systems/editor_grid_render_system.hpp"

namespace pxt {
    struct EditorGridPushConstantData {
        glm::vec4 xAxisColor{1.f};
        glm::vec4 zAxisColor{1.f};
        float gridUnitSize = 0.5f;
        // how many grid squares run along a grid group edge
        uint32_t gridMinorsPerMajor = 1;
    };

    EditorGridRenderSystem::EditorGridRenderSystem(Context& context, DescriptorSetLayout& globalSetLayout,
                                                   VkRenderPass renderPass)
        : m_context(context), m_renderPassHandle(renderPass) {
        createPipelineLayout(globalSetLayout);
        createPipeline();
    }

    EditorGridRenderSystem::~EditorGridRenderSystem() {
        vkDestroyPipelineLayout(m_context.getDevice(), m_pipelineLayout, nullptr);
    }

    void EditorGridRenderSystem::createPipelineLayout(DescriptorSetLayout& globalSetLayout) {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(EditorGridPushConstantData);

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{
            globalSetLayout.getDescriptorSetLayout()
        };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(m_context.getDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) !=
            VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }
    }

    void EditorGridRenderSystem::createPipeline(bool useCompiledSpirvFiles) {
        PXT_ASSERT(m_pipelineLayout != nullptr, "Cannot create pipeline before pipelineLayout");

        RasterizationPipelineConfigInfo pipelineConfig{};
        Pipeline::defaultPipelineConfigInfo(pipelineConfig);
        Pipeline::enableAlphaBlending(pipelineConfig);
        pipelineConfig.renderPass = m_renderPassHandle;
        pipelineConfig.pipelineLayout = m_pipelineLayout;

        // we dont use vertex buffers
        pipelineConfig.attributeDescriptions.clear();
        pipelineConfig.bindingDescriptions.clear();

        // we can change this but it aligns with current shader logic
        pipelineConfig.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;

        // we disable depth write for the grid, we dont need it
        // it is still occluded by opaque geometry
        pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;

        const std::string baseShaderPath = useCompiledSpirvFiles ? SPV_SHADERS_PATH : SHADERS_PATH;
        const std::string filenameSuffix = useCompiledSpirvFiles ? ".spv" : "";

        std::vector<std::string> shaderFilePaths;
        for (const auto& filePath : m_shaderFilePaths) {
            shaderFilePaths.push_back(baseShaderPath + filePath + filenameSuffix);
        };

        m_pipeline = createUnique<Pipeline>(m_context, shaderFilePaths, pipelineConfig);
    }

    void EditorGridRenderSystem::render(FrameInfo& frameInfo) {
        m_pipeline->bind(frameInfo.commandBuffer);

        std::array<VkDescriptorSet, 1> descriptorSets = {
            frameInfo.globalDescriptorSet
        };

        vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0,
                                static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);

        EditorGridPushConstantData push{};
        push.xAxisColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        push.zAxisColor = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        push.gridUnitSize = 0.5f;
        push.gridMinorsPerMajor = 4;

        vkCmdPushConstants(frameInfo.commandBuffer, m_pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(EditorGridPushConstantData), &push);

        vkCmdDraw(frameInfo.commandBuffer, 4, 1, 0, 0);
    }

    void EditorGridRenderSystem::reloadShaders() {
        PXT_INFO("Reloading shaders...");
        createPipeline(false);
    }
} // namespace pxt