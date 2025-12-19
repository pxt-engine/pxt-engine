#include "graphics/render_systems/render_layer.hpp"
#include "core/events/editor_events.hpp"
#include "core/events/event_dispatcher.hpp"
#include "core/events/imgui_events.hpp"
#include "ui/widgets/space.hpp"

#include "utils/vk_enum_str.h"

#include "application.hpp"

namespace pxt {
    RenderLayer::RenderLayer(Context& context, Renderer& renderer, DescriptorAllocatorGrowable& descriptorAllocator,
                             TextureRegistry& textureRegistry, MaterialRegistry& materialRegistry,
                             BLASRegistry& blasRegistry, DescriptorSetLayout& globalSetLayout,
                             Shared<Environment> environment)

        : Layer("RenderLayer"),

          m_context(context), m_renderer(renderer), m_textureRegistry(textureRegistry),
          m_materialRegistry(materialRegistry), m_blasRegistry(blasRegistry),
          m_descriptorAllocator(descriptorAllocator), m_globalSetLayout(globalSetLayout),
          m_environment(std::move(environment)) {
        m_offscreenColorFormat = m_context.findSupportedFormat(
            {VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R8G8B8A8_UNORM}, VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);

        PXT_INFO("Offscreen color format: {}", STR_VK_FORMAT(m_offscreenColorFormat));

        if (m_offscreenColorFormat == VK_FORMAT_UNDEFINED) {
            throw std::runtime_error(
                "Failed to find a suitable offscreen color format for RenderLayer's render target!");
        }

        createRenderPass();
        createSceneImage();
        createFinalImage();
        createOffscreenDepthResources();
        createOffscreenFrameBuffer();
        createRenderSystems();

        createDescriptorSetsImGui();
    }

    RenderLayer::~RenderLayer() {};

    void RenderLayer::recreateViewportResources() {
        // wait for the device to be idle
        vkDeviceWaitIdle(m_context.getDevice());

        // destroy old resources: FrameBuffer will be destroyed by reassigning the unique_ptr

        createSceneImage();
        createFinalImage();
        createOffscreenDepthResources();
        createOffscreenFrameBuffer();

        updateImguiDescriptorSet();
    }

    void RenderLayer::createRenderPass() {
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
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkSubpassDependency dependency = {};
        dependency.dstSubpass = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency.dstStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.srcAccessMask = 0;
        dependency.srcStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

        // A second dependency for the transition to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        // This ensures that when the render pass finishes, the image is ready for sampling.
        VkSubpassDependency readDependency{};
        readDependency.srcSubpass = 0;
        readDependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        readDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        readDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // Read in fragment shader
        readDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        readDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT; // Read by shader

        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
        std::array<VkSubpassDependency, 2> dependencies = {dependency, readDependency};

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();

        m_offscreenRenderPass = createUnique<RenderPass>(m_context, renderPassInfo, colorAttachment, depthAttachment,
                                                         "RenderLayer Offscreen Render Pass");
    }

    void RenderLayer::createSceneImage() {
        VkImageCreateInfo sceneImageInfo{};
        sceneImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        sceneImageInfo.imageType = VK_IMAGE_TYPE_2D;
        sceneImageInfo.extent.width = m_viewportExtent.width;
        sceneImageInfo.extent.height = m_viewportExtent.height;
        sceneImageInfo.extent.depth = 1;
        sceneImageInfo.mipLevels = 1;
        sceneImageInfo.arrayLayers = 1;
        sceneImageInfo.format = m_offscreenColorFormat;
        sceneImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        sceneImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        sceneImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | // to be writable in a renderpass
                               VK_IMAGE_USAGE_SAMPLED_BIT |          // to be readable in a shader
                               VK_IMAGE_USAGE_STORAGE_BIT |          // to be writable for raytracing shaders
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT;      // to copy to it later (denoised image)
        sceneImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        sceneImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        m_sceneImage = createShared<VulkanImage>(m_context, sceneImageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        // transition once to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL layout
        m_sceneImage->transitionImageLayoutSingleTimeCmd(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        VkImageViewCreateInfo colorViewInfo{};
        colorViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        colorViewInfo.image = m_sceneImage->getVkImage();
        colorViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorViewInfo.format = m_offscreenColorFormat;
        colorViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorViewInfo.subresourceRange.baseMipLevel = 0;
        colorViewInfo.subresourceRange.levelCount = 1;
        colorViewInfo.subresourceRange.baseArrayLayer = 0;
        colorViewInfo.subresourceRange.layerCount = 1;

        m_sceneImage->createImageView(colorViewInfo);

        // sampler for later imgui access
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        // Addressing Mode: Clamp to edge is usually best for render targets
        // This prevents "wrapping" artifacts if the sampling coordinates go slightly outside [0,1]
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = m_context.getPhysicalDeviceProperties().limits.maxSamplerAnisotropy;
        // Unnormalized coordinates: Use normalized UVs (0.0 to 1.0)
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        // Comparison: Not for texture sampling, leave disabled
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f; // Only use base mip level

        m_sceneImage->createSampler(samplerInfo);
    }

    void RenderLayer::createFinalImage() {
        VkImageCreateInfo finalImageInfo{};
        finalImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        finalImageInfo.imageType = VK_IMAGE_TYPE_2D;
        finalImageInfo.extent.width = m_viewportExtent.width;
        finalImageInfo.extent.height = m_viewportExtent.height;
        finalImageInfo.extent.depth = 1;
        finalImageInfo.mipLevels = 1;
        finalImageInfo.arrayLayers = 1;
        finalImageInfo.format = m_offscreenColorFormat;
        finalImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        finalImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        finalImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | // to be writable in a renderpass
                               VK_IMAGE_USAGE_SAMPLED_BIT |          // to be readable in a shader
                               VK_IMAGE_USAGE_STORAGE_BIT;           // to be writable for raytracing shaders
        finalImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        finalImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        m_finalImage = createUnique<VulkanImage>(m_context, finalImageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        // transition once to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL layout
        m_finalImage->transitionImageLayoutSingleTimeCmd(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        VkImageViewCreateInfo colorViewInfo{};
        colorViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        colorViewInfo.image = m_finalImage->getVkImage();
        colorViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorViewInfo.format = m_offscreenColorFormat;
        colorViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorViewInfo.subresourceRange.baseMipLevel = 0;
        colorViewInfo.subresourceRange.levelCount = 1;
        colorViewInfo.subresourceRange.baseArrayLayer = 0;
        colorViewInfo.subresourceRange.layerCount = 1;

        m_finalImage->createImageView(colorViewInfo);

        // sampler for later imgui access
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        // Addressing Mode: Clamp to edge is usually best for render targets
        // This prevents "wrapping" artifacts if the sampling coordinates go slightly outside [0,1]
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = m_context.getPhysicalDeviceProperties().limits.maxSamplerAnisotropy;
        // Unnormalized coordinates: Use normalized UVs (0.0 to 1.0)
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        // Comparison: Not for texture sampling, leave disabled
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f; // Only use base mip level

        m_finalImage->createSampler(samplerInfo);
    }

    void RenderLayer::createOffscreenDepthResources() {
        // DEPTH RESOURCE
        VkFormat depthFormat = m_context.findDepthFormat();

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_viewportExtent.width;
        imageInfo.extent.height = m_viewportExtent.height;
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

    void RenderLayer::createOffscreenFrameBuffer() {
        // Create the offscreen framebuffer
        std::array<VkImageView, 2> attachments = {m_sceneImage->getImageView(), m_offscreenDepthImage->getImageView()};

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_offscreenRenderPass->getHandle();
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_viewportExtent.width;
        framebufferInfo.height = m_viewportExtent.height;
        framebufferInfo.layers = 1;

        m_offscreenFb = createUnique<FrameBuffer>(m_context, framebufferInfo, "RenderLayer Offscreen Framebuffer",
                                                  m_sceneImage, m_offscreenDepthImage);
    }

    void RenderLayer::createRenderSystems() {
        m_pointLightSystem = createUnique<PointLightSystem>(m_context, m_offscreenRenderPass->getHandle(),
                                                            m_globalSetLayout.getDescriptorSetLayout());

        m_shadowMapRenderSystem =
            createUnique<ShadowMapRenderSystem>(m_context, m_descriptorAllocator, m_globalSetLayout);

        m_materialRenderSystem = createUnique<MaterialRenderSystem>(
            m_context, m_descriptorAllocator, m_textureRegistry, m_globalSetLayout, m_offscreenRenderPass->getHandle(),
            m_shadowMapRenderSystem->getShadowMapImageInfo());

        m_debugRenderSystem = createUnique<DebugRenderSystem>(m_context, m_descriptorAllocator, m_textureRegistry,
                                                              m_offscreenRenderPass->getHandle(), m_globalSetLayout);

        m_skyboxRenderSystem = createUnique<SkyboxRenderSystem>(m_context, m_environment, m_globalSetLayout,
                                                                m_offscreenRenderPass->getHandle());

        m_denoiserRenderSystem =
            createUnique<DenoiserRenderSystem>(m_context, m_descriptorAllocator, m_renderer.getSwapChainExtent());

        m_densityTextureSystem = createUnique<DensityTextureRenderSystem>(
            m_context, m_descriptorAllocator, VkExtent3D{256, 256, 256}, VkExtent3D{32, 32, 32});

        m_rayTracingRenderSystem = createUnique<RayTracingRenderSystem>(
            m_context, m_descriptorAllocator, m_textureRegistry, m_materialRegistry, m_blasRegistry, m_environment,
            m_globalSetLayout, m_sceneImage, *m_densityTextureSystem);

        m_objectPickingSystem =
            createUnique<ObjectPickingSystem>(m_context, m_descriptorAllocator, m_globalSetLayout, m_viewportExtent);

        m_compositionRenderSystem = createUnique<CompositionRenderSystem>(m_context, m_descriptorAllocator);

        m_selectionMaskRenderSystem = createUnique<SelectionMaskRenderSystem>(m_context, m_descriptorAllocator,
                                                                              m_globalSetLayout, m_viewportExtent);
    }

    void RenderLayer::reloadShaders() {
        // wait for the device to be idle before reloading shaders
        vkDeviceWaitIdle(m_context.getDevice());

        PXT_INFO("Reloading shaders in RenderLayer...");

        // reload shaders in all render systems
        if (m_isRaytracingEnabled) {
            m_rayTracingRenderSystem->reloadShaders();
            m_denoiserRenderSystem->reloadShaders();
        } else {
            m_materialRenderSystem->reloadShaders();
            m_debugRenderSystem->reloadShaders();
            m_skyboxRenderSystem->reloadShaders();
            m_pointLightSystem->reloadShaders();
            m_shadowMapRenderSystem->reloadShaders();
        }

        m_densityTextureSystem->reloadShaders();
        m_objectPickingSystem->reloadShaders();
        m_compositionRenderSystem->reloadShaders();
        m_selectionMaskRenderSystem->reloadShaders();

        PXT_INFO("Shaders reloaded successfully.");
    }

    void RenderLayer::onUpdate(FrameInfo& frameInfo, GlobalUbo& ubo) {
        // check if the user asked for the shaders to be reloaded
        if (m_isReloadShadersButtonPressed) {
            reloadShaders();

            m_isReloadShadersButtonPressed = false;
        }

        // update ubo buffer
        ubo.projection = frameInfo.camera.getProjectionMatrix();
        ubo.view = frameInfo.camera.getViewMatrix();
        ubo.inverseView = frameInfo.camera.getInverseViewMatrix();

        // update light values into ubo
        m_pointLightSystem->update(frameInfo, ubo);

        // update shadow map
        m_shadowMapRenderSystem->update(frameInfo, ubo);

        // update material descriptor set
        m_materialRegistry.updateDescriptorSet(frameInfo.frameIndex);

        // update raytracing scene
        if (m_isRaytracingEnabled) {
            m_denoiserRenderSystem->update(ubo);
            m_rayTracingRenderSystem->update(frameInfo);
        }
    }

    void RenderLayer::doRenderPasses(FrameInfo& frameInfo) {
        if (m_densityTextureSystem->needsRegeneration()) {
            m_densityTextureSystem->generate(frameInfo.commandBuffer);
        }

        // object picking
        if (m_isObjectPickingRequested || m_selectedEntityUUID != core::UUID::s_invalidId) {
            // here we render the scene to an offscreen buffer with object IDs as colors
            // and save the pixel color at the mouse coords inside a buffer.
            // we will read the buffer in the post frame update and reset the bool.
            m_objectPickingSystem->render(frameInfo, m_renderer, m_objectPickPixelCoords, m_isObjectPickingRequested);
        }

        // render to offscreen main render pass
        if (m_isRaytracingEnabled) {
            m_rayTracingRenderSystem->render(frameInfo, m_renderer);

            // transition the scene image to shader_read_only_optimal layout for denoiser sampling
            m_rayTracingRenderSystem->transitionImageToShaderReadOnlyOptimal(
                frameInfo, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

            if (m_isDenoisingEnabled) {
                m_denoiserRenderSystem->denoise(frameInfo, m_sceneImage);

                // this transitions the scene image back to shader_read_only_optimal for the next
                // renderpass (for now only point light billboards or ImGui Presentation)
                m_rayTracingRenderSystem->transitionImageToShaderReadOnlyOptimal(
                    frameInfo, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            }

            // begin offscreen render pass for point light billboards
            /*m_renderer.beginRenderPass(frameInfo.commandBuffer, m_offscreenRenderPass->getVkRenderPass(),
                    m_offscreenFb, m_renderer.getSwapChainExtent());

            //m_pointLightSystem->render(frameInfo);

            m_renderer.endRenderPass(frameInfo.commandBuffer);*/
        } else {
            // render shadow cube map
            // the render function of the shadow map render system will
            // do how many passes it needs to do (6 in this case - 1 point light)
            m_shadowMapRenderSystem->render(frameInfo, m_renderer);

            // begin offscreen render pass
            m_renderer.beginRenderPass(frameInfo.commandBuffer, *m_offscreenRenderPass, *m_offscreenFb,
                                       m_viewportExtent);

            m_skyboxRenderSystem->render(frameInfo);

            // choose if debug view or not
            if (m_isDebugEnabled) {
                m_debugRenderSystem->render(frameInfo);
            } else {
                m_materialRenderSystem->render(frameInfo);
            }

            m_pointLightSystem->render(frameInfo);

            m_renderer.endRenderPass(frameInfo.commandBuffer, *m_offscreenRenderPass, *m_offscreenFb);
        }

        // render selection mask
        m_selectionMaskRenderSystem->render(frameInfo, m_renderer, m_selectedEntityUUID);

        // composition pass (compute shader)
        m_compositionRenderSystem->render(frameInfo, *m_sceneImage, m_selectionMaskRenderSystem->getMaskColorImage(),
                                          m_objectPickingSystem->getObjectIdImage(), *m_finalImage,
                                          frameInfo.scene.getObjPickingIdFromEntityUUID(m_selectedEntityUUID));
    }

    void RenderLayer::onPostFrameUpdate(FrameInfo& frameInfo) {
        m_densityTextureSystem->postFrameUpdate(frameInfo.frameFence);

        if (m_isObjectPickingRequested) {
            uint32_t objPickingId = m_objectPickingSystem->postFrameUpdate(frameInfo.frameFence);
            core::UUID newSelectedEntityUUID = frameInfo.scene.getEntityUUIDFromObjPickingId(objPickingId);

            if (newSelectedEntityUUID != m_selectedEntityUUID) {
                // notify about the new selection
                m_selectedEntityUUID = newSelectedEntityUUID;

                Application::get().queueEvent(core::SelectedEntityChangedEvent(m_selectedEntityUUID));
            }

            m_isObjectPickingRequested = false;
        }
    }

    void RenderLayer::onEvent(core::Event& event) {
        core::EventDispatcher dispatcher(event);

        dispatcher.dispatch<core::ImGuiViewportResizeEvent>([this](auto& event) {
            m_viewportExtent = {event.getWidth(), event.getHeight()};
            recreateViewportResources();

            // update scene image for raytracing
            m_rayTracingRenderSystem->updateSceneImage(m_sceneImage);

            // update the denoiser's images with new extent
            m_denoiserRenderSystem->updateImages(m_viewportExtent);

            // update object picking system with new extent
            m_objectPickingSystem->updateImage(m_viewportExtent);

            m_selectionMaskRenderSystem->updateImage(m_viewportExtent);

            return true;
        });

        dispatcher.dispatch<core::PickObjectAtEvent>([this](auto& event) {
            m_objectPickPixelCoords = {event.getPixelX(), event.getPixelY()};
            m_isObjectPickingRequested = true;

            return true;
        });

        dispatcher.dispatch<core::SelectedEntityChangedEvent>([this](core::SelectedEntityChangedEvent& e) {
            m_selectedEntityUUID = e.getSelectedEntityUUID();
            return false;
        });
    }

    void RenderLayer::createDescriptorSetsImGui() {
        // DESCRIPTOR SET FOR IMGUI VIEWPORT
        m_finalImageDescriptorSetLayout =
            DescriptorSetLayout::Builder(m_context)
                .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
                .build();

        VkDescriptorImageInfo imageInfo;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_sceneImage->getImageView();
        imageInfo.sampler = m_sceneImage->getSamplerHandle();

        m_descriptorAllocator.allocate(m_finalImageDescriptorSetLayout->getDescriptorSetLayout(),
                                       m_finalImageDescriptorSet);

        DescriptorWriter(m_context, *m_finalImageDescriptorSetLayout)
            .writeImage(0, &imageInfo)
            .updateSet(m_finalImageDescriptorSet);
    }

    void RenderLayer::updateImguiDescriptorSet() {
        VkDescriptorImageInfo imageInfo;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_finalImage->getImageView();
        imageInfo.sampler = m_finalImage->getSamplerHandle();

        DescriptorWriter(m_context, *m_finalImageDescriptorSetLayout)
            .writeImage(0, &imageInfo)
            .updateSet(m_finalImageDescriptorSet);
    }

    void RenderLayer::onUpdateUi(FrameInfo& frameInfo) {
        ImGui::Begin("Raytracing Renderer");
        ImGui::Checkbox("Enable Raytracing", &m_isRaytracingEnabled);

        ImGui::TextColored(
            ImVec4(0.8, 0.6, 0.1, 1.0),
            "If changes were made to the %s shaders\n(prior of switching render type), you need to reload them!",
            m_isRaytracingEnabled ? "Raytracing" : "Rasterization");

        ui::Space::render(0.0, 10.0);

        if (m_isRaytracingEnabled) {
            m_rayTracingRenderSystem->updateUi();

            ImGui::Begin("Denoiser Settings");

            ImGui::Checkbox("Enable Denoising", &m_isDenoisingEnabled);

            if (m_isDenoisingEnabled)
                m_denoiserRenderSystem->updateUi();

            ImGui::End();
        }

        ImGui::End();

        ImGui::Begin("Debug Renderer");

        m_isReloadShadersButtonPressed = (ImGui::Button("Reload Shaders", ImVec2(150, 0)));

        ImGui::Checkbox("Enable Debug", &m_isDebugEnabled);

        if (m_isDebugEnabled) {
            ImGui::Text("Debug Renderer is enabled");
            m_debugRenderSystem->updateUi();
            m_densityTextureSystem->updateUi();
        } else {
            ImGui::Text("Debug Renderer is disabled");
        }
        ImGui::End();

        if (!m_isRaytracingEnabled) {
            m_shadowMapRenderSystem->updateUi();
        }
    }
} // namespace pxt