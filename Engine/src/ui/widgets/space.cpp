#include "ui/widgets/space.hpp"

namespace PXTEngine::UI {
	void Space::render(float width, float height) {
		ImGui::Dummy(ImVec2(width, height));
	}
}