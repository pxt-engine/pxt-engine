#pragma once

#include "core/pch.hpp"
#include "graphics/swap_chain.hpp"
#include "graphics/context/context.hpp"
#include "graphics/frame_info.hpp"
#include "graphics/descriptors/descriptors.hpp"
#include "scene/ecs/entity.hpp"

namespace PXTEngine {

	struct ComponentUiInfo {
		std::string name;
		// the function that will draw the ImGui component
		std::function<void(Entity)> drawer;
	};

	class UiRenderSystem {
	public:
		UiRenderSystem(Context& context, VkRenderPass renderPass);
		~UiRenderSystem();

		UiRenderSystem(const UiRenderSystem&) = delete;
		UiRenderSystem& operator=(const UiRenderSystem&) = delete;

		void beginBuildingUi(Scene& scene);
		void render(FrameInfo& frameInfo);

	private:
		void initImGui(VkRenderPass& renderPass);
		void registerComponents();

		VkDescriptorSet addImGuiTexture(VkSampler sampler, VkImageView imageView, VkImageLayout layout);

		void saveSceneUi(Scene& scene);
		void drawSceneEntityList(Scene& scene);
		void drawEntityInspector(Scene& scene);
		void buildUi(Scene& scene);

		Context& m_context;

		Unique<DescriptorAllocatorGrowable> m_imguiDescriptorAllocator;
		Unique<DescriptorPool> m_imGuiPool{};

		std::vector<ComponentUiInfo> m_componentUiRegistry;
		UUID m_selectedEntityID; // currently selected entity in the inspector
		bool m_isAnEntitySelected = false;
		bool m_openSaveSceneDialog = false;

		/*
		 *@brief Registers a component of type T into the m_componentUiRegistry.
		 *		 Each element has a name and a function that dictates how it is
		 *		 drawn into the entity inspector drawer.
		 *
		*/
		template<typename T>
		void RegisterComponent(const std::string& name, std::function<void(T&)> uiFunction) {
			m_componentUiRegistry.push_back({
				name,
				[=](PXTEngine::Entity entity) {
					if (entity.has<T>()) {
						T& component = entity.get<T>();
						if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
							uiFunction(component);

							ImGui::TreePop();
						}
						ImGui::Dummy({ 0.0f, 5.0f }); // spacing
					}
				}
			});
		}
	};
}