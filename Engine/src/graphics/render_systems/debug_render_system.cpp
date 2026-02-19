#include "graphics/render_systems/debug_render_system.hpp"

#include "graphics/resources/vk_mesh.hpp"
#include "resources/resource_manager.hpp"
#include "scene/ecs/entity.hpp"

namespace pxt {

    struct DebugPushConstantData {
        glm::mat4 modelMatrix{1.f};
        glm::mat4 normalMatrix{1.f};
        glm::vec4 color{1.f};
        glm::vec4 objPickingColor{1.f};
        uint32_t enableObjectPicking{0};
        uint32_t enableWireframe{0};
        uint32_t enableNormals{0};
        int textureIndex = 0;
        int normalMapIndex = 1;
        int ambientOcclusionMapIndex = 0;
        float tilingFactor = 1.0f;
        float blinnPhongSpecularIntensity = 0.0f;
        float blinnPhongSpecularShininess = 1.0f;
    };

    DebugRenderSystem::DebugRenderSystem(Context& context, DescriptorAllocatorGrowable& descriptorAllocator,
                                         TextureRegistry& textureRegistry, VkRenderPass renderPass,
                                         DescriptorSetLayout& globalSetLayout)
        : m_context(context), m_descriptorAllocator(descriptorAllocator), m_textureRegistry(textureRegistry),
          m_renderPassHandle(renderPass) {
        createPipelineLayout(globalSetLayout);
        createPipelines();
    }

    DebugRenderSystem::~DebugRenderSystem() {
        vkDestroyPipelineLayout(m_context.getDevice(), m_pipelineLayout, nullptr);
    }

    void DebugRenderSystem::createPipelineLayout(DescriptorSetLayout& globalSetLayout) {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(DebugPushConstantData);

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout.getHandle(),
                                                                m_textureRegistry.getDescriptorSetLayout()};

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

    void DebugRenderSystem::createPipelines(bool useCompiledSpirvFiles) {
        PXT_ASSERT(m_pipelineLayout != nullptr, "Cannot create pipeline before pipelineLayout");

        // Default Solid Pipeline
        RasterizationPipelineConfigInfo pipelineConfig{};
        Pipeline::defaultPipelineConfigInfo(pipelineConfig);
        pipelineConfig.renderPass = m_renderPassHandle;
        ;
        pipelineConfig.pipelineLayout = m_pipelineLayout;

        const std::string baseShaderPath = useCompiledSpirvFiles ? SPV_SHADERS_PATH : SHADERS_PATH;
        const std::string filenameSuffix = useCompiledSpirvFiles ? ".spv" : "";

        std::vector<std::string> shaderFilePaths;
        for (const auto& filePath : m_shaderFilePaths) {
            shaderFilePaths.push_back(baseShaderPath + filePath + filenameSuffix);
        };

        m_pipelineSolid = createUnique<Pipeline>(m_context, shaderFilePaths, pipelineConfig);

        // Wireframe Pipeline
        pipelineConfig.rasterizationInfo.polygonMode = VK_POLYGON_MODE_LINE;

        m_pipelineWireframe = createUnique<Pipeline>(m_context, shaderFilePaths, pipelineConfig);
    }

    // TODO: change how we do this
    template <typename... Components>
    void DebugRenderSystem::processEntities(ComponentList<Components...> neededComponents, FrameInfo& frameInfo) {
        auto view = frameInfo.scene.getEntitiesWith(neededComponents);

        for (auto entity : view) {
            const auto& [transform, meshComponent, materialComponent, objPickingIdComponent] =
                view.get<TransformComponent, MeshComponent, MaterialComponent, ObjPickingIdComponent>(entity);

            auto material = frameInfo.rm.get<Material>(materialComponent.material);
            auto vulkanMesh = frameInfo.rm.get<VulkanMesh>(meshComponent.mesh);

            DebugPushConstantData push{};
            push.modelMatrix = transform.mat4();
            push.normalMatrix = transform.normalMatrix(push.modelMatrix);
            push.color = material->getAlbedoColor() * glm::vec4(materialComponent.tint, 1.0f);
            push.objPickingColor = objPickingIdComponent.getColorAsVec4();
            push.textureIndex = m_isAlbedoMapEnabled ? m_textureRegistry.getIndex(material->getAlbedoMap()->id) : -1;
            push.normalMapIndex = m_isNormalMapEnabled ? m_textureRegistry.getIndex(material->getNormalMap()->id) : -1;
            push.ambientOcclusionMapIndex =
                m_isAOMapEnabled ? m_textureRegistry.getIndex(material->getAmbientOcclusionMap()->id) : -1;
            push.tilingFactor = materialComponent.tilingFactor;
            push.blinnPhongSpecularIntensity = material->getBlinnPhongSpecularIntensity();
            push.blinnPhongSpecularShininess = material->getBlinnPhongSpecularShininess();

            push.enableObjectPicking = (uint32_t)(m_renderMode == ObjectPickingID);
            push.enableWireframe = (uint32_t)(m_renderMode == Wireframe);
            push.enableNormals = (uint32_t)m_isNormalColorEnabled;

            vkCmdPushConstants(frameInfo.commandBuffer, m_pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(DebugPushConstantData), &push);

            vulkanMesh->bind(frameInfo.commandBuffer);
            vulkanMesh->draw(frameInfo.commandBuffer);
        }
    }

    void DebugRenderSystem::render(FrameInfo& frameInfo) {
        if (m_renderMode == Wireframe) {
            m_pipelineWireframe->bind(frameInfo.commandBuffer);
        } else {
            m_pipelineSolid->bind(frameInfo.commandBuffer);
        }

        std::array<VkDescriptorSet, 2> descriptorSets = {frameInfo.globalDescriptorSet,
                                                         m_textureRegistry.getDescriptorSet()};

        vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0,
                                static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);

        if (frameInfo.engineMode == core::EngineMode::EDIT) {
            ComponentList<TransformComponent, MeshComponent, MaterialComponent, ObjPickingIdComponent, VisibilityTag>
                componentList;
            processEntities(componentList, frameInfo);

        } else {
            ComponentList<TransformComponent, MeshComponent, MaterialComponent, ObjPickingIdComponent, RenderableTag>
                componentList;
            processEntities(componentList, frameInfo);
        }
    }

    void DebugRenderSystem::updateUi() {
        ImGui::Text("Render Mode:");
        ImGui::RadioButton("Wireframe", &m_renderMode, Wireframe);
        ImGui::RadioButton("Fill", &m_renderMode, Fill);
        ImGui::RadioButton("Object Picking ID", &m_renderMode, ObjectPickingID);
        ImGui::BeginDisabled(m_renderMode != Fill);
        ImGui::Checkbox("Show Normals as Color", &m_isNormalColorEnabled);
        ImGui::Checkbox("Show Albedo Map", &m_isAlbedoMapEnabled);
        ImGui::Checkbox("Show Normal Map", &m_isNormalMapEnabled);
        ImGui::Checkbox("Show Ambient Occlusion Map", &m_isAOMapEnabled);
        ImGui::EndDisabled();
    }

    void DebugRenderSystem::reloadShaders() {
        PXT_INFO("Reloading shaders...");
        createPipelines(false);
    }

    // this is needed to explicitly instantiate the template functions
    template void DebugRenderSystem::processEntities(
        ComponentList<TransformComponent, MeshComponent, MaterialComponent, ObjPickingIdComponent, VisibilityTag>,
        FrameInfo&);

    template void DebugRenderSystem::processEntities(
        ComponentList<TransformComponent, MeshComponent, MaterialComponent, ObjPickingIdComponent, RenderableTag>,
        FrameInfo&);
} // namespace pxt