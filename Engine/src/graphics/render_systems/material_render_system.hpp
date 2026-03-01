#pragma once

#include "core/pch.hpp"
#include "graphics/context/context.hpp"
#include "graphics/descriptors/descriptors.hpp"
#include "graphics/descriptors/descriptor_manager.hpp"
#include "graphics/frame_info.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/resources/texture_registry.hpp"
#include "graphics/swap_chain.hpp"
#include "scene/scene.hpp"

namespace pxt {

    class MaterialRenderSystem {
    public:
        MaterialRenderSystem(Context& context, DescriptorManager& descriptorManager,
                             TextureRegistry& textureRegistry, const DescriptorSetLayout& globalSetLayout,
                             VkRenderPass renderPass, VkDescriptorImageInfo shadowMapImageInfo);
        ~MaterialRenderSystem();

        MaterialRenderSystem(const MaterialRenderSystem&) = delete;
        MaterialRenderSystem& operator=(const MaterialRenderSystem&) = delete;

        void render(FrameInfo& frameInfo);
        void reloadShaders();

    private:
        void createDescriptorSets(VkDescriptorImageInfo shadowMapImageInfo);
        void createPipelineLayout(const DescriptorSetLayout& globalSetLayout);
        void createPipeline(bool useCompiledSpirvFiles = true);

        template <typename... Components>
        void processEntities(ComponentList<Components...> neededComponents, FrameInfo& frameInfo);

        Context& m_context;
        TextureRegistry& m_textureRegistry;

        VkRenderPass m_renderPassHandle;
        Unique<Pipeline> m_pipeline;
        VkPipelineLayout m_pipelineLayout;

        DescriptorManager& m_descriptorManager;

        DescriptorSetHandle m_shadowMapDescriptorSet = core::UID::s_invalidId;

        std::array<const std::string, 2> m_shaderFilePaths = {"material_shader.vert", "material_shader.frag"};
    };
} // namespace pxt