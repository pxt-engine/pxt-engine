#include "core/input/input_state.hpp"

namespace pxt::core {

    void InputState::beginFrame() {
        // clear transient events
        keyPressed.fill(false);
        keyReleased.fill(false);

        mousePressed.fill(false);
        mouseReleased.fill(false);

        mouseDelta = { 0.f, 0.f };
        scrollDelta = { 0.f, 0.f };
        textInput.clear();
    }

    void InputState::reset() {
        // clear transient events
		beginFrame();

		keyDown.fill(false);
		mouseDown.fill(false);
    }

    // Keyboard
    void InputState::onKey(KeyCode key, bool down) {
        size_t i = (size_t)key;

        if (down)
        {
			onKeyPress(key);
        }
        else
        {
			onKeyRelease(key);
        }
    }

    void InputState::onKeyPress(KeyCode key) {
        size_t i = (size_t)key;
        if (!keyDown[i]) keyPressed[i] = true;
        keyDown[i] = true;
    }

    void InputState::onKeyRelease(KeyCode key) {
        size_t i = (size_t)key;
        if (keyDown[i]) keyReleased[i] = true;
        keyDown[i] = false;
    }

    void InputState::onKeyRepeat(KeyCode key) {
        size_t i = (size_t)key;
        // Repeat doesn’t change keyDown, but may trigger repeat events in game logic
        keyPressed[i] = true;
    }

    // Mouse
    void InputState::onMouseButton(MouseButton btn, bool down) {
        size_t i = (size_t)btn;

        if (down)
        {
			onMousePress(btn);
        }
        else
        {
            onMouseRelease(btn);
        }
    }

    void InputState::onMousePress(MouseButton button) {
        size_t i = (size_t)button;
        if (!mouseDown[i]) mousePressed[i] = true;
        mouseDown[i] = true;
    }

    void InputState::onMouseRelease(MouseButton button) {
        size_t i = (size_t)button;
        if (mouseDown[i]) mouseReleased[i] = true;
        mouseDown[i] = false;
    }

    void InputState::onMouseMove(double x, double y) {
        glm::vec2 newPos = { (float)x, (float)y };
        mouseDelta = newPos - mousePos;
        mousePos = newPos;
    }

    void InputState::onScroll(double xoff, double yoff) {
        scrollDelta.x += (float)xoff;
        scrollDelta.y += (float)yoff;
    }

    void InputState::onChar(uint32_t c) {
        textInput.push_back(c);
    }
}