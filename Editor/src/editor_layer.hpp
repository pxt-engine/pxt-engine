#pragma once

#include "pxtengine.h"
#include "ui/entity_inspector.hpp"
#include "ui/scene_hierarchy.hpp"
#include "ui/main_menu_bar.hpp"

namespace pxt::editor {
	class EditorLayer : public core::Layer {
	public:
		EditorLayer();
		
		void onEvent(core::Event& event) override;
		void onUpdateUi(FrameInfo& frameInfo) override;

	private:
		void updateSceneUi(VkDescriptorSet sceneDescriptorSet, float sceneAspectRatio);
		ImVec2 getImageSizeWithAspectRatioForImGuiWindow(ImVec2 windowSize, float aspectRatio);
		bool onMouseButtonPress(core::MouseButtonPressEvent& event);

		bool m_isViewportFocused = false;
		bool m_isViewportHovered = false;

		glm::vec2 m_lastClickMousePosImGui = { 0.0f, 0.0f };

		SceneHierarchy m_sceneHierarchy{};
		EntityInspector m_entityInspector{};
		MainMenuBar m_mainMenuBar{};

		core::UUID m_selectedEntityID = core::UUID::s_invalidId;

		ImVec2 m_sceneImageExtent{ 0.f, 0.f };
		ImVec2 m_viewportUpperLeftScreenCoord{ 0.f, 0.f };
	};
}