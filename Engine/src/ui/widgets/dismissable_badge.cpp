#include "ui/widgets/dismissable_badge.hpp"

namespace pxt::ui {
    bool DismissableBadge::render(const char* label, bool* open, const ImVec2& size) {
        if (!*open)
            return false;

        bool clicked = false;
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);

        float width = size.x > 0 ? size.x : ImGui::GetContentRegionAvail().x;
        float height = size.y > 0 ? size.y : (g.FontSize + style.FramePadding.y * 2.0f);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImRect bb(pos, ImVec2(pos.x + width, pos.y + height));

        ImGui::ItemSize(bb, style.FramePadding.y);
        if (!ImGui::ItemAdd(bb, id))
            return false;

        bool hovered, held;
        if (ImGui::ButtonBehavior(bb, id, &hovered, &held)) {
            clicked = true;
            *open = false;
        }

        const ImU32 col = ImGui::GetColorU32((held && hovered) ? ImGuiCol_ButtonActive
                                             : hovered         ? ImGuiCol_ButtonHovered
                                                               : ImGuiCol_Button);
        window->DrawList->AddRectFilled(bb.Min, bb.Max, col, style.FrameRounding);

        float centerY = pos.y + (height / 2.0f);

        // calculate "X" position first to know the text limit
        constexpr const char* closeX = "X";
        ImVec2 xSize = ImGui::CalcTextSize(closeX);
        float xPos = pos.x + width - xSize.x - style.FramePadding.x;

        // calculate text constraints
        float textStartX = pos.x + style.FramePadding.x;
        // Limit text to stop before the 'X' and a small spacing buffer
        float textRightLimit = xPos - style.ItemSpacing.x;

        // render Text with Clipping
        const char* textEnd = ImGui::FindRenderedTextEnd(label);
        ImVec2 textSize = ImGui::CalcTextSize(label, textEnd);

        // Define the clipping rectangle: (Left, Top, Right, Bottom)
        // This prevents the text from drawing beyond 'textRightLimit'
        ImVec4 clipRect(pos.x, pos.y, textRightLimit, pos.y + height);

        window->DrawList->AddText(g.Font, g.FontSize, ImVec2(textStartX, centerY - (textSize.y / 2.0f)),
                                  ImGui::GetColorU32(ImGuiCol_Text), label, textEnd, 0.0f, &clipRect);

        // render "X"
        window->DrawList->AddText(ImVec2(xPos, centerY - (xSize.y / 2.0f)), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                                  closeX);

        return clicked;
    }

    /**
     * @brief Renders a dismissible tag/button with an icon on the left.
     * @return true if the button was clicked.
     */
    bool DismissableBadge::renderWithIcon(const char* label, bool* open, ImTextureID icon, const ImVec2& size) {
        if (!*open)
            return false;

        bool clicked = false;
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);

        // determine size and position
        float width = size.x > 0 ? size.x : ImGui::GetContentRegionAvail().x;
        float height = size.y > 0 ? size.y : (g.FontSize + style.FramePadding.y * 2.0f);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImRect bb(pos, ImVec2(pos.x + width, pos.y + height));

        // interaction
        ImGui::ItemSize(bb, style.FramePadding.y);
        if (!ImGui::ItemAdd(bb, id))
            return false;

        bool hovered, held;
        if (ImGui::ButtonBehavior(bb, id, &hovered, &held)) {
            clicked = true;
            *open = false;
        }

        // render Background
        const ImU32 col = ImGui::GetColorU32((held && hovered) ? ImGuiCol_ButtonActive
                                             : hovered         ? ImGuiCol_ButtonHovered
                                                               : ImGuiCol_Button);
        window->DrawList->AddRectFilled(bb.Min, bb.Max, col, style.FrameRounding);

        // component Calculations
        float centerY = pos.y + (height / 2.0f);

        // Calculate "X" (Right Side)
        constexpr const char* closeX = "X";
        ImVec2 xSize = ImGui::CalcTextSize(closeX);
        float xPos = pos.x + width - xSize.x - style.FramePadding.x;

        // Calculate Icon (Left Side)
        float currentX = pos.x + style.FramePadding.x;
        float iconSize = height - (style.FramePadding.y * 2.0f);

        // Draw Icon
        window->DrawList->AddImage(icon, ImVec2(currentX, centerY - (iconSize / 2.0f)),
                                   ImVec2(currentX + iconSize, centerY + (iconSize / 2.0f)));

        // render Text with Clipping
        float textStartX = currentX + iconSize + style.ItemSpacing.x;
        float textRightLimit = xPos - style.ItemSpacing.x; // Boundary before the X

        const char* textEnd = ImGui::FindRenderedTextEnd(label);
        ImVec2 textSize = ImGui::CalcTextSize(label, textEnd);

        // Clipping Rect: (Left, Top, Right, Bottom)
        ImVec4 clipRect(textStartX, pos.y, textRightLimit, pos.y + height);

        window->DrawList->AddText(g.Font, g.FontSize, ImVec2(textStartX, centerY - (textSize.y / 2.0f)),
                                  ImGui::GetColorU32(ImGuiCol_Text), label, textEnd, 0.0f, &clipRect);

        // render "X"
        window->DrawList->AddText(ImVec2(xPos, centerY - (xSize.y / 2.0f)), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                                  closeX);

        return clicked;
    }
} // namespace pxt::ui