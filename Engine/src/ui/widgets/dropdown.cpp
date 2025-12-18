#include "ui/widgets/dropdown.hpp"

namespace pxt::ui {
    void Dropdown::render(const char* label, int& currentItem, std::span<const char*> itemsName) {
        if (ImGui::BeginCombo(label, itemsName[currentItem])) {
            for (int n = 0; n < itemsName.size(); n++) {
                const bool isSelected = (currentItem == n);
                if (ImGui::Selectable(itemsName[n], isSelected)) {
                    currentItem = n;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }
} // namespace pxt::ui