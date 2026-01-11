#include "application.hpp"

#include "core/diagnostics.hpp"
#include "core/events/engine_state_events.hpp"
#include "core/events/event_dispatcher.hpp"
#include "core/events/window_event.hpp"
#include "core/input/input.hpp"
#include "graphics/dummy_view_provider.hpp"
#include "graphics/resources/texture2d.hpp"
#include "scene/camera_data.hpp"
#include "scene/ecs/component.hpp"
#include "scene/ecs/entity.hpp"

#include "tracy/Tracy.hpp"

namespace pxt {

    Application* Application::m_instance = nullptr;

    Application::Application() { m_instance = this; }

    Application::~Application() {};

    void Application::start() {
        PXT_PROFILE_FN();

        m_resourceManagerPtr = pushLayer<ResourceManager>();

        // load default and scene assets and register them in the resource registry
        createDefaultResources();
        {
            PXT_PROFILE("pxt::Application::loadScene");
            loadScene();
        }
        registerResources();

        // create the pool manager, ubo buffers, and global descriptor sets
        createDescriptorPoolAllocator();
        createUboBuffers();
        createGlobalDescriptorSet();

        // create the descriptor sets for the textures
        m_textureRegistry.setDescriptorAllocator(m_descriptorAllocator.get());
        m_textureRegistry.createDescriptorSet();

        // create the descriptor sets for the materials
        m_materialRegistry.setDescriptorAllocator(m_descriptorAllocator.get());
        m_materialRegistry.createDescriptorSets();
        // materials descriptor set will be updated every frame
        // in the master render system update method

        // create descriptor set for skybox
        if (m_scene.getEnvironment()->getSkybox()) {
            auto skybox = std::static_pointer_cast<VulkanSkybox>(m_scene.getEnvironment()->getSkybox());
            skybox->createDescriptorSet(*m_descriptorAllocator);
        }

        // create the render layer
        auto renderLayer =
            createUnique<RenderLayer>(m_context, m_renderer, *m_descriptorAllocator, m_textureRegistry,
                                      m_materialRegistry, m_blasRegistry, *m_globalSetLayout, m_scene.getEnvironment());

        // we store a non-owning pointer to the render layer for
        // later operations in the main loop
        m_renderLayerPtr = pushLayer<RenderLayer>(std::move(renderLayer));

        // same here for imGui
        m_uiRenderLayerPtr = pushOverlay<UiRenderLayer>(m_context, m_renderer.getSwapChainRenderPass());

        m_window.setEventCallback([this]<typename E>(E&& event) { onEvent(std::forward<E>(event)); });

        // here the event is owned by the queue, we just pass it by reference
        m_eventQueue.setMainCallbackFunction([this](core::Event& event) { onEvent(event); });
    }

    void Application::createDescriptorPoolAllocator() {
        // for now we have one ubo and a lot of textures
        std::vector<PoolSizeRatio> ratios = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.0f},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<float>(m_textureRegistry.getTextureCount())},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.0f},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.0f},
            {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 2.0f}};

        m_descriptorAllocator =
            createUnique<DescriptorAllocatorGrowable>(m_context, SwapChain::MAX_FRAMES_IN_FLIGHT, ratios);
    }

    void Application::createUboBuffers() {
        for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
            m_uboBuffers[i] =
                createUnique<VulkanBuffer>(m_context, sizeof(GlobalUbo), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            m_uboBuffers[i]->map();
        }
    }

    void Application::createGlobalDescriptorSet() {
        m_globalSetLayout =
            DescriptorSetLayout::Builder(m_context)
                .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                                VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
                .build();

        for (int i = 0; i < m_globalDescriptorSets.size(); i++) {
            auto bufferInfo = m_uboBuffers[i]->descriptorInfo();

            m_descriptorAllocator->allocate(m_globalSetLayout->getDescriptorSetLayout(), m_globalDescriptorSets[i]);

            DescriptorWriter(m_context, *m_globalSetLayout)
                .writeBuffer(0, &bufferInfo)
                .updateSet(m_globalDescriptorSets[i]);
        }
    }

    void Application::createDefaultResources() {
        // color are stored in RGBA format but bytes are reversed (Little-Endian Systems)
        // 0x0A0B0C0D -> Alpha = 0A, Blue = 0B, Green = 0C, Red = 0D
        std::unordered_map<std::string, std::pair<uint32_t, ImageFormat>> defaultImagesData = {
            {WHITE_PIXEL, {0xFFFFFFFF, RGBA8_SRGB}},
            {WHITE_PIXEL_LINEAR, {0xFFFFFFFF, RGBA8_LINEAR}},
            {GRAY_PIXEL_LINEAR, {0xFF808080, RGBA8_LINEAR}},
            {BLACK_PIXEL_LINEAR, {0xFF000000, RGBA8_LINEAR}},
            {NORMAL_PIXEL_LINEAR, {0xFFFF8080, RGBA8_LINEAR}}};

        for (const auto& [name, data] : defaultImagesData) {
            // Create a buffer with the pixel data
            uint32_t color = data.first;

            ImageInfo info;
            info.width = 1;
            info.height = 1;
            info.channels = 4;
            info.format = data.second;

            std::span<uint8_t> buffer(reinterpret_cast<uint8_t*>(&color), sizeof(color));

            // Buffer buffer = Buffer(&color, sizeof(color));
            Shared<Image> image = createShared<Texture2D>(m_context, info, buffer);
            m_resourceManagerPtr->add(image, name);
        }

        auto defaultMaterial = Material::Builder()
                                   .setAlbedoColor(glm::vec4(1.0f))
                                   .setAlbedoMap(m_resourceManagerPtr->get<Image>(WHITE_PIXEL))
                                   .setNormalMap(m_resourceManagerPtr->get<Image>(NORMAL_PIXEL_LINEAR))
                                   .setAmbientOcclusionMap(m_resourceManagerPtr->get<Image>(WHITE_PIXEL_LINEAR))
                                   .setMetallic(0.0f)
                                   .setRoughness(0.0f)
                                   //.setMetallicMap(m_resourceManager.get<Image>(BLACK_PIXEL_LINEAR))
                                   //.setRoughnessMap(m_resourceManager.get<Image>(GRAY_PIXEL_LINEAR))
                                   .setEmissiveMap(m_resourceManagerPtr->get<Image>(WHITE_PIXEL_LINEAR))
                                   .setTransmission(0.0f)
                                   .setIndexOfRefraction(1.3f)
                                   .build();

        ResourceManager::defaultMaterial = defaultMaterial;

        m_resourceManagerPtr->add(defaultMaterial, DEFAULT_MATERIAL);

        // Create blue noise texture resources
        ImageInfo blueNoiseInfo;
        blueNoiseInfo.width = BLUE_NOISE_TEXTURE_SIZE;
        blueNoiseInfo.height = BLUE_NOISE_TEXTURE_SIZE;
        blueNoiseInfo.channels = 4;
        blueNoiseInfo.format = RGBA32_LINEAR;
        blueNoiseInfo.filtering = ImageFiltering::Nearest;
        blueNoiseInfo.flags = ImageFlags::UnnormalizedCoordinates;

        std::string blueNoiseFile;

        for (uint32_t i = 0; i < BLUE_NOISE_TEXTURE_COUNT; i++) {
            blueNoiseFile = BLUE_NOISE_FILE + std::to_string(i) + BLUE_NOISE_FILE_EXT;
            m_resourceManagerPtr->get<Image>(blueNoiseFile, &blueNoiseInfo);
        }
    }

    void Application::registerResources() {
        // TODO: we will eventually redo all resource management, this sucks :)

        // iterate over resource and register images
        m_resourceManagerPtr->foreach ([&](const Shared<Resource>& resource) {
            if (resource->getType() == Resource::Type::Image) {
                const auto image = std::static_pointer_cast<Image>(resource);
                m_textureRegistry.add(image);
            } else if (resource->getType() == Resource::Type::Mesh) {
                auto mesh = std::static_pointer_cast<Mesh>(resource);
                m_blasRegistry.getOrCreateBLAS(mesh);
            }
        });

        m_resourceManagerPtr->foreach ([&](const Shared<Resource>& resource) {
            if (resource->getType() == Resource::Type::Material) {
                auto material = std::static_pointer_cast<Material>(resource);

                m_materialRegistry.add(material);
            }
        });
    }

    void Application::run() {
        if (m_viewProviderPtr == nullptr) {
            auto dummyViewProvider = DummyViewProvider();
            m_viewProviderPtr = &dummyViewProvider;
            PXT_WARN("Engine View Provider is set to NULL. Make sure to set a valid implementation of IViewProvider to "
                     "the Engine using the setViewProvider() API. Defaulting to DummyViewProvider now.");
        }

        auto currentTime = std::chrono::high_resolution_clock::now();

        // set the initial state of the engine to EDIT
        // this will not be hardcoded in the future
        queueEvent(core::EngineModeChangedEvent(core::EngineMode::EDIT));

        m_scene.onStart();

        uint32_t frameCount = 0;
        while (isRunning()) {
            // reset temporary inputs
            core::Input::getState().reset();
            // then poll events to update input state
            glfwPollEvents();
            m_eventQueue.pollEvents();

            auto newTime = std::chrono::high_resolution_clock::now();
            float elapsedTime = std::chrono::duration<float>(newTime - currentTime).count();
            currentTime = newTime;

            m_layerStack.onBeginFrame(elapsedTime);

            if (auto commandBuffer = m_renderer.beginFrame()) {
                int frameIndex = m_renderer.getFrameIndex();

                // if we are in PLAY/RUNTIME mode we want to run game scripts
                if (m_engineMode == core::EngineMode::PLAY || m_engineMode == core::EngineMode::RUNTIME) {
                    m_scene.onUpdate(elapsedTime);
                }

                float aspectRatio = m_renderLayerPtr->getSceneAspectRatio();
                CameraMatrices cameraMatrices = m_viewProviderPtr->getCameraMatrices(aspectRatio);

                FrameInfo frameInfo = {
                    frameIndex,
                    elapsedTime,
                    aspectRatio,
                    commandBuffer,
                    cameraMatrices,
                    m_globalDescriptorSets[frameIndex],
                    m_renderLayerPtr->getImGuiSceneDescriptorSet(),
                    m_scene,
                    m_renderer.getSwapChainCurrentFrameFence(),       // Frame fence
                    m_renderer.getSwapChainImageAvailableSemaphore(), // Wait semaphore
                    m_renderer.getSwapChainRenderFinishedSemaphore(m_renderer.getSwapChainCurrentImageIndex()),
                };

                GlobalUbo ubo{};
                ubo.ambientLightColor = m_scene.getEnvironment()->getAmbientLight();
                ubo.frameCount = frameCount++;

                m_layerStack.onUpdate(frameInfo, ubo);

                // write updated globalUbo
                m_uboBuffers[frameIndex]->writeToBuffer(&ubo);
                m_uboBuffers[frameIndex]->flush();

                m_renderLayerPtr->doRenderPasses(frameInfo);

                // IMGUI RENDER
                m_uiRenderLayerPtr->beginFrame(m_scene, m_renderer, frameInfo);
                m_layerStack.onUpdateUi(frameInfo);
                m_uiRenderLayerPtr->render(frameInfo, m_renderer);

                m_renderer.endFrame();

                m_layerStack.onPostFrameUpdate(frameInfo);
            }

            // tracy end frame mark
            FrameMark;
        }

        vkDeviceWaitIdle(m_context.getDevice());
    }

    bool Application::isRunning() { return !m_window.shouldClose() && m_running; }

    void Application::onEvent(core::Event& event) {
        core::EventDispatcher dispatcher(event);

        dispatcher.dispatch<core::WindowCloseEvent>([this](auto& event) {
            m_running = false;
            return true; // TODO: this should be false to allow other layers to handle close event
        });

        dispatcher.dispatch<core::WindowResizeEvent>([this](auto& event) {
            m_renderer.onWindowResize();
            return true;
        });

        dispatcher.dispatch<core::RequestEngineModeChangeEvent>([this](auto& event) {
            // TODO: here we should check if the engine is ready or can switch mode, if yes, we trigger the event
            queueEvent<core::EngineModeChangedEvent>(core::EngineModeChangedEvent(event.getNewEngineMode()));
            return false;
        });

        dispatcher.dispatch<core::EngineModeChangedEvent>([this](auto& event) {
            m_engineMode = event.getNewEngineMode();
            return false;
        });

        m_layerStack.onEvent(event);

        m_scene.onEvent(event);
    }
} // namespace pxt