#include "ui/scene_hierarchy.hpp"

namespace pxt::editor {
	void SceneHierarchy::onUpdateUi(FrameInfo& frameInfo, core::UUID& selectedEntityId) {
		drawSceneEntityList(frameInfo.scene, selectedEntityId);
	}

	void SceneHierarchy::drawSceneEntityList(Scene& scene, core::UUID& selectedEntityId) {
		ImGui::Begin("Scene Entities");

		if (ImGui::Button("Add Entity")) {
			scene.createEntity("New Entity");
		}

		ImGui::Separator();

		// draw all entities in the scene
		auto view = scene.getEntitiesWith<IDComponent, NameComponent>();
		for (auto entityHandle : view) {
			const auto& [idComponent, nameComponent] = view.get<IDComponent, NameComponent>(entityHandle);

			bool selected = (selectedEntityId == idComponent.uuid);
			if (ImGui::Selectable(nameComponent.name.c_str(), selected)) {
				selectedEntityId = idComponent.uuid;
			}
		}
		ImGui::End();
	}
}