#include "graphics/render_systems/object_picking_system.hpp"
#include "core/obj_picking_id.hpp"
#include "graphics/resources/vk_mesh.hpp"
#include "scene/ecs/component.hpp"

namespace pxt {
    struct ObjPickingPushConstantData {
        glm::mat4 modelMatrix{1.f};
        glm::vec4 objPickingColor{1.f};
    };

    struct SinglePixelRgbaBuffer {
        glm::u8vec4 pixelColor;
    };

    ObjectPickingSystem::ObjectPickingSystem(Context& context, DescriptorAllocatorGrowable& descriptorAllocator,
                                             DescriptorSetLayout& globalSetLayout, VkExtent2D sceneImageExtent)
        : m_context{context}, m_descriptorAllocator{descriptorAllocator}, m_sceneExtent{sceneImageExtent} {
        createRenderPass();
        createSceneColorIdsImage();
        createOffscreenDepthResources();
        createOffscreenFrameBuffer();

        createPixelColorBuffer();

        createPipelineLayout(globalSetLayout);
        createPipeline();
    }

    ObjectPickingSystem::~ObjectPickingSystem() {
        vkDestroyPipelineLayout(m_context.getDevice(), m_pipelineLayout, nullptr);
    }

    void ObjectPickingSystem::createRenderPass() {
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = m_context.findDepthFormat();
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = m_offscreenColorFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 0;
        renderPassInfo.pDependencies = VK_NULL_HANDLE;

        m_offscreenRenderPass = createUnique<RenderPass>(m_context, renderPassInfo, colorAttachment, depthAttachment,
                                                         "ObjectPickingSystem Offscreen Render Pass");
    }

    void ObjectPickingSystem::createSceneColorIdsImage() {
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
                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT |     // to transfer the pixel data to a buffer
                               VK_IMAGE_USAGE_SAMPLED_BIT;           // to be readable in a shader (composition pass)
        sceneImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        sceneImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        m_sceneWithColorIds = createShared<VulkanImage>(m_context, sceneImageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        // in case we need to read from it before ever selecting an object
        m_sceneWithColorIds->transitionImageLayoutSingleTimeCmd(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                                                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

        VkImageViewCreateInfo colorViewInfo{};
        colorViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        colorViewInfo.image = m_sceneWithColorIds->getVkImage();
        colorViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorViewInfo.format = m_offscreenColorFormat;
        colorViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorViewInfo.subresourceRange.baseMipLevel = 0;
        colorViewInfo.subresourceRange.levelCount = 1;
        colorViewInfo.subresourceRange.baseArrayLayer = 0;
        colorViewInfo.subresourceRange.layerCount = 1;

        m_sceneWithColorIds->createImageView(colorViewInfo);
    }

    void ObjectPickingSystem::createOffscreenDepthResources() {
        // DEPTH RESOURCE
        VkFormat depthFormat = m_context.findDepthFormat();

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_sceneExtent.width;
        imageInfo.extent.height = m_sceneExtent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = depthFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        m_offscreenDepthImage = createShared<VulkanImage>(m_context, imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_offscreenDepthImage->getVkImage();
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = depthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        m_offscreenDepthImage->createImageView(viewInfo);
    }

    void ObjectPickingSystem::createOffscreenFrameBuffer() {
        // Create the offscreen framebuffer
        std::array<VkImageView, 2> attachments = {m_sceneWithColorIds->getImageView(),
                                                  m_offscreenDepthImage->getImageView()};

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_offscreenRenderPass->getHandle();
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_sceneExtent.width;
        framebufferInfo.height = m_sceneExtent.height;
        framebufferInfo.layers = 1;

        m_offscreenFb =
            createUnique<FrameBuffer>(m_context, framebufferInfo, "ObjectPickingSystem Offscreen Framebuffer",
                                      m_sceneWithColorIds, m_offscreenDepthImage);
    }

    void ObjectPickingSystem::createPixelColorBuffer() {
        SinglePixelRgbaBuffer singlePixelRgbaBufferData{};
        singlePixelRgbaBufferData.pixelColor = {0, 0, 0, 0};

        m_selectedPixelColorBuffer = createUnique<VulkanBuffer>(
            m_context, sizeof(SinglePixelRgbaBuffer), 1,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT // we need to see it from the cpu
        );

        m_selectedPixelColorBuffer->map();
        m_selectedPixelColorBuffer->writeToBuffer(&singlePixelRgbaBufferData);
        m_selectedPixelColorBuffer->unmap();
    }

    void ObjectPickingSystem::createPipelineLayout(DescriptorSetLayout& globalSetLayout) {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(ObjPickingPushConstantData);

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

    void ObjectPickingSystem::createPipeline(bool useCompiledSpirvFiles) {
        PXT_ASSERT(m_pipelineLayout != nullptr, "Cannot create pipeline before pipelineLayout");

        RasterizationPipelineConfigInfo pipelineConfig{};
        Pipeline::defaultPipelineConfigInfo(pipelineConfig);
        pipelineConfig.renderPass = m_offscreenRenderPass->getHandle();
        pipelineConfig.pipelineLayout = m_pipelineLayout;
        // we need only position
        pipelineConfig.attributeDescriptions = VulkanMesh::getVertexAttributeDescriptionOnlyPositon();

        const std::string baseShaderPath = useCompiledSpirvFiles ? SPV_SHADERS_PATH : SHADERS_PATH;
        const std::string filenameSuffix = useCompiledSpirvFiles ? ".spv" : "";

        std::vector<std::string> shaderFilePaths;
        for (const auto& filePath : m_shaderFilePaths) {
            shaderFilePaths.push_back(baseShaderPath + filePath + filenameSuffix);
        };

        m_pipeline = createUnique<Pipeline>(m_context, shaderFilePaths, pipelineConfig);
    }

    void ObjectPickingSystem::render(FrameInfo& frameInfo, Renderer& renderer, u32vec2 mousePixelCoords,
                                     bool isObjectPickingRequested) {
        // black is the "no object" color
        VkClearColorValue blackClearColor = {0.f, 0.f, 0.f, 1.f};
        renderer.beginRenderPass(frameInfo.commandBuffer, *m_offscreenRenderPass, *m_offscreenFb, m_sceneExtent,
                                 blackClearColor);

        m_pipeline->bind(frameInfo.commandBuffer);

        std::array<VkDescriptorSet, 1> descriptorSets = {frameInfo.globalDescriptorSet};

        vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0,
                                static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);

        auto view = frameInfo.scene.getEntitiesWith<TransformComponent, MeshComponent, ObjPickingIdComponent>();
        for (auto entity : view) {

            const auto& [transform, meshComponent, objPickingIdComponent] =
                view.get<TransformComponent, MeshComponent, ObjPickingIdComponent>(entity);

            auto vulkanMesh = std::static_pointer_cast<VulkanMesh>(meshComponent.mesh);

            ObjPickingPushConstantData push{};
            push.modelMatrix = transform.mat4();
            push.objPickingColor = objPickingIdComponent.getColorAsVec4();

            vkCmdPushConstants(frameInfo.commandBuffer, m_pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(ObjPickingPushConstantData), &push);

            vulkanMesh->bind(frameInfo.commandBuffer);
            vulkanMesh->draw(frameInfo.commandBuffer);
        }

        renderer.endRenderPass(frameInfo.commandBuffer, *m_offscreenRenderPass, *m_offscreenFb);

        if (isObjectPickingRequested) {
            // if a click was registered, copy the pixel under the mouse cursor to the buffer
            // transition image to transfer source layout
            m_sceneWithColorIds->transitionImageLayout(frameInfo.commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                       VK_PIPELINE_STAGE_TRANSFER_BIT);

            // define the region to copy (1x1 pixel at mouse coords)
            VkBufferImageCopy region = {};
            region.bufferOffset = 0;
            region.bufferRowLength = 0;   // tightly packed
            region.bufferImageHeight = 0; // tightly packed
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = {static_cast<int32_t>(mousePixelCoords.x), static_cast<int32_t>(mousePixelCoords.y),
                                  0}; // pixel coordinates
            region.imageExtent = {1, 1, 1};

            vkCmdCopyImageToBuffer(frameInfo.commandBuffer, m_sceneWithColorIds->getVkImage(),
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_selectedPixelColorBuffer->getBuffer(), 1,
                                   &region);

            // then transition to shader read only optimal for composition pass use
            m_sceneWithColorIds->transitionImageLayout(
                frameInfo.commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        } else {
            // just transition back to shader read only optimal otherwise
            m_sceneWithColorIds->transitionImageLayout(
                frameInfo.commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        }
    }

    void ObjectPickingSystem::updateImage(VkExtent2D newExtent) {
        m_sceneExtent = newExtent;

        createSceneColorIdsImage();
        createOffscreenDepthResources();
        createOffscreenFrameBuffer();
    }

    void ObjectPickingSystem::reloadShaders() {
        PXT_INFO("Reloading shaders...");
        createPipeline(false);
    }

    uint32_t ObjectPickingSystem::postFrameUpdate(VkFence frameFence) {
        // synchronize with the frame fence to ensure the GPU has finished
        vkWaitForFences(m_context.getDevice(), 1, &frameFence, VK_TRUE, UINT64_MAX);

        // read the selected pixel color from the buffer
        m_selectedPixelColorBuffer->map();
        glm::u8vec4 color;
        std::memcpy(&color, m_selectedPixelColorBuffer->getMappedMemory(), sizeof(glm::u8vec4));
        m_selectedPixelColorBuffer->unmap();

        uint32_t pickedObjectId = core::ObjPickingId::getIdFromColor(glm::u8vec3(color.r, color.g, color.b));

        return pickedObjectId;
    }
} // namespace pxt