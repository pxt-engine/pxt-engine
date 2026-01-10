#pragma once

#include "core/input/key_code.hpp"
#include "core/pch.hpp"

namespace pxt::core {
    class InputState {
    public:
        // Frame Update
        void beginFrame();
        void reset();

        // Keyboard events
        void onKey(KeyCode key, bool down);
        void onKeyPress(KeyCode key);
        void onKeyRelease(KeyCode key);
        void onKeyRepeat(KeyCode key);

        // Mouse events
        void onMouseButton(MouseButton btn, bool down);
        void onMousePress(MouseButton button);
        void onMouseRelease(MouseButton button);
        void onMouseMove(double x, double y);
        void onScroll(double xoff, double yoff);

        void onChar(uint32_t charCode);

        bool isKeyDown(KeyCode key) const { return m_keyDown[(size_t)key]; }

        bool isKeyPressed(KeyCode key) const { return m_keyPressed[(size_t)key]; }

        bool isKeyReleased(KeyCode key) const { return m_keyReleased[(size_t)key]; }

        bool isMouseButtonDown(MouseButton button) const { return m_mouseDown[(size_t)button]; }

        bool isMouseButtonPressed(MouseButton button) const { return m_mousePressed[(size_t)button]; }

        bool isMouseButtonReleased(MouseButton button) const { return m_mouseReleased[(size_t)button]; }

        glm::vec2 getMousePosition() const { return m_mousePos; }

        glm::vec2 getMousePositionImGui() const {
            ImVec2 imguiMousePos = ImGui::GetMousePos();
            return glm::vec2(imguiMousePos.x, imguiMousePos.y);
        }

        glm::vec2 getMouseDelta() const { return m_mouseDelta; }

        glm::vec2 getScrollDelta() const { return m_scrollDelta; }

    private:
        // Keyboard
        std::array<bool, (size_t)KeyCode::COUNT> m_keyDown{};
        std::array<bool, (size_t)KeyCode::COUNT> m_keyPressed{};
        std::array<bool, (size_t)KeyCode::COUNT> m_keyReleased{};

        // Mouse
        std::array<bool, (size_t)MouseButton::COUNT> m_mouseDown{};
        std::array<bool, (size_t)MouseButton::COUNT> m_mousePressed{};
        std::array<bool, (size_t)MouseButton::COUNT> m_mouseReleased{};

        glm::vec2 m_mousePos{0.f, 0.f};
        glm::vec2 m_mouseDelta{0.f, 0.f};
        glm::vec2 m_scrollDelta{0.f, 0.f};

        // Text Input
        std::vector<uint32_t> m_textInput; // UTF-32 characters

        bool m_isViewportFocused = false;
        bool m_isViewportHovered = false;

        // we need this to block certain events when interacting with UI
        bool m_isCursorOverUI = false;
    };
} // namespace pxt::core
