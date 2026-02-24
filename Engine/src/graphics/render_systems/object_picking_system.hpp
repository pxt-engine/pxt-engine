#pragma once

#include "core/pch.hpp"
#include "graphics/context/context.hpp"
#include "graphics/descriptors/descriptors.hpp"
#include "graphics/frame_buffer.hpp"
#include "graphics/frame_info.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/render_pass.hpp"
#include "graphics/renderer.hpp"
#include "graphics/resources/vk_buffer.hpp"
#include "graphics/resources/vk_image.hpp"

namespace pxt {

    class ObjectPickingSystem {
    public:
        ObjectPickingSystem(Context& context,
                            const DescriptorSetLayout& globalSetLayout, VkExtent2D sceneImageExtent);
        ~ObjectPickingSystem();

        ObjectPickingSystem(const ObjectPickingSystem&) = delete;
        ObjectPickingSystem& operator=(const ObjectPickingSystem&) = delete;

        void render(FrameInfo& frameInfo, Renderer& renderer, u32vec2 mousePixelCoords, bool isObjectPickingRequested);

        void updateImage(VkExtent2D newExtent);
        void reloadShaders();
        [[nodiscard]] uint32_t postFrameUpdate(VkFence frameFence);

        [[nodiscard]] VulkanImage& getObjectIdImage() const { return *m_sceneWithColorIds; }

    private:
        void createRenderPass();
        void createSceneColorIdsImage();
        void createOffscreenDepthResources();
        void createOffscreenFrameBuffer();

        void createPixelColorBuffer();

        void createPipelineLayout(const DescriptorSetLayout& globalSetLayout);
        void createPipeline(bool useCompiledSpirvFiles = true);

        template <typename... Components>
        void processEntities(ComponentList<Components...> neededComponents, FrameInfo& frameInfo);

        Context& m_context;

        Unique<RenderPass> m_offscreenRenderPass = nullptr;
        Unique<FrameBuffer> m_offscreenFb = nullptr;

        // TODO: these images should be unique, but the framebuffer wrapper class
        // currently requires shared pointers -> change that class to receive references
        Shared<VulkanImage> m_sceneWithColorIds = nullptr;
        VkExtent2D m_sceneExtent;
        VkFormat m_offscreenColorFormat = VK_FORMAT_R8G8B8A8_UNORM;
        Shared<VulkanImage> m_offscreenDepthImage = nullptr;

        VkPipelineLayout m_pipelineLayout;
        Unique<Pipeline> m_pipeline = nullptr;

        Unique<VulkanBuffer> m_selectedPixelColorBuffer = nullptr;

        std::array<const std::string, 2> m_shaderFilePaths = {"obj_picking_shader.vert", "obj_picking_shader.frag"};
    };

} // namespace pxt