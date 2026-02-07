#include "graphics/window.hpp"

#include "core/events/event.hpp"
#include "core/events/keyboard_event.hpp"
#include "core/events/mouse_event.hpp"
#include "core/events/window_event.hpp"
#include "core/input/input.hpp"
#include "core/input/mapper/glfw_input_mapper.hpp"

#include <ImGuizmo.h>

#include <stb_image.h>

namespace pxt {

    Window::Window(const WindowData& props) : m_data(props) {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        m_window = glfwCreateWindow(props.width, props.height, props.title.c_str(), nullptr, nullptr);

        glfwSetWindowUserPointer(m_window, &m_data);

        registerCallbacks();
    }

    Window::~Window() {
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

    void Window::loadWindowIcon(const std::string& iconPath) {
        GLFWimage images[1];
        images[0].pixels = stbi_load(iconPath.c_str(), &images[0].width, &images[0].height, 0, 4); // rgba channels
        glfwSetWindowIcon(m_window, 1, images);
        stbi_image_free(images[0].pixels);
    }

    void Window::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface) {
        if (glfwCreateWindowSurface(instance, m_window, nullptr, surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    void Window::registerCallbacks() {

        glfwSetWindowCloseCallback(m_window, [](GLFWwindow* window) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            core::WindowCloseEvent event;
            data.eventCallback(event);
        });

        glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int width, int height) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            data.width = static_cast<uint32_t>(width);
            data.height = static_cast<uint32_t>(height);
            data.frameBufferResized = true;

            core::WindowResizeEvent event(width, height);
            data.eventCallback(event);
        });

        glfwSetKeyCallback(m_window, [](GLFWwindow* window, int glfwKey, int scancode, int action, int mods) {
            ImGuiIO& io = ImGui::GetIO();
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            // forward to ImGui backend
            ImGui_ImplGlfw_KeyCallback(window, glfwKey, scancode, action, mods);

            // let ImGui handle it if it wants (only press and repeat events)
            bool imguiBlocksInput = io.WantCaptureKeyboard || ImGuizmo::IsUsingAny();

            core::KeyCode key = core::mapGLFWKey(glfwKey);

            switch (action) {
            case GLFW_PRESS: {
                if (imguiBlocksInput)
                    return;

                core::Input::getState().onKeyPress(key);
                core::KeyPressEvent event(key);
                data.eventCallback(event);
                break;
            }
            case GLFW_RELEASE: {
                // we always proecess key release events since
                // imgui could eat them and bug the input state
                core::Input::getState().onKeyRelease(key);
                core::KeyReleaseEvent event(key);
                data.eventCallback(event);
                break;
            }
            case GLFW_REPEAT: {
                if (imguiBlocksInput)
                    return;

                core::Input::getState().onKeyRepeat(key);
                core::KeyPressEvent event(key, 1);
                data.eventCallback(event);
                break;
            }
            }
        });

        glfwSetCharCallback(m_window, [](GLFWwindow* window, unsigned int glfwKey) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            ImGuiIO& io = ImGui::GetIO();

            ImGui_ImplGlfw_CharCallback(window, glfwKey);

            if (io.WantTextInput)
                return;

            core::KeyCode key = core::mapGLFWKey(glfwKey);

            core::Input::getState().onChar(glfwKey);

            core::KeyDownEvent event(key);
            data.eventCallback(event);
        });

        glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int glfwButton, int action, int mods) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            ImGui_ImplGlfw_MouseButtonCallback(window, glfwButton, action, mods);

            core::MouseButton button = core::mapGLFWMouseButton(glfwButton);

            // we block the mouse left button if ImGuizmo is using or hovering, because that is what ImGuizmo uses in
            // his gizmos
            bool imguiBlocksInput = (ImGuizmo::IsUsing() || ImGuizmo::IsOver()) && button == core::MouseButton::Button0;

            switch (action) {
            case GLFW_PRESS: {
                if (imguiBlocksInput)
                    return;

                core::Input::getState().onMousePress(button);
                core::MouseButtonPressEvent event(button);
                data.eventCallback(event);
                break;
            }
            case GLFW_RELEASE: {
                core::Input::getState().onMouseRelease(button);
                core::MouseButtonReleaseEvent event(button);
                data.eventCallback(event);
                break;
            }
            }
        });

        glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xOffset, double yOffset) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            ImGui_ImplGlfw_ScrollCallback(window, xOffset, yOffset);

            if (ImGuizmo::IsUsingAny())
                return;

            core::Input::getState().onScroll(xOffset, yOffset);
            core::MouseScrollEvent event(xOffset, yOffset);
            data.eventCallback(event);
        });

        glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xPos, double yPos) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            ImGui_ImplGlfw_CursorPosCallback(window, xPos, yPos);

            // prevent mouse movement events when using gizmos
            if (ImGuizmo::IsUsingAny())
                return;

            // PXT_INFO("Viewport Hovered: {}, Viewport Focused: {}", ui::s_isViewportHovered, ui::s_isViewportFocused);

            core::Input::getState().onMouseMove(xPos, yPos);
            core::MouseMoveEvent event(xPos, yPos);
            data.eventCallback(event);
        });
    }
} // namespace pxt