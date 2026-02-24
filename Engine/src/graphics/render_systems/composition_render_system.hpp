#pragma once

#include "core/pch.hpp"
#include "graphics/context/context.hpp"
#include "graphics/descriptors/descriptors.hpp"
#include "graphics/descriptors/descriptor_manager.hpp"
#include "graphics/frame_info.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/resources/vk_image.hpp"
#include "graphics/resources/vk_sampler.hpp"

namespace pxt {

    class CompositionRenderSystem {
    public:
        CompositionRenderSystem(Context& context, DescriptorManager& descriptorManager);

        ~CompositionRenderSystem();

        CompositionRenderSystem(const CompositionRenderSystem&) = delete;
        CompositionRenderSystem& operator=(const CompositionRenderSystem&) = delete;

        void render(FrameInfo& frameInfo, VulkanImage& sceneColor, VulkanImage& selectionMask, VulkanImage& objIdsImage, VulkanImage& outputImage,
                    uint32_t selectedObjPickingId);

        void reloadShaders();

    private:
        void createDescriptorSet();
        void createPipelineLayout();
        void createPipeline(bool useCompiledSpirvFiles = true);

    private:
        Context& m_context;
        DescriptorManager& m_descriptorManager;

        // this dscriptor set will not be needed when samplers
        // are separable from images themselves
        // (we need this DS to hold the images with nearest sampling)
        DescriptorSetHandle m_descriptorSet = core::UID::s_invalidId;

        VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
        Unique<Pipeline> m_pipeline = nullptr;

        Shared<VulkanSampler> m_nearestSampler = nullptr;

        const std::string m_shaderPath = "obj_outline.comp";
    };

} // namespace pxt
