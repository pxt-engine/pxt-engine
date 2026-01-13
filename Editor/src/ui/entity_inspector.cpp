#include "ui/entity_inspector.hpp"

namespace pxt::editor {

    EntityInspector::EntityInspector() { registerComponents(); }

    EntityInspector::~EntityInspector() {}

    void EntityInspector::drawEntityInspector(Scene& scene, const core::UUID& selectedEntityId) {
        ImGui::Begin("Entity Inspector");
        if (selectedEntityId != core::UUID::s_invalidId) {
            Entity entity = scene.getEntity(selectedEntityId);

            if (entity) {
                // draw registered components
                for (auto& info : m_componentUiRegistry) {
                    info.drawer(entity);
                }
            }

            // --- Centered, wide button ---
            const float buttonWidth = 200.0f;
            float availWidth = ImGui::GetContentRegionAvail().x;
            float cursorX = ImGui::GetCursorPosX();
            ImGui::SetCursorPosX(cursorX + (availWidth - buttonWidth) * 0.5f);

            if (ImGui::Button("Add Component", ImVec2(buttonWidth, 0.0f))) {
                m_openAddComponentWindow = true;
            }

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

                ImGui::Begin("Add Component", &m_openAddComponentWindow, addComponentWindowFlags);

                static ImGuiTextFilter simpleFilter;
                simpleFilter.Draw("Search");

                const char* lines[] = {"aaa1.c",   "bbb1.c",   "ccc1.c", "aaa2.cpp",
                                       "bbb2.cpp", "ccc2.cpp", "abc.h",  "hello, world"};

                for (int i = 0; i < IM_ARRAYSIZE(lines); i++) {
                    if (simpleFilter.PassFilter(lines[i])) {
                        if (ImGui::Selectable(lines[i])) {
                            // TODO: add component logic with a name->type map
                            PXT_DEBUG("clicked on {}", lines[i]);
                            m_openAddComponentWindow = false;
                        }
                    }
                }

                ImGui::End();
            }

        } else {
            ImGui::Text("No entity selected");
        }

        ImGui::End();
    }

    void EntityInspector::onUpdateUi(FrameInfo& frameInfo, const core::UUID& selectedEntityId) {
        drawEntityInspector(frameInfo.scene, selectedEntityId);
    }

    void EntityInspector::registerComponents() {
        // IDComponent
        RegisterComponent<IDComponent>(
            "IDComponent", [](auto& c, Entity entity) { ImGui::Text("core::UUID: %s", c.uuid.toString().c_str()); });

        // NameComponent
        RegisterComponent<NameComponent>("NameComponent", [](auto& c, Entity entity) {
            char buffer[25];
            memset(buffer, 0, sizeof(buffer));
            strncpy(buffer, c.name.c_str(), sizeof(buffer) - 1);
            if (ImGui::InputText("Name (max 25 chars)", buffer, sizeof(buffer))) {
                c.name = buffer;
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
            ImGui::DragFloat3("Translation", glm::value_ptr(c.translation), 0.01f);
            ImGui::DragFloat3("Scale", glm::value_ptr(c.scale), 0.01f);

            glm::vec3 rotationDegrees = glm::degrees(c.rotation);
            if (ImGui::DragFloat3("Rotation", glm::value_ptr(rotationDegrees), 0.5f)) {
                c.rotation = glm::radians(rotationDegrees);
            }
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
            if (c.material) {
                ImGui::Text("Material: %s", c.material->alias.c_str());
                c.material->drawMaterialUi();
            } else {
                ImGui::Text("No Material assigned");
            }

            ImGui::SliderFloat("Texture Tiling Factor", &c.tilingFactor, 0.0f, 25.0f);
            ImGui::ColorEdit3("Tint", glm::value_ptr(c.tint));
        });

        // MeshComponent
        RegisterComponent<MeshComponent>("MeshComponent", [](MeshComponent& c, Entity entity) {
            ImGui::Text("Mesh name: %s", c.mesh->alias.c_str());
        });

        // CameraComponent
        RegisterComponent<CameraComponent>("CameraComponent", [](CameraComponent& c, Entity entity) {
            auto sceneOpt = entity.tryGetScene();
            core::UUID entUUID = entity.getUUID();

            if (!sceneOpt.has_value()) [[unlikely]] {
                PXT_WARN("Entity ({}) has no scene!", entUUID.toString());
            }
            Scene& scene = sceneOpt->get();
            bool isActiveCamera = scene.getActiveCameraEntityUUID() == entUUID;

            if (ImGui::Checkbox("Active", &isActiveCamera)) {
                if (isActiveCamera) {
                    scene.setActiveCameraEntity(entUUID);
                } else {
                    scene.setActiveCameraEntity(core::UUID::s_invalidId);
                }
            }

            ImGui::SameLine(0.f, 20.f);

            c.cameraData.drawCameraUi();
        });

        // PointLightComponent
        RegisterComponent<PointLightComponent>("PointLightComponent", [](PointLightComponent& c, Entity entity) {
            ImGui::DragFloat("Intensity", &c.lightIntensity, 0.1f, 0.0f, 10.0f);
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