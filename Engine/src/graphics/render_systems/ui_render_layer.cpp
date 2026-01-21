#include "graphics/render_systems/ui_render_layer.hpp"
#include "application.hpp"
#include "core/events/event_dispatcher.hpp"
#include "core/events/keyboard_event.hpp"
#include "core/events/mouse_event.hpp"
#include "scene/scene_serializer.hpp"

#include <ImGuizmo.h>

namespace pxt {

    UiRenderLayer::UiRenderLayer(Context& context, VkRenderPass renderPass)
        : Layer("UiRenderLayer"),

          m_context(context) {
        initImGui(renderPass);
    }

    UiRenderLayer::~UiRenderLayer() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void UiRenderLayer::initImGui(VkRenderPass& renderPass) {
        // ImGui will use the same descriptor set for all textures.
        // ImGui will use this pool for fonts and its stuff, textures will be allocated from the descriptor allocator
        // growable
        // TODO: maybe set format will change
        m_imGuiPool = DescriptorPool::Builder(m_context)
                          .setMaxSets(10)
                          .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10)
                          .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
                          .build();

        IMGUI_CHECKVERSION();
        ImGuiContext* imguiCtx = ImGui::CreateContext();
        ImGuizmo::SetImGuiContext(imguiCtx);
        ImGui::StyleColorsDark();

        setImGuiStyle();

        // enable docking and load ini file
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        io.IniFilename = IMGUI_INI_FILEPATH.c_str();
        PXT_INFO("ImGui .ini file set to: {}", io.IniFilename);

        // here we pass false to not install callbacks, as we want to handle events ourselves
        ImGui_ImplGlfw_InitForVulkan(m_context.getWindow().getBaseWindow(), false);
        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.Instance = m_context.getInstance();
        initInfo.PhysicalDevice = m_context.getPhysicalDevice();
        initInfo.Device = m_context.getDevice();
        initInfo.QueueFamily = m_context.findPhysicalQueueFamilies().graphicsFamily;
        initInfo.Queue = m_context.getGraphicsQueue();
        initInfo.PipelineInfoMain.RenderPass = renderPass;
        initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
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

        // ImGui_ImplVulkan_CreateFontsTexture();
    }

    void UiRenderLayer::setImGuiStyle() {
        ImGuiStyle& style = ImGui::GetStyle();
        ImGuiIO& io = ImGui::GetIO();

        ImFont* font = io.Fonts->AddFontFromFileTTF((FONTS_PATH + "Roboto-VariableFont_wdth,wght.ttf").c_str(), 16.5f);

        io.FontDefault = font;

        // Dark Color Scheme
        style.Colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.015f, 0.015f, 0.015f, 0.94f);
        style.Colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        style.Colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
        style.Colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
        style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.36f, 0.09f, 0.48f, 0.54f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.569f, 0.325f, 0.859f, 0.40f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.569f, 0.325f, 0.859f, 0.67f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.025f, 0.025f, 0.025f, 1.00f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.36f, 0.09f, 0.48f, 1.00f);
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
        style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.03f, 0.03f, 0.03f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
        style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
        style.Colors[ImGuiCol_CheckMark] = ImVec4(0.569f, 0.325f, 0.859f, 1.00f);
        style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.54f, 0.22f, 0.88f, 1.00f);
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.569f, 0.325f, 0.859f, 1.00f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.569f, 0.325f, 0.859f, 0.40f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.569f, 0.325f, 0.859f, 1.00f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.55f, 0.23f, 0.88f, 1.00f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.569f, 0.325f, 0.859f, 0.31f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.569f, 0.325f, 0.859f, 0.80f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.569f, 0.325f, 0.859f, 1.00f);
        style.Colors[ImGuiCol_Separator] = style.Colors[ImGuiCol_Border];
        style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
        style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
        style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.569f, 0.325f, 0.859f, 0.20f);
        style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.569f, 0.325f, 0.859f, 0.67f);
        style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.569f, 0.325f, 0.859f, 0.95f);
        style.Colors[ImGuiCol_TabHovered] = style.Colors[ImGuiCol_HeaderHovered];
        style.Colors[ImGuiCol_Tab] = ImLerp(style.Colors[ImGuiCol_Header], style.Colors[ImGuiCol_TitleBgActive], 0.80f);
        style.Colors[ImGuiCol_TabSelected] =
            ImLerp(style.Colors[ImGuiCol_HeaderActive], style.Colors[ImGuiCol_TitleBgActive], 0.60f);
        style.Colors[ImGuiCol_TabSelectedOverline] = style.Colors[ImGuiCol_HeaderActive];
        style.Colors[ImGuiCol_TabDimmed] = ImLerp(style.Colors[ImGuiCol_Tab], style.Colors[ImGuiCol_TitleBg], 0.80f);
        style.Colors[ImGuiCol_TabDimmedSelected] =
            ImLerp(style.Colors[ImGuiCol_TabSelected], style.Colors[ImGuiCol_TitleBg], 0.40f);
        style.Colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        style.Colors[ImGuiCol_DockingPreview] = style.Colors[ImGuiCol_HeaderActive] * ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
        style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        style.Colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
        style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.0f);
        style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.0f);
        style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
        style.Colors[ImGuiCol_TextLink] = style.Colors[ImGuiCol_HeaderActive];
        style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.569f, 0.325f, 0.859f, 0.35f);
        style.Colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        style.Colors[ImGuiCol_NavCursor] = ImVec4(0.569f, 0.325f, 0.859f, 1.00f);
        style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

        // Global alpha
        style.Alpha = 1.0f;          // Global alpha (0.0-1.0)
        style.DisabledAlpha = 0.60f; // Additional alpha for disabled items

        // Window geometry
        style.WindowPadding = ImVec2(8, 8);          // Padding within window
        style.WindowRounding = 7.0f;                 // Radius of window corners
        style.WindowBorderSize = 1.0f;               // Thickness of border
        style.WindowMinSize = ImVec2(32, 32);        // Minimum window size
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f); // Title bar alignment (0.0=left, 0.5=center, 1.0=right)

        // Window menu bar
        style.WindowMenuButtonPosition = ImGuiDir_Right;

        // Child windows
        style.ChildRounding = 2.0f;   // Radius of child window corners
        style.ChildBorderSize = 1.0f; // Thickness of child window border

        // Popups
        style.PopupRounding = 5.0f;   // Radius of popup corners
        style.PopupBorderSize = 1.0f; // Thickness of popup border

        // Frames (used by most widgets)
        style.FramePadding = ImVec2(5, 4); // Padding within framed rectangle
        style.FrameRounding = 5.0f;        // Radius of frame corners
        style.FrameBorderSize = 0.0f;      // Thickness of frame border

        // Items
        style.ItemSpacing = ImVec2(8, 8);      // Horizontal/vertical spacing between widgets
        style.ItemInnerSpacing = ImVec2(4, 4); // Horizontal/vertical spacing within elements

        // Layout
        style.IndentSpacing = 21.0f;    // Horizontal indentation for tree nodes
        style.ColumnsMinSpacing = 6.0f; // Minimum spacing between columns
        style.ScrollbarSize = 9.0f;     // Width of scrollbars
        style.ScrollbarRounding = 9.0f; // Radius of scrollbar corners
        style.GrabMinSize = 12.0f;      // Minimum size of grab boxes
        style.GrabRounding = 0.0f;      // Radius of grabs

        // Tabs
        style.TabRounding = 8.0f;        // Radius of tab corners
        style.TabBorderSize = 0.0f;      // Thickness of tab border
        style.TabBarBorderSize = 1.0f;   // Thickness of tab bar border
        style.TabBarOverlineSize = 0.0f; // Thickness of tab overline

        // Alignment
        style.ButtonTextAlign = ImVec2(0.5f, 0.5f);     // Button text alignment
        style.SelectableTextAlign = ImVec2(0.0f, 0.0f); // Selectable text alignment

        // Rendering
        style.AntiAliasedLines = true;            // Enable anti-aliasing on lines
        style.AntiAliasedLinesUseTex = true;      // Use texture for AA lines (faster)
        style.AntiAliasedFill = true;             // Enable anti-aliasing on filled shapes
        style.CurveTessellationTol = 1.25f;       // Curve tessellation tolerance
        style.CircleTessellationMaxError = 0.30f; // Maximum error for circle approximatio
    }

    void UiRenderLayer::render(FrameInfo& frameInfo, Renderer& renderer) {
        // TODO: move the ui functions of "buildUi" into "onUpdateUi" of other layers
        buildUi(frameInfo.scene);

        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frameInfo.commandBuffer);

        renderer.endSwapChainRenderPass(frameInfo.commandBuffer);
    }

    void UiRenderLayer::onEvent(core::Event& event) {
        // here we can add custom events (not glfw) like appRender, etc.
        // normal inputs are handled with glfw callbacks (see window.cpp)
    }

    void UiRenderLayer::beginFrame(Scene& scene, Renderer& renderer, FrameInfo& frameInfo) {
        renderer.beginSwapChainRenderPass(frameInfo.commandBuffer);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // IMPORTANT: This is required for docking to work in the main window (for customizations, view imgui_demo.cpp)
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

        // initialize gizmos for this frame
        ImGuizmo::BeginFrame();
    }

    void UiRenderLayer::buildUi(Scene& scene) {
        ImGui::ShowMetricsWindow();
        // ImGui::ShowDemoWindow();
    }
} // namespace pxt