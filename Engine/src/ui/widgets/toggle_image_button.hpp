#pragma once

#include "core/pch.hpp"

namespace pxt::ui {

    /*
     * @brief Renders a toggle button.
     */
    class ToggleImageButton {
    public:
        template <typename T>
        static bool render(ImTextureID texture, const char* strId, const char* tooltip, const T& valueOn,
                           const T& valueOff, T& currentValue, const ImVec2& size,
                           const ImVec4& activeColor = ImVec4(0.05f, 0.25f, 0.65f, 1.f),
                           const ImVec4& activeHoveredColor = ImVec4(0.30f, 0.50f, 0.90f, 1.00f),
                           const ImVec4& activePressedColor = ImVec4(0.20f, 0.40f, 0.80f, 1.00f),
                           const ImVec4& inactiveColor = ImVec4(0.10f, 0.1f, 0.1f, 1.f),
                           const ImVec4& inactiveHovered = ImVec4(0.25f, 0.25f, 0.25f, 1.f),
                           const ImVec4& inactivePressed = ImVec4(0.20f, 0.20f, 0.20f, 1.f)) {
            const bool isActive = (currentValue == valueOn);

            if (isActive) {
                ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeHoveredColor);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, activePressedColor);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, inactiveColor);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, inactiveHovered);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, inactivePressed);
            }

            bool clicked = ImGui::ImageButton(strId, texture, size, ImVec2(0, 0), ImVec2(1, 1));

            if (clicked) {
                if (isActive) {
                    // Deselect
                    currentValue = valueOff;
                } else {
                    currentValue = valueOn;
                }
            }

            if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
                ImGui::SetTooltip("%s", tooltip);
            }

            ImGui::PopStyleColor(3);

            return clicked;
        }
    };
} // namespace pxt::ui