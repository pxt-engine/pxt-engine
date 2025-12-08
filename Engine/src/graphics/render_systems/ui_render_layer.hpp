#pragma once

#include "core/pch.hpp"
#include "core/events/event.hpp"
#include "core/layer/layer.hpp"
#include "graphics/swap_chain.hpp"
#include "graphics/renderer.hpp"
#include "graphics/context/context.hpp"
#include "graphics/frame_info.hpp"
#include "graphics/descriptors/descriptors.hpp"
#include "scene/ecs/entity.hpp"
#include "ui/widgets/space.hpp"

namespace pxt {
	class UiRenderLayer : public core::Layer {
	public:
		UiRenderLayer(Context& context, VkRenderPass renderPass);
		~UiRenderLayer();

		UiRenderLayer(const UiRenderLayer&) = delete;
		UiRenderLayer& operator=(const UiRenderLayer&) = delete;

		void onEvent(core::Event& event) override;
		void beginFrame(Scene& scene, Renderer& renderer, FrameInfo& frameInfo);
		void render(FrameInfo& frameInfo, Renderer& renderer);

	private:
		void initImGui(VkRenderPass& renderPass);

		VkDescriptorSet addImGuiTexture(VkSampler sampler, VkImageView imageView, VkImageLayout layout);

		void saveSceneUi(Scene& scene);
		void buildUi(Scene& scene);

		Context& m_context;

		Unique<DescriptorAllocatorGrowable> m_imguiDescriptorAllocator;
		Unique<DescriptorPool> m_imGuiPool{};

		bool m_openSaveSceneDialog = false;
	};
}