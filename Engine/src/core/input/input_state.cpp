#include "core/input/input_state.hpp"

namespace pxt::core {

    void InputState::reset() {
        // clear transient events
        m_keyPressed.fill(false);
        m_keyReleased.fill(false);

        m_mousePressed.fill(false);
        m_mouseReleased.fill(false);

        m_mouseDelta = {0.f, 0.f};
        m_scrollDelta = {0.f, 0.f};
        m_textInput.clear();
    }

    // Keyboard
    void InputState::onKey(KeyCode key, bool down) {
        size_t i = (size_t)key;

        if (down) {
            onKeyPress(key);
        } else {
            onKeyRelease(key);
        }
    }

    void InputState::onKeyPress(KeyCode key) {
        size_t i = (size_t)key;
        if (!m_keyDown[i])
            m_keyPressed[i] = true;
        m_keyDown[i] = true;
    }

    void InputState::onKeyRelease(KeyCode key) {
        size_t i = (size_t)key;
        if (m_keyDown[i])
            m_keyReleased[i] = true;
        m_keyDown[i] = false;
    }

    void InputState::onKeyRepeat(KeyCode key) {
        size_t i = (size_t)key;
        // Repeat doesn’t change m_keyDown, but may trigger repeat events in game logic
        m_keyPressed[i] = true;
    }

    // Mouse
    void InputState::onMouseButton(MouseButton btn, bool down) {
        size_t i = (size_t)btn;

        if (down) {
            onMousePress(btn);
        } else {
            onMouseRelease(btn);
        }
    }

    void InputState::onMousePress(MouseButton button) {
        size_t i = (size_t)button;
        if (!m_mouseDown[i])
            m_mousePressed[i] = true;
        m_mouseDown[i] = true;
    }

    void InputState::onMouseRelease(MouseButton button) {
        size_t i = (size_t)button;
        if (m_mouseDown[i])
            m_mouseReleased[i] = true;
        m_mouseDown[i] = false;
    }

    void InputState::onMouseMove(double x, double y) {
        glm::vec2 newPos = {(float)x, (float)y};
        m_mouseDelta = newPos - m_mousePos;
        m_mousePos = newPos;
    }

    void InputState::onScroll(double xoff, double yoff) {
        m_scrollDelta.x += (float)xoff;
        m_scrollDelta.y += (float)yoff;
    }

    void InputState::onChar(uint32_t c) { m_textInput.push_back(c); }
} // namespace pxt::core