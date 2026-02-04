#include "ui/entity_inspector.hpp"
#include "ui/drag_and_drop.hpp"
#include "ui/resource_slot.hpp"

namespace pxt::editor {

    EntityInspector::EntityInspector() { registerComponents(); }

    EntityInspector::~EntityInspector() {}

    void EntityInspector::drawEntityInspector(Scene& scene, const core::UID& selectedEntityId) {
        ImGui::Begin("Entity Inspector");

        if (selectedEntityId == core::UID::s_invalidId) {
            ImGui::Text("No entity selected");
            ImGui::End();

            return;
        }

        Entity entity = scene.getEntity(selectedEntityId);

        if (entity) {
            // draw components
            for (auto& info : m_componentUiRegistry) {
                info.drawer(entity);
            }
        }

        // --- Centered, wide button ---
        const float buttonWidth = 200.0f;
        float availWidth = ImGui::GetContentRegionAvail().x;
        float cursorX = ImGui::GetCursorPosX();
        ImGui::SetCursorPosX(cursorX + (availWidth - buttonWidth) * 0.5f);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 5));
        if (ImGui::Button("Add Component", ImVec2(buttonWidth, 0.0f))) {
            m_openAddComponentWindow = true;
        }
        ImGui::PopStyleVar();

        // Capture button rect in screen space
        ImVec2 buttonMin = ImGui::GetItemRectMin();
        ImVec2 buttonMax = ImGui::GetItemRectMax();

        // --- Add Component window ---
        if (m_openAddComponentWindow) {
            ImGuiWindowFlags addComponentWindowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                                                       ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize;

            ImVec2 windowSize(200.0f, 250.0f);

            // Position directly below the button
            ImVec2 windowPos(buttonMin.x, buttonMax.y);

            ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
            ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);

            // make window bg color less transparent
            ImGuiStyle& style = ImGui::GetStyle();
            ImVec4 windowBgColor = style.Colors[ImGuiCol_WindowBg];
            windowBgColor.w = 0.97f;
            ImGui::PushStyleColor(ImGuiCol_WindowBg, windowBgColor);

            ImGui::Begin("Add Component", &m_openAddComponentWindow, addComponentWindowFlags);

            m_openAddComponentWindow &= ImGui::IsWindowFocused();

            static ImGuiTextFilter simpleFilter;
            simpleFilter.Draw("Search");

            for (auto& info : m_componentUiRegistry) {
                // skip essential components
                if (info.essential) {
                    continue;
                }

                if (simpleFilter.PassFilter(info.name.c_str())) {
                    if (ImGui::Selectable(info.name.c_str())) {
                        info.addComponent(entity);
                        m_openAddComponentWindow = false;
                    }
                }
            }

            ImGui::End();
            ImGui::PopStyleColor();
        }

        ImGui::End();
    }

    void EntityInspector::onUpdateUi(FrameInfo& frameInfo, const core::UID& selectedEntityId) {
        drawEntityInspector(frameInfo.scene, selectedEntityId);
    }

    static void DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f) {
        ImGui::PushID(label.c_str());

        // Create a table with 2 columns.
        // ImGuiTableFlags_SizingFixedFit ensures the label column only takes what it needs.
        if (ImGui::BeginTable("##table", 2)) {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 75.0f);
            ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text(label.c_str());

            ImGui::TableSetColumnIndex(1);

            // Logic for X, Y, Z controls
            ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());

            float innerSpacing = 4.0f;
            float groupSpacing = 8.0f;
            float lineHeight = ImGui::GetFontSize() + GImGui->Style.FramePadding.y * 2.0f;
            ImVec2 buttonSize = {lineHeight + 3.0f, lineHeight};

            // --- X ---
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.8f, 0.1f, 0.15f, 1.0f});
            if (ImGui::Button("X", buttonSize))
                values.x = resetValue;
            ImGui::PopStyleColor();
            ImGui::SameLine(0, innerSpacing);
            ImGui::DragFloat("##X", &values.x, 0.1f);
            ImGui::PopItemWidth();
            ImGui::SameLine(0, groupSpacing);

            // --- Y ---
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.2f, 0.7f, 0.2f, 1.0f});
            if (ImGui::Button("Y", buttonSize))
                values.y = resetValue;
            ImGui::PopStyleColor();
            ImGui::SameLine(0, innerSpacing);
            ImGui::DragFloat("##Y", &values.y, 0.1f);
            ImGui::PopItemWidth();
            ImGui::SameLine(0, groupSpacing);

            // --- Z ---
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.1f, 0.25f, 0.8f, 1.0f});
            if (ImGui::Button("Z", buttonSize))
                values.z = resetValue;
            ImGui::PopStyleColor();
            ImGui::SameLine(0, innerSpacing);
            ImGui::DragFloat("##Z", &values.z, 0.1f);
            ImGui::PopItemWidth();

            ImGui::EndTable();
        }

        ImGui::PopID();
    }

    void EntityInspector::registerComponents() {
        // IDComponent
        RegisterComponent<IDComponent>(
            "IDComponent", [](auto& c, Entity entity) { ImGui::Text("UID: %s", c.uid.toString().c_str()); });

        // NameComponent
        RegisterComponent<NameComponent>("NameComponent", [](auto& c, Entity entity) {
            std::array<char, 25> textBuffer;

            memset(textBuffer.data(), 0, textBuffer.size());
            strncpy(textBuffer.data(), c.name.c_str(), textBuffer.size() - 1);

            bool shouldUpdate = false;

            // The if returns true when Enter is pressed
            if (ImGui::InputText("Name (max 25 chars)", textBuffer.data(), textBuffer.size(),
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                shouldUpdate = true;
            }

            // The if returns true when the input field loses focus after an edit
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                shouldUpdate = true;
            }

            if (shouldUpdate) {
                std::string newName = textBuffer.data();

                if (newName.empty()) {
                    newName = "Unnamed Entity";
                }

                auto sceneOpt = entity.tryGetScene();
                if (!sceneOpt.has_value()) [[unlikely]] {
                    PXT_WARN("Entity \"{}\" has no scene!", entity.getUID().toString());
                    return;
                }

                Scene& scene = sceneOpt->get();
                // here we ensure the new name is unique in the scene, else we append a number
                newName = scene.getUniqueEntityName(newName);

                PXT_INFO("Renamed Entity \"{}\" to \"{}\"", c.name, newName);
                c.name = newName;
            }
        });

        // Transform2dComponent
        RegisterComponent<Transform2dComponent>("Transform2dComponent", [](auto& c, Entity entity) {
            ImGui::DragFloat2("Translation", glm::value_ptr(c.translation), 0.01f);
            ImGui::DragFloat2("Scale", glm::value_ptr(c.scale), 0.01f);
            ImGui::DragFloat("Rotation", &c.rotation, 0.01f, -360.0f, 360.0f);
        });

        // TransformComponent
        RegisterComponent<TransformComponent>("TransformComponent", [](auto& c, Entity entity) {
            DrawVec3Control("Translation", c.translation);

            glm::vec3 rotationDegrees = glm::degrees(c.rotation);
            DrawVec3Control("Rotation", rotationDegrees);
            c.rotation = glm::radians(rotationDegrees);

            DrawVec3Control("Scale", c.scale, 1.0f); // Default scale reset to 1.0
        });

        // ColorComponent
        RegisterComponent<ColorComponent>(
            "ColorComponent", [](auto& c, Entity entity) { ImGui::ColorEdit3("Color", glm::value_ptr(c.color)); });

        // VolumeComponent
        RegisterComponent<VolumeComponent>("VolumeComponent", [](auto& c, Entity entity) {
            ImGui::ColorEdit3("Absorption", glm::value_ptr(c.volume.absorption));
            ImGui::ColorEdit3("Scattering", glm::value_ptr(c.volume.scattering));
            ImGui::SliderFloat("PhaseFunctionG", &c.volume.phaseFunctionG, -1.0f, 1.0f, "%.2f");
            ImGui::SeparatorText("Density Texture");
            // TODO: volume textures
            /*if (c.volume.densityTextureId == std::numeric_limits<uint32_t>::max()) {
                    ImGui::Text("Not selected");
            }
            else {
                    ImGui::Text("Texture ID: %u", c.volume.densityTextureId);
            }

            ImGui::SeparatorText("Detail Texture");
            if (c.volume.detailTextureId == std::numeric_limits<uint32_t>::max()) {
                    ImGui::Text("Not selected");
            }
            else {
                    ImGui::Text("Texture ID: %u", c.volume.detailTextureId);
            }*/
        });

        // MaterialComponent
        RegisterComponent<MaterialComponent>("MaterialComponent", [](auto& c, Entity entity) {
            ResourceManager& rm = Application::get().getResourceManager();

            DragAndDrop::EnginePayload payload = {c.material, DragAndDrop::PayloadSource::AssetBrowser,
                                                  Resource::Type::Material};

            if (ResourceSlot::render("##entity-inspector-material-component-resource-slot", payload, rm)) {
                entity.update<MaterialComponent>([&payload](auto& matComp) { matComp.material = payload.id; });

                std::string newMaterialAlias = c.material.isValid() ? rm.get<Material>(c.material)->alias : "No Material Assigned";

                PXT_INFO("Changed Material of Entity \"{}\" to Mesh \"{}\"", entity.getName(), newMaterialAlias);
            }

            if (c.material.isValid()) {
                auto material = rm.get<Material>(c.material);
                ImGui::Text("Material: %s", material->alias.c_str());
                material->drawMaterialUi();
            } else {
                ImGui::Text("No Material assigned");
            }

            ImGui::SliderFloat("Texture Tiling Factor", &c.tilingFactor, 0.0f, 25.0f);
            ImGui::ColorEdit3("Tint", glm::value_ptr(c.tint));
        });

        // MeshComponent
        RegisterComponent<MeshComponent>("MeshComponent", [](MeshComponent& c, Entity entity) {
            DragAndDrop::EnginePayload payload = {c.mesh, DragAndDrop::PayloadSource::AssetBrowser,
                                                  Resource::Type::Mesh};

            ResourceManager& rm = Application::get().getResourceManager();

            if (ResourceSlot::render("##entity-inspector-mesh-component-resource-slot", payload, rm)) {
                entity.update<MeshComponent>([&payload](auto& meshComp) { meshComp.mesh = payload.id; });

                std::string newMeshAlias = c.mesh.isValid() ? rm.get<Mesh>(c.mesh)->alias : "No Mesh Assigned";

                PXT_INFO("Changed Mesh of Entity \"{}\" to Mesh \"{}\"", entity.getName(), newMeshAlias);
            }
        });

        // CameraComponent
        RegisterComponent<CameraComponent>("CameraComponent", [](CameraComponent& c, Entity entity) {
            auto sceneOpt = entity.tryGetScene();
            core::UID entUID = entity.getUID();

            if (!sceneOpt.has_value()) [[unlikely]] {
                PXT_WARN("Entity ({}) has no scene!", entUID.toString());
            }
            Scene& scene = sceneOpt->get();
            bool isActiveCamera = scene.getActiveCameraEntityUID() == entUID;

            if (ImGui::Checkbox("Active", &isActiveCamera)) {
                if (isActiveCamera) {
                    scene.setActiveCameraEntity(entUID);
                } else {
                    scene.setActiveCameraEntity(core::UID::s_invalidId);
                }
            }

            ImGui::SameLine(0.f, 20.f);

            c.cameraData.drawCameraUi();
        });

        // PointLightComponent
        RegisterComponent<PointLightComponent>("PointLightComponent", [](PointLightComponent& c, Entity entity) {
            ImGui::DragFloat("Intensity", &c.lightIntensity, 0.1f, 0.0f, 10.0f);
            ImGui::ColorEdit3("Color", glm::value_ptr(c.lightColor));
        });

        // ScriptComponent
        RegisterComponent<ScriptComponent>("ScriptComponent", [](ScriptComponent& c, Entity entity) {
            if (c.script) {
                ImGui::Text("Script instance: %p", c.script);
            } else {
                ImGui::Text("No script bound.");
            }
        });
    }
} // namespace pxt::editor