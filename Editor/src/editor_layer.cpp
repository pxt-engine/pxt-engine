#include "editor_layer.hpp"

namespace pxt::editor {
	EditorLayer::EditorLayer() : core::Layer("EditorLayer") {}

	void EditorLayer::onEvent(core::Event& event) {
		// here we have to decide which events to forward (not handled) if
		// the viewport is not focused (for now everything is blocked)
		if (!m_isViewportFocused) {
			core::Input::getState().reset();
			return;
		}
		
		core::EventDispatcher dispatcher(event);

		dispatcher.dispatch<core::MouseButtonPressEvent>([this](core::MouseButtonPressEvent& e) {
			return onMouseButtonPress(e);
		});
	}

	bool EditorLayer::onMouseButtonPress(core::MouseButtonPressEvent& event) {
		// handle mouse button press events here
		m_lastClickMousePos = core::Input::getState().getMousePosition();

		PXT_INFO("Mouse Button Pressed at position: ({}, {})",
			m_lastClickMousePos.x, m_lastClickMousePos.y);

		return false;
	}

	void EditorLayer::onUpdateUi(FrameInfo& frameInfo) {
		// first update scene hierarchy ui (an entity might be selected)
		m_sceneHierarchy.onUpdateUi(frameInfo, m_selectedEntityID);
		
		// then update entity inspector ui
		m_entityInspector.onUpdateUi(frameInfo, m_selectedEntityID);

		// main menu bar
		m_mainMenuBar.onUpdateUi(frameInfo);

		// maybe viewport class in the future?
		updateSceneUi(frameInfo.sceneDescriptorSet, frameInfo.sceneAspectRatio);
	}

	void EditorLayer::updateSceneUi(VkDescriptorSet sceneDescriptorSet, float sceneAspectRatio) {
		ImTextureID scene = (ImTextureID)sceneDescriptorSet;

		// we push a style var to remove the viewpoer window padding
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Viewport");

		m_isViewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_None);
		m_isViewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

		// we see the size of the window and we make the image fit the window with an aspect ratio
		ImVec2 windowSize = ImGui::GetContentRegionAvail();
		m_sceneImageExtentInViewport = getImageSizeWithAspectRatioForImGuiWindow(
			windowSize,
			sceneAspectRatio
		);

		// Calculate the horizontal and vertical offsets for centering
		float titleBarSize = ImGui::GetFrameHeight() * 2;
		float offsetX = (windowSize.x - m_sceneImageExtentInViewport.x) * 0.5f;
		float offsetY = (windowSize.y - m_sceneImageExtentInViewport.y + titleBarSize) * 0.5f;

		// Move the cursor to the calculated position
		// ImGui::SetCursorPos() sets the next drawing position relative to the top-left of the *content region*.
		ImGui::SetCursorPos(ImVec2(offsetX, offsetY));

		ImGui::Image(scene, m_sceneImageExtentInViewport);
		ImGui::End();
		ImGui::PopStyleVar();
	}

	ImVec2 EditorLayer::getImageSizeWithAspectRatioForImGuiWindow(
		ImVec2 windowSize, float aspectRatio) {
		ImVec2 ratioedExtent = { 0, 0 };

		// Calculate the width if the image fills the height
		float widthBasedOnHeight = windowSize.y * aspectRatio;

		// If filling the height makes the width exceed the window's width,
		// then the image must fill the width instead.
		if (widthBasedOnHeight > windowSize.x) {
			ratioedExtent.x = windowSize.x;
			ratioedExtent.y = windowSize.x / aspectRatio;
		}
		else {
			// Otherwise, fill the height
			ratioedExtent.x = widthBasedOnHeight;
			ratioedExtent.y = windowSize.y;
		}

		return ratioedExtent;
	}
}