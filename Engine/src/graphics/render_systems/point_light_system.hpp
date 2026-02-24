#pragma once

#include "core/pch.hpp"
#include "graphics/context/context.hpp"
#include "graphics/descriptors/descriptor_manager.hpp"
#include "graphics/frame_info.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/swap_chain.hpp"
#include "scene/camera_data.hpp"
#include "scene/scene.hpp"

namespace pxt {

    class PointLightSystem {
    public:
        PointLightSystem(Context& context, DescriptorManager& descriptorManager, VkRenderPass renderPass, DescriptorSetHandle globalDescriptorSet);
        ~PointLightSystem();

        PointLightSystem(const PointLightSystem&) = delete;
        PointLightSystem& operator=(const PointLightSystem&) = delete;

        void update(FrameInfo& frameInfo, GlobalUbo& ubo);
        void render(FrameInfo& frameInfo);
        void reloadShaders();

    private:
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
        void createPipeline(bool useCompiledSpirvFiles = true);

        Context& m_context;

        DescriptorManager& m_descriptorManager;
        DescriptorSetHandle m_globalDescriptorSet = core::UID::s_invalidId;

        VkRenderPass m_renderPass;
        Unique<Pipeline> m_pipeline;
        VkPipelineLayout m_pipelineLayout;

        std::array<const std::string, 2> m_shaderFilePaths = {"point_light_billboard.vert",
                                                              "point_light_billboard.frag"};
    };
} // namespace pxt