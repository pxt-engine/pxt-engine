#pragma once

#include "core/pch.hpp"
#include "graphics/context/context.hpp"
#include "graphics/descriptors/descriptors.hpp"
#include "graphics/frame_buffer.hpp"
#include "graphics/frame_info.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/render_pass.hpp"
#include "graphics/renderer.hpp"
#include "graphics/resources/vk_image.hpp"

namespace pxt {

    class SelectionMaskRenderSystem {
    public:
        SelectionMaskRenderSystem(Context& context,
                                  const DescriptorSetLayout& globalSetLayout, VkExtent2D sceneImageExtent);
        ~SelectionMaskRenderSystem();

        SelectionMaskRenderSystem(const SelectionMaskRenderSystem&) = delete;
        SelectionMaskRenderSystem& operator=(const SelectionMaskRenderSystem&) = delete;

        void render(FrameInfo& frameInfo, Renderer& renderer, core::UID selectedEntityUID);

        void updateImage(VkExtent2D newExtent);
        void reloadShaders();

        [[nodiscard]] VulkanImage& getMaskColorImage() const { return *m_selectionMaskImage; }

    private:
        void createRenderPass();
        void createMaskColorImage();
        void createOffscreenFrameBuffer();

        void createPipelineLayout(const DescriptorSetLayout& globalSetLayout);
        void createPipeline(bool useCompiledSpirvFiles = true);

        Context& m_context;

        Unique<RenderPass> m_offscreenRenderPass = nullptr;
        Unique<FrameBuffer> m_offscreenFb = nullptr;

        // TODO: these images should be unique, but the framebuffer wrapper class
        // currently requires shared pointers -> change that class to receive references
        Shared<VulkanImage> m_selectionMaskImage = nullptr;
        VkExtent2D m_sceneExtent;
        VkFormat m_offscreenColorFormat = VK_FORMAT_R8_UNORM;

        VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
        Unique<Pipeline> m_pipeline = nullptr;

        std::array<const std::string, 2> m_shaderFilePaths = {"selection_mask_shader.vert",
                                                              "selection_mask_shader.frag"};
    };

} // namespace pxt