#pragma once

#include "core/events/event.hpp"
#include "core/layer/layer.hpp"
#include "core/pch.hpp"
#include "graphics/context/context.hpp"
#include "graphics/descriptors/descriptors.hpp"
#include "graphics/frame_info.hpp"
#include "graphics/renderer.hpp"
#include "graphics/resources/blas_registry.hpp"
#include "graphics/resources/material_registry.hpp"
#include "graphics/resources/texture_registry.hpp"

#include "graphics/frame_buffer.hpp"
#include "graphics/render_pass.hpp"
#include "graphics/render_systems/composition_render_system.hpp"
#include "graphics/render_systems/debug_render_system.hpp"
#include "graphics/render_systems/denoiser_render_system.hpp"
#include "graphics/render_systems/density_texture_system.hpp"
#include "graphics/render_systems/material_render_system.hpp"
#include "graphics/render_systems/object_picking_system.hpp"
#include "graphics/render_systems/point_light_system.hpp"
#include "graphics/render_systems/raytracing_render_system.hpp"
#include "graphics/render_systems/selection_mask_render_system.hpp"
#include "graphics/render_systems/shadow_map_render_system.hpp"
#include "graphics/render_systems/skybox_render_system.hpp"
#include "graphics/render_systems/editor_grid_render_system.hpp"

#include "scene/environment.hpp"

namespace pxt {
    class RenderLayer : public core::Layer {
    public:
        RenderLayer(Context& context, Renderer& renderer, DescriptorAllocatorGrowable& descriptorAllocator,
                    TextureRegistry& textureRegistry, MaterialRegistry& materialRegistry, BLASRegistry& blasRegistry,
                    DescriptorSetLayout& globalSetLayout, Shared<Environment> environment);

        ~RenderLayer();

        RenderLayer(const RenderLayer&) = delete;
        RenderLayer& operator=(const RenderLayer&) = delete;
        RenderLayer(RenderLayer&&) = delete;
        RenderLayer& operator=(RenderLayer&&) = delete;

        VkDescriptorSet getImGuiSceneDescriptorSet() const { return m_finalImageDescriptorSet; }

        void onUpdate(FrameInfo& frameInfo, GlobalUbo& ubo) override;
        void onUpdateUi(FrameInfo& frameInfo) override;
        void onPostFrameUpdate(FrameInfo& frameInfo) override;
        void onEvent(core::Event& event) override;

        void doRenderPasses(FrameInfo& frameInfo);

        float getSceneAspectRatio() const {
            return static_cast<float>(m_viewportExtent.width) / static_cast<float>(m_viewportExtent.height);
        }

    private:
        void recreateViewportResources();
        void createRenderPass();
        void createSceneImage();
        void createFinalImage();
        void createOffscreenDepthResources();
        void createOffscreenFrameBuffer();
        void createRenderSystems();

        void reloadShaders();

        void createDescriptorSetsImGui();
        void updateImguiDescriptorSet();

        Context& m_context;
        Renderer& m_renderer;
        TextureRegistry& m_textureRegistry;
        MaterialRegistry& m_materialRegistry;
        BLASRegistry& m_blasRegistry;

        DescriptorAllocatorGrowable& m_descriptorAllocator;

        DescriptorSetLayout& m_globalSetLayout;

        Shared<Environment> m_environment;

        std::array<Unique<VulkanBuffer>, SwapChain::MAX_FRAMES_IN_FLIGHT> m_uboBuffers;

        Unique<MaterialRenderSystem> m_materialRenderSystem = nullptr;
        Unique<PointLightSystem> m_pointLightSystem = nullptr;
        Unique<ShadowMapRenderSystem> m_shadowMapRenderSystem = nullptr;
        Unique<DebugRenderSystem> m_debugRenderSystem = nullptr;
        Unique<SkyboxRenderSystem> m_skyboxRenderSystem = nullptr;
        Unique<RayTracingRenderSystem> m_rayTracingRenderSystem = nullptr;
        Unique<DenoiserRenderSystem> m_denoiserRenderSystem = nullptr;
        Unique<DensityTextureRenderSystem> m_densityTextureSystem = nullptr;
        Unique<ObjectPickingSystem> m_objectPickingSystem = nullptr;
        Unique<CompositionRenderSystem> m_compositionRenderSystem = nullptr;
        Unique<SelectionMaskRenderSystem> m_selectionMaskRenderSystem = nullptr;
        Unique<EditorGridRenderSystem> m_editorGridRenderSystem = nullptr;

        Unique<RenderPass> m_offscreenRenderPass;
        Unique<FrameBuffer> m_offscreenFb;

        Shared<VulkanImage> m_sceneImage;
        VkFormat m_offscreenColorFormat;

        Shared<VulkanImage> m_offscreenDepthImage;

        Unique<VulkanImage> m_finalImage = nullptr;

        VkDescriptorSet m_finalImageDescriptorSet = VK_NULL_HANDLE;
        Unique<DescriptorSetLayout> m_finalImageDescriptorSetLayout = nullptr;

        // this initial value will never be used, as it will be updated
        // on the first ViewportResizeEvent. That will happen
        // the first frame the ImGui viewport is created.
        VkExtent2D m_viewportExtent{1600, 900};

        bool m_isDebugEnabled = false;
        bool m_isRaytracingEnabled = false;
        bool m_isReloadShadersButtonPressed = false;
        bool m_isDenoisingEnabled = true;

        bool m_isObjectPickingRequested = false;
        u32vec2 m_objectPickPixelCoords{0, 0};

        core::UID m_selectedEntityUID{core::UID::s_invalidId};
    };
} // namespace pxt