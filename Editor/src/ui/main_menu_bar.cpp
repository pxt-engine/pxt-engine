#include "main_menu_bar.hpp"
#include "core/events/window_event.hpp"

namespace pxt::editor {
    void MainMenuBar::onUpdateUi(FrameInfo& frameInfo) {
        saveSceneModal(frameInfo.scene);

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open...")) {
                    // TODO: Implement "Open" logic here
                    printf("File -> Open... clicked!\n");
                }
                if (ImGui::MenuItem("Save Scene")) {
                    m_openSaveSceneDialog = true;
                }
                if (ImGui::MenuItem("Exit")) {
                    Application::get().queueEvent(core::WindowCloseEvent());
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        if (m_openSaveSceneDialog) {
            ImGui::OpenPopup("Save Scene Dialog");
            m_openSaveSceneDialog = false;
        }
    }

    void MainMenuBar::saveSceneModal(Scene& scene) {
        // Always center this window when appearing
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("Save Scene Dialog", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char sceneNameBuffer[64] = "";
            ImGui::Text("Enter Scene Name: (64 max)");
            ImGui::InputText("##SceneName", sceneNameBuffer, IM_ARRAYSIZE(sceneNameBuffer));

            if (ImGui::Button("OK", ImVec2(120, 0))) {
                std::string sceneName = sceneNameBuffer;
                if (!sceneName.empty()) {
                    scene.setName(sceneName);

                    // TODO: Use a project file system manager (save, create, load,	etc.)
                    auto rm = Application::get().getResourceManager();
                    SceneSerializer serializer(&scene, rm);
                    serializer.serialize(SCENES_PATH + sceneName + ".pxtscene");
                    PXT_INFO("Saving scene with name: {}\n", sceneName);
                }

                ImGui::CloseCurrentPopup();
                memset(sceneNameBuffer, 0, sizeof(sceneNameBuffer));
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
} // namespace pxt::editor