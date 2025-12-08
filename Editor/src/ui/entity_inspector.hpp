#pragma once

#include "pxtengine.h"
#include "ui/widgets/space.hpp"

namespace pxt::editor {

	struct ComponentUiInfo {
		std::string name;
		// the function that will draw the ImGui component
		std::function<void(Entity)> drawer;
	};

	class EntityInspector {
	public:
		EntityInspector();
		~EntityInspector();

		EntityInspector(const EntityInspector&) = delete;
		EntityInspector& operator=(const EntityInspector&) = delete;

		void registerComponents();

		void drawEntityInspector(Scene& scene, const core::UUID& selectedEntityId);
		void onUpdateUi(FrameInfo& frameInfo, const core::UUID& selectedEntityId);

	private:
		std::vector<ComponentUiInfo> m_componentUiRegistry;

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
				[=](pxt::Entity entity) {
					if (entity.has<T>()) {
						T& component = entity.get<T>();
						if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
							uiFunction(component);

							ImGui::TreePop();
						}
						ui::Space::render(0.0f, 5.0f);
					}
				}
				});
		}
	};
}