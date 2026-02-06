#pragma once

#include "core/pch.hpp"

namespace pxt::ui {

    class ToggleButton {
    public:
        // --- ICON VERSION ---
        template <typename T>
        static bool icon(const char* iconOn, const char* iconOff, const char* strId, const char* tooltip,
                         const T& valueOn, const T& valueOff, T& currentValue, const ImVec2& size,
                         const ImVec2& innerPadding = ImGui::GetStyle().FramePadding,
                         const ImVec4& activeColor = ImVec4(0.569f, 0.325f, 0.859f, 1.f),
                         const ImVec4& inactiveColor = ImVec4(0.1f, 0.1f, 0.1f, 1.f)) {

            ImGui::PushID(strId);

            // we need this to return to the button initial position
            ImVec2 backupPos = ImGui::GetCursorScreenPos();

            const bool isActive = (currentValue == valueOn);
            pushStyle(isActive, activeColor, inactiveColor);

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, innerPadding);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f); // Make it invisible

            bool clicked = ImGui::ButtonEx("##invisible", size, ImGuiButtonFlags_AlignTextBaseLine);

            ImGui::PopStyleVar();
            ImGui::PopStyleVar();

            // check if the invisible button is hovered
            bool isButtonHovered = ImGui::IsItemHovered();

            constexpr ImVec4 hoverIconColor = ImVec4(1.f, 1.f, 1.f, 1.f);
            constexpr ImVec4 defaultIconColor = ImVec4(0.6f, 0.6f, 0.6f, 1.f);

            // reset cursor and draw the actual icon text with the right color
            ImGui::SetCursorScreenPos(backupPos);
            ImVec4 iconColor = isButtonHovered ? hoverIconColor : defaultIconColor;

            ImGui::PushStyleColor(ImGuiCol_Text, iconColor);

            const char* icon = isActive ? iconOn : iconOff;
            ImGui::Text(icon); // Render just the text/icon

            ImGui::PopStyleColor();

            handleLogic(clicked, isActive, valueOn, valueOff, currentValue, tooltip);

            popStyle(3);
            ImGui::PopID();
            return clicked;
        }

        // --- IMAGE VERSION ---
        template <typename T>
        static bool image(ImTextureID texture, const char* strId, const char* tooltip, const T& valueOn,
                          const T& valueOff, T& currentValue, const ImVec2& size,
                          const ImVec2& innerPadding = ImGui::GetStyle().FramePadding,
                          const ImVec4& activeColor = ImVec4(0.569f, 0.325f, 0.859f, 1.f),
                          const ImVec4& inactiveColor = ImVec4(0.1f, 0.1f, 0.1f, 1.f)) {

            const bool isActive = (currentValue == valueOn);
            pushStyle(isActive, activeColor, inactiveColor);

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, innerPadding);
            bool clicked = ImGui::ImageButton(strId, texture, size, ImVec2(0, 0), ImVec2(1, 1));
            ImGui::PopStyleVar();

            handleLogic(clicked, isActive, valueOn, valueOff, currentValue, tooltip);

            popStyle(3);
            return clicked;
        }

    private:
        // Helper to handle colors
        static void pushStyle(bool isActive, const ImVec4& activeColor, const ImVec4& inactiveColor) {
            ImVec4 base = isActive ? activeColor : inactiveColor;

            // Auto-calculate hover/active shades (slightly lighter/darker)
            ImVec4 hovered = ImVec4(base.x * 1.2f, base.y * 1.2f, base.z * 1.2f, base.w);
            ImVec4 pressed = ImVec4(base.x * 0.8f, base.y * 0.8f, base.z * 0.8f, base.w);

            ImGui::PushStyleColor(ImGuiCol_Button, base);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, pressed);
        }

        static void popStyle(const int pushedVarsCount) { ImGui::PopStyleColor(pushedVarsCount); }

        template <typename T>
        static void handleLogic(bool clicked, bool isActive, const T& valueOn, const T& valueOff, T& currentValue,
                                const char* tooltip) {
            if (clicked) {
                currentValue = isActive ? valueOff : valueOn;
            }
            if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
                ImGui::SetTooltip("%s", tooltip);
            }
        }
    };
} // namespace pxt::ui