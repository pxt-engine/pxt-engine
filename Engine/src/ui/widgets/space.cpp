#include "ui/widgets/space.hpp"

namespace pxt::ui {
    void Space::render(float width, float height) { ImGui::Dummy(ImVec2(width, height)); }
} // namespace pxt::ui