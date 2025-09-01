#include "graphics/render_systems/ui_render_system.hpp"

namespace PXTEngine {

	UiRenderSystem::UiRenderSystem(Context& context, VkRenderPass renderPass) : m_context(context) {
		initImGui(renderPass);

		registerComponents();
	}

	UiRenderSystem::~UiRenderSystem() {
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void UiRenderSystem::initImGui(VkRenderPass& renderPass) {
		// we need one set per imgui rendered texture NOT PER FRAME!!! (fonts included)
		// ImGui will use the same descriptor set for all textures.
		// ImGui will use this pool for fonts and its stuff, textures will be allocated from the descriptor allocator growable
		// TODO: maybe set format will change
		m_imGuiPool = DescriptorPool::Builder(m_context)
			.setMaxSets(2) 
			.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2)
			.setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
			.build();

		std::vector<PoolSizeRatio> poolRatios = {
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5.0f },
		};

		m_imguiDescriptorAllocator = createUnique<DescriptorAllocatorGrowable>(
			m_context,
			8,       // starting sets per pool
			poolRatios,
			2.0f,     // growth factor
			512       // max sets cap
		);

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		// enable docking and load ini file
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		io.IniFilename = IMGUI_INI_FILEPATH.c_str();
		PXT_INFO("ImGui .ini file set to: {}", io.IniFilename);

		ImGui_ImplGlfw_InitForVulkan(m_context.getWindow().getBaseWindow(), true);
		ImGui_ImplVulkan_InitInfo initInfo{};
		initInfo.Instance = m_context.getInstance();
		initInfo.PhysicalDevice = m_context.getPhysicalDevice();
		initInfo.Device = m_context.getDevice();
		initInfo.QueueFamily = m_context.findPhysicalQueueFamilies().graphicsFamily;
		initInfo.Queue = m_context.getGraphicsQueue();
		initInfo.RenderPass = renderPass;
		initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		initInfo.PipelineCache = VK_NULL_HANDLE;
		initInfo.DescriptorPool = m_imGuiPool->getDescriptorPool();
		initInfo.Allocator = nullptr;
		initInfo.MinImageCount = SwapChain::MAX_FRAMES_IN_FLIGHT;
		initInfo.ImageCount = SwapChain::MAX_FRAMES_IN_FLIGHT;
		initInfo.CheckVkResultFn = [](VkResult err) {
			if (err != VK_SUCCESS) {
				std::cerr << "[Vulkan Error] VkResult: " << err << std::endl;
				assert(false);
			}
		};
		ImGui_ImplVulkan_Init(&initInfo);

		ImGui_ImplVulkan_CreateFontsTexture();
	}

	VkDescriptorSet UiRenderSystem::addImGuiTexture(VkSampler sampler, VkImageView imageView, VkImageLayout layout) {
		VkDescriptorSet descriptorSet;
		
		Unique<DescriptorSetLayout> imguiLayout = DescriptorSetLayout::Builder(m_context)
			.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build();

		m_imguiDescriptorAllocator->allocate(imguiLayout->getDescriptorSetLayout(), descriptorSet);

		VkDescriptorImageInfo descImage{};
		descImage.sampler = sampler;
		descImage.imageView = imageView;
		descImage.imageLayout = layout;

		DescriptorWriter(m_context, *imguiLayout)
			.writeImage(0, &descImage)
			.updateSet(descriptorSet);

		return descriptorSet;
	}

	void UiRenderSystem::drawSceneEntityList(Scene& scene)
	{
		ImGui::Begin("Scene Entities");
		if (ImGui::Button("Add Entity")) {
			scene.createEntity("New Entity");
		}
		ImGui::Separator();

		// draw all entities in the scene
		auto view = scene.getEntitiesWith<IDComponent, NameComponent>();
		for (auto entityHandle : view) {
			const auto& [idComponent, nameComponent] = view.get<IDComponent, NameComponent>(entityHandle);

			bool selected = (m_selectedEntityID == idComponent.uuid);
			if (ImGui::Selectable(nameComponent.name.c_str(), selected)) {
				m_selectedEntityID = idComponent.uuid;
				isAnEntitySelected = true;
			}
		}
		ImGui::End();
	}

	void UiRenderSystem::drawEntityInspector(Scene& scene) {
		ImGui::Begin("Entity Inspector");
		if (isAnEntitySelected) {
			Entity entity = scene.getEntity(m_selectedEntityID);

			if (entity) {
				// draw registered components
				for (auto& info : m_componentUiRegistry) {
					info.drawer(entity);
				}
			}
		}
		else {
			ImGui::Text("No entity selected");
		}

		ImGui::End();
	}

	void UiRenderSystem::render(FrameInfo& frameInfo) {
		buildUi(frameInfo.scene);

		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frameInfo.commandBuffer);
	}

	void UiRenderSystem::beginBuildingUi() {
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// MAIN MENU BAR (might become a method itself in the future)
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("Open...")) {
					// TODO: Implement "Open" logic here
					printf("File -> Open... clicked!\n");
				}
				if (ImGui::MenuItem("Exit")) {
					// TODO: Implement "Exit" logic here
					printf("File -> Exit clicked!\n");
				}
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}

		// IMPORTANT: This is required for docking to work in the main window (for customizations, view imgui_demo.cpp)
		ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
	}

	void UiRenderSystem::buildUi(Scene& scene) {
		drawSceneEntityList(scene);
		drawEntityInspector(scene);

		ImGui::ShowMetricsWindow();
	}

	void UiRenderSystem::registerComponents() {
		// IDComponent
		RegisterComponent<IDComponent>("IDComponent", [](auto& c) {
			ImGui::Text("UUID: %s", c.uuid.toString().c_str());
		});

		// NameComponent
		RegisterComponent<NameComponent>("NameComponent", [](auto& c) {
			char buffer[25];
			memset(buffer, 0, sizeof(buffer));
			strncpy(buffer, c.name.c_str(), sizeof(buffer) - 1);
			if (ImGui::InputText("Name (max 25 chars)", buffer, sizeof(buffer))) {
				c.name = buffer;
			}
		});

		// ColorComponent
		RegisterComponent<ColorComponent>("ColorComponent", [](auto& c) {
			ImGui::ColorEdit3("Color", glm::value_ptr(c.color));
		});

		// VolumeComponent
		RegisterComponent<VolumeComponent>("VolumeComponent", [](auto& c) {
			ImGui::ColorEdit3("Absorption", glm::value_ptr(c.volume.absorption));
			ImGui::ColorEdit3("Scattering", glm::value_ptr(c.volume.scattering));
			ImGui::SliderFloat("PhaseFunctionG", &c.volume.phaseFunctionG, -1.0f, 1.0f, "%.2f");
			ImGui::SeparatorText("Density Texture");
			if (c.volume.densityTextureId == std::numeric_limits<uint32_t>::max()) {
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
			}
		});

		// MaterialComponent
		RegisterComponent<MaterialComponent>("MaterialComponent", [](auto& c) {
			if (c.material) {
				ImGui::Text("Material: %s", c.material->alias.c_str());
				c.material->drawMaterialUi();
			}
			else {
				ImGui::Text("No Material assigned");
			}

			ImGui::SliderFloat("Texture Tiling Factor", &c.tilingFactor, 0.0f, 25.0f);
			ImGui::ColorEdit3("Tint", glm::value_ptr(c.tint));
		});

		// Transform2dComponent
		RegisterComponent<Transform2dComponent>("Transform2dComponent", [](auto& c) {
			ImGui::DragFloat2("Translation", glm::value_ptr(c.translation), 0.01f);
			ImGui::DragFloat2("Scale", glm::value_ptr(c.scale), 0.01f);
			ImGui::DragFloat("Rotation", &c.rotation, 0.01f, -360.0f, 360.0f);
		});

		// TransformComponent
		RegisterComponent<TransformComponent>("TransformComponent", [](auto& c) {
			ImGui::DragFloat3("Translation", glm::value_ptr(c.translation), 0.01f);
			ImGui::DragFloat3("Scale", glm::value_ptr(c.scale), 0.01f);
			ImGui::DragFloat3("Rotation", glm::value_ptr(c.rotation), 0.01f);
		});

		// MeshComponent
		RegisterComponent<MeshComponent>("MeshComponent", [](MeshComponent& c) {
			ImGui::Text("Mesh name: %s", c.mesh->alias.c_str());
		});

		// ScriptComponent
		RegisterComponent<ScriptComponent>("ScriptComponent", [](ScriptComponent& c) {
			if (c.script) {
				ImGui::Text("Script instance: %p", c.script);
			}
			else {
				ImGui::Text("No script bound.");
			}
		});

		// CameraComponent
		RegisterComponent<CameraComponent>("CameraComponent", [](CameraComponent& c) {
			ImGui::BeginDisabled(true); //TODO: remove when we can choose which camera to use
			ImGui::Checkbox("Main Camera", &c.isMainCamera);
			ImGui::EndDisabled();

			c.camera.drawCameraUi();
		});

		// PointLightComponent
		RegisterComponent<PointLightComponent>("PointLightComponent", [](PointLightComponent& c) {
			ImGui::DragFloat("Intensity", &c.lightIntensity, 0.1f, 0.0f, 10.0f);
		});
	}
}