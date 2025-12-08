#pragma once

#include "pxtengine.h"

namespace pxt::editor {
	class SceneHierarchy {
	public:
		SceneHierarchy() = default;
		~SceneHierarchy() = default;

		void onUpdateUi(FrameInfo& frameInfo, core::UUID& selectedEntityId);
		void drawSceneEntityList(Scene& scene, core::UUID& selectedEntityId);
	};
}
