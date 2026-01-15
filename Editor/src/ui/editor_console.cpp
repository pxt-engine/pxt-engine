#include "ui/editor_console.hpp"

namespace pxt::editor {

    EditorConsole::EditorConsole() {}

    void EditorConsole::push(EditorLogEntry entry) { m_entries.emplace_back(std::move(entry)); }

    void EditorConsole::clear() { m_entries.clear(); }

    ImVec4 getLogLevelColor(const core::LogLevel level) {
        switch (level) {
        case core::LogLevel::Debug:
            return {60 / 255.f, 127 / 255.f, 185 / 255.f, 1};
        case core::LogLevel::Warn:
            return {255 / 255.f, 127 / 255.f, 40 / 255.f, 1};
        case core::LogLevel::Error:
        case core::LogLevel::Fatal:
            return {237 / 255.f, 28 / 255.f, 36 / 255.f, 1};
        default:
            return {17 / 255.f, 129 / 255.f, 13 / 255.f, 1};
        }
    }

    std::string getLogLevelPrefix(const core::LogLevel level) {
        switch (level) {
        case core::LogLevel::Info:
            return "[INFO] ";
        case core::LogLevel::Debug:
            return "[DEBUG] ";
        case core::LogLevel::Warn:
            return "[WARN] ";
        case core::LogLevel::Error:
            return "[ERROR] ";
        case core::LogLevel::Fatal:
            return "[FATAL] ";
        default:
            return "";
        }
    }

    void EditorConsole::onUpdateUi() {

        ImGui::Begin("Editor Console");

        if (ImGui::Button("Clear"))
            clear();

        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_autoScroll);

        ImGui::Separator();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 1));
        ImGui::BeginChild("LogRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        for (const auto& entry : m_entries) {
            constexpr ImVec4 textColor = {1, 1, 1, 1};

            // Color the prefix and use white for text
            ImVec4 levelColor = getLogLevelColor(entry.level);
            std::string prefix = getLogLevelPrefix(entry.level);
            ImGui::PushStyleColor(ImGuiCol_Text, levelColor);
            ImGui::TextUnformatted(prefix.c_str());
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text, textColor);
            ImGui::TextWrapped("%s", entry.message.c_str());
            ImGui::PopStyleColor();
        }

        if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::End();
    }

} // namespace pxt::editor