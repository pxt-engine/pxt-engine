#include "ui/environment.hpp"

#include "graphics/resources/vk_skybox.hpp"
#include "scene/environment.hpp"

namespace pxt::editor {
    void EnvironmentUi::onUpdateUi(FrameInfo& frameInfo) {
        ImGui::Begin("Environment");

        Shared<Environment> environment = frameInfo.scene.getEnvironment();
        if (!environment->getSkybox()) {
            ImGui::Text("No skybox set for the scene.");
            ImGui::End();
        }

        auto skybox = std::static_pointer_cast<VulkanSkybox>(environment->getSkybox());

        ImTextureID cube_posx = (ImTextureID)skybox->getDebugDescriptorSet(0);
        ImTextureID cube_negx = (ImTextureID)skybox->getDebugDescriptorSet(1);
        ImTextureID cube_posy =
            (ImTextureID)skybox->getDebugDescriptorSet(3); // swap negative and positive y because vulkan :)
        ImTextureID cube_negy = (ImTextureID)skybox->getDebugDescriptorSet(2);
        ImTextureID cube_posz = (ImTextureID)skybox->getDebugDescriptorSet(4);
        ImTextureID cube_negz = (ImTextureID)skybox->getDebugDescriptorSet(5);

        /* Render the cube map textures flat out in this format (with y mirrored):
        //                +----+
                          | +Y |
                +----+----+----+----+
                | -X | +Z | +X | -Z |
                +----+----+----+----+
                          | -Y |
                          +----+
        */

        const float horizontalAvailSpace = ImGui::GetContentRegionAvail().x;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        ImVec2 faceSize = ImVec2((horizontalAvailSpace / 4) - spacing, (horizontalAvailSpace / 4) - spacing);
        float totalMiddleRowWidth = faceSize.x * 4 + spacing * 3;
        float offsetToCenter = (ImGui::GetContentRegionAvail().x - totalMiddleRowWidth) * 0.5f;

        // Row 1: Centered +Y
        ImGui::SetCursorPosX(offsetToCenter + faceSize.x + spacing); // Center it over 4 middle-row faces
        ImGui::Image(cube_posy, faceSize, ImVec2(0, 1), ImVec2(1, 0));

        // Row 2: -X +Z +X -Z
        ImGui::SetCursorPosX(offsetToCenter); // Align middle row
        ImGui::Image(cube_negx, faceSize, ImVec2(0, 1), ImVec2(1, 0));
        ImGui::SameLine();
        ImGui::Image(cube_posz, faceSize, ImVec2(0, 1), ImVec2(1, 0));
        ImGui::SameLine();
        ImGui::Image(cube_posx, faceSize, ImVec2(0, 1), ImVec2(1, 0));
        ImGui::SameLine();
        ImGui::Image(cube_negz, faceSize, ImVec2(0, 1), ImVec2(1, 0));

        // Row 3: Centered -Y
        ImGui::SetCursorPosX(offsetToCenter + faceSize.x + spacing); // Same X as +Y
        ImGui::Image(cube_negy, faceSize, ImVec2(0, 1), ImVec2(1, 0));

        ImGui::End();
    }

} // namespace pxt::editor