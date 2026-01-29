#include "graphics/render_systems/selection_mask_render_system.hpp"
#include "graphics/resources/vk_mesh.hpp"
#include "resources/resource_manager.hpp"
#include "scene/ecs/component.hpp"
#include "scene/ecs/entity.hpp"

namespace pxt {
    struct SelectionMaskPush {
        glm::mat4 modelMatrix{1.f};
    };

    SelectionMaskRenderSystem::SelectionMaskRenderSystem(Context& context,
                                                         DescriptorAllocatorGrowable& descriptorAllocator,
                                                         DescriptorSetLayout& globalSetLayout,
                                                         VkExtent2D sceneImageExtent)
        : m_context{context}, m_descriptorAllocator{descriptorAllocator}, m_sceneExtent{sceneImageExtent} {
        createRenderPass();
        createMaskColorImage();
        createOffscreenFrameBuffer();

        createPipelineLayout(globalSetLayout);
        createPipeline();
    }

    SelectionMaskRenderSystem::~SelectionMaskRenderSystem() {
        vkDestroyPipelineLayout(m_context.getDevice(), m_pipelineLayout, nullptr);
    }

    void SelectionMaskRenderSystem::createRenderPass() {
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = m_offscreenColorFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = VK_NULL_HANDLE;

        std::array<VkAttachmentDescription, 1> attachments = {colorAttachment};

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 0;
        renderPassInfo.pDependencies = VK_NULL_HANDLE;

        m_offscreenRenderPass = createUnique<RenderPass>(m_context, renderPassInfo, colorAttachment,
                                                         "SelectionMaskRenderSystem Offscreen Render Pass");
    }

    void SelectionMaskRenderSystem::createMaskColorImage() {
        VkImageCreateInfo sceneImageInfo{};
        sceneImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        sceneImageInfo.imageType = VK_IMAGE_TYPE_2D;
        sceneImageInfo.extent.width = m_sceneExtent.width;
        sceneImageInfo.extent.height = m_sceneExtent.height;
        sceneImageInfo.extent.depth = 1;
        sceneImageInfo.mipLevels = 1;
        sceneImageInfo.arrayLayers = 1;
        sceneImageInfo.format = m_offscreenColorFormat;
        sceneImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        sceneImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        sceneImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | // to be writable in a renderpass
                               VK_IMAGE_USAGE_SAMPLED_BIT;           // to be readable in a shader (composition pass)
        sceneImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        sceneImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        m_selectionMaskImage =
            createShared<VulkanImage>(m_context, sceneImageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkImageViewCreateInfo colorViewInfo{};
        colorViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        colorViewInfo.image = m_selectionMaskImage->getVkImage();
        colorViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorViewInfo.format = m_offscreenColorFormat;
        colorViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorViewInfo.subresourceRange.baseMipLevel = 0;
        colorViewInfo.subresourceRange.levelCount = 1;
        colorViewInfo.subresourceRange.baseArrayLayer = 0;
        colorViewInfo.subresourceRange.layerCount = 1;

        m_selectionMaskImage->createImageView(colorViewInfo);
    }

    void SelectionMaskRenderSystem::createOffscreenFrameBuffer() {
        // Create the offscreen framebuffer
        std::array<VkImageView, 1> attachments = {m_selectionMaskImage->getImageView()};

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_offscreenRenderPass->getHandle();
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_sceneExtent.width;
        framebufferInfo.height = m_sceneExtent.height;
        framebufferInfo.layers = 1;

        m_offscreenFb = createUnique<FrameBuffer>(
            m_context, framebufferInfo, "SelectionMaskRenderSystem Offscreen Framebuffer", m_selectionMaskImage);
    }

    void SelectionMaskRenderSystem::createPipelineLayout(DescriptorSetLayout& globalSetLayout) {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(SelectionMaskPush);

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{
            globalSetLayout.getDescriptorSetLayout(),
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

    void SelectionMaskRenderSystem::createPipeline(bool useCompiledSpirvFiles) {
        PXT_ASSERT(m_pipelineLayout != nullptr, "Cannot create pipeline before pipelineLayout");

        RasterizationPipelineConfigInfo pipelineConfig{};
        Pipeline::defaultPipelineConfigInfo(pipelineConfig);
        pipelineConfig.renderPass = m_offscreenRenderPass->getHandle();
        pipelineConfig.pipelineLayout = m_pipelineLayout;
        // we need only position
        pipelineConfig.attributeDescriptions = VulkanMesh::getVertexAttributeDescriptionOnlyPositon();

        Pipeline::disableDepthTest(pipelineConfig);

        const std::string baseShaderPath = useCompiledSpirvFiles ? SPV_SHADERS_PATH : SHADERS_PATH;
        const std::string filenameSuffix = useCompiledSpirvFiles ? ".spv" : "";

        std::vector<std::string> shaderFilePaths;
        for (const auto& filePath : m_shaderFilePaths) {
            shaderFilePaths.push_back(baseShaderPath + filePath + filenameSuffix);
        };

        m_pipeline = createUnique<Pipeline>(m_context, shaderFilePaths, pipelineConfig);
    }

    void SelectionMaskRenderSystem::render(FrameInfo& frameInfo, Renderer& renderer, core::UID selectedEntityUID) {
        // black is the "no object" color
        VkClearColorValue blackClearColor = {0.f, 0.f, 0.f, 1.f};
        renderer.beginRenderPass(frameInfo.commandBuffer, *m_offscreenRenderPass, *m_offscreenFb, m_sceneExtent,
                                 blackClearColor);

        if (selectedEntityUID == core::UID::s_invalidId) {
            // no entity selected, leave black and do nothing
            renderer.endRenderPass(frameInfo.commandBuffer, *m_offscreenRenderPass, *m_offscreenFb);
            return;
        }

        Entity selectedEntity = frameInfo.scene.getEntity(selectedEntityUID);
        bool isEntityVisible = frameInfo.engineMode == core::EngineMode::EDIT ? selectedEntity.has<VisibilityTag>()
                                                                              : selectedEntity.has<RenderableTag>();

        if (!isEntityVisible || !selectedEntity.has<TransformComponent>() || !selectedEntity.has<MeshComponent>()) {
            // entity does not have required components, leave black and do nothing
            renderer.endRenderPass(frameInfo.commandBuffer, *m_offscreenRenderPass, *m_offscreenFb);
            return;
        }

        const auto& transform = selectedEntity.get<TransformComponent>();
        const auto& meshComponent = selectedEntity.get<MeshComponent>();

        // this is still required, because we are not excluding entities with invalid mesh handles
        // through the view lookup (see material_render_system.cpp render for an example)
        if (!meshComponent.mesh.isValid()) {
            // entity does not have required components, leave black and do nothing
            renderer.endRenderPass(frameInfo.commandBuffer, *m_offscreenRenderPass, *m_offscreenFb);
            return;
        }

        m_pipeline->bind(frameInfo.commandBuffer);

        std::array<VkDescriptorSet, 1> descriptorSets = {frameInfo.globalDescriptorSet};

        vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0,
                                static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);

        auto vulkanMesh = frameInfo.rm.get<VulkanMesh>(meshComponent.mesh);

        SelectionMaskPush push{};
        push.modelMatrix = transform.mat4();

        vkCmdPushConstants(frameInfo.commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(SelectionMaskPush), &push);

        vulkanMesh->bind(frameInfo.commandBuffer);
        vulkanMesh->draw(frameInfo.commandBuffer);

        renderer.endRenderPass(frameInfo.commandBuffer, *m_offscreenRenderPass, *m_offscreenFb);
    }

    void SelectionMaskRenderSystem::updateImage(VkExtent2D newExtent) {
        m_sceneExtent = newExtent;

        createMaskColorImage();
        createOffscreenFrameBuffer();
    }

    void SelectionMaskRenderSystem::reloadShaders() {
        PXT_INFO("Reloading shaders...");
        createPipeline(false);
    }
} // namespace pxt