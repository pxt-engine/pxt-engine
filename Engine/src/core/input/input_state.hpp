#pragma once

#include "core/input/key_code.hpp"
#include "core/pch.hpp"

namespace pxt::core {
    class InputState {
    public:
        // Keyboard
        std::array<bool, (size_t)KeyCode::COUNT> keyDown{};
        std::array<bool, (size_t)KeyCode::COUNT> keyPressed{};
        std::array<bool, (size_t)KeyCode::COUNT> keyReleased{};

        // Mouse
        std::array<bool, (size_t)MouseButton::COUNT> mouseDown{};
        std::array<bool, (size_t)MouseButton::COUNT> mousePressed{};
        std::array<bool, (size_t)MouseButton::COUNT> mouseReleased{};

        glm::vec2 mousePos{0.f, 0.f};
        glm::vec2 mouseDelta{0.f, 0.f};
        glm::vec2 scrollDelta{0.f, 0.f};

        // Text Input
        std::vector<uint32_t> textInput; // UTF-32 characters

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

        bool isKeyDown(KeyCode key) const { return keyDown[(size_t)key]; }

        bool isKeyPressed(KeyCode key) const { return keyPressed[(size_t)key]; }

        bool isKeyReleased(KeyCode key) const { return keyReleased[(size_t)key]; }

        bool isMouseButtonDown(MouseButton button) const { return mouseDown[(size_t)button]; }

        bool isMouseButtonPressed(MouseButton button) const { return mousePressed[(size_t)button]; }

        bool isMouseButtonReleased(MouseButton button) const { return mouseReleased[(size_t)button]; }

        glm::vec2 getMousePosition() const { return mousePos; }

        glm::vec2 getMousePositionImGui() const {
            ImVec2 imguiMousePos = ImGui::GetMousePos();
            return glm::vec2(imguiMousePos.x, imguiMousePos.y);
        }

        glm::vec2 getMouseDelta() const { return mouseDelta; }

        glm::vec2 getScrollDelta() const { return scrollDelta; }
    };
} // namespace pxt::core
