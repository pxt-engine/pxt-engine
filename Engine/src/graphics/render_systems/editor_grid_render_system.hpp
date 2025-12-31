#pragma once

#include "core/pch.hpp"
#include "graphics/context/context.hpp"
#include "graphics/frame_info.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/descriptors/descriptors.hpp"

namespace pxt {

    // TODO: this class' behavior should be defined by the editor using future Engine APIs
    class EditorGridRenderSystem {
    public:
        EditorGridRenderSystem(Context& context, DescriptorSetLayout& globalSetLayout,
                             VkRenderPass renderPass);
        ~EditorGridRenderSystem();

        EditorGridRenderSystem(const EditorGridRenderSystem&) = delete;
        EditorGridRenderSystem& operator=(const EditorGridRenderSystem&) = delete;

        void render(FrameInfo& frameInfo);
        void reloadShaders();

    private:
        void createPipelineLayout(DescriptorSetLayout& globalSetLayout);
        void createPipeline(bool useCompiledSpirvFiles = true);

        Context& m_context;

        VkRenderPass m_renderPassHandle;
        Unique<Pipeline> m_pipeline;
        VkPipelineLayout m_pipelineLayout;

        std::array<const std::string, 2> m_shaderFilePaths = {"grid_shader.vert", "grid_shader.frag"};
    };
} // namespace pxt