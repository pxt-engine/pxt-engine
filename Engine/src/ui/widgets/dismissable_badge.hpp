#pragma once

namespace pxt::ui {
    class DismissableBadge {
    public:
        static bool render(const char* label, bool* open, const ImVec2& size = ImVec2(0, 30));
        static bool renderWithIcon(const char* label, bool* open, ImTextureID icon,
                                   const ImVec2& size = ImVec2(0, 30));
    };
} // namespace pxt::ui