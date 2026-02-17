#include "ui/environment.hpp"

#include "scene/environment.hpp"
#include "ui/drag_and_drop.hpp"
#include "ui/resource_slot.hpp"

namespace pxt::editor {
    void EnvironmentUi::onUpdateUi(FrameInfo& frameInfo) {
        ImGui::Begin("Environment");

        Shared<Environment> environment = frameInfo.scene.getEnvironment();
        if (!environment->getSkybox()) {
            ImGui::Text("No skybox set for the scene.");
            ImGui::End();
        }

        bool dirtyColor = false;

        glm::vec3 ambientLightColor = environment->getAmbientLightColor();
        float ambientLightIntensity = environment->getAmbientLightIntensity();

        if (ImGui::ColorEdit3("Sky Color", glm::value_ptr(ambientLightColor))) {
            dirtyColor = true;
        }
        if (ImGui::DragFloat("Sky Intensity", &(ambientLightIntensity), 0.01f, 0.f, 1.f)) {
            dirtyColor = true;
        }

        if (dirtyColor) {
            environment->setAmbientLight(glm::vec4(ambientLightColor, ambientLightIntensity));
        }

        auto skybox = std::static_pointer_cast<VulkanSkybox>(environment->getSkybox());

        core::UID cubeMapId = skybox->getCubeMap().id;

        // build the expected payload for the drag-and-drop target
        DragAndDrop::EnginePayload skyboxPayload = {
            .id = cubeMapId, .source = DragAndDrop::PayloadSource::AssetBrowser, .type = Resource::Type::Image};

        if (ResourceSlot::render("Skybox Texture", skyboxPayload, frameInfo.rm)) {
            std::string path = frameInfo.rm.get<Image>(skyboxPayload.id)->alias;

            std::array<std::string, 6> skyboxTextures{};
            skyboxTextures.fill(path);

            environment->setSkybox(skyboxTextures);
            skybox = std::static_pointer_cast<VulkanSkybox>(environment->getSkybox()); // get the newly set skybox
        }

        drawSkybox(skybox);

        ImGui::End();
    }

    void EnvironmentUi::drawSkybox(Shared<VulkanSkybox> skybox) {

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
    }

} // namespace pxt::editor