#include "core/input/input.hpp"

namespace pxt::core {
    bool Input::isKeyReleased(KeyCode key) { return getState().isKeyReleased(key); }

    bool Input::isKeyPressed(KeyCode key) { return getState().isKeyPressed(key); }

    bool Input::isKeyDown(KeyCode key) { return getState().isKeyDown(key); }

    bool Input::isMouseButtonPressed(MouseButton button) { return getState().isMouseButtonPressed(button); }

    bool Input::isMouseButtonReleased(MouseButton button) { return getState().isMouseButtonReleased(button); }

    bool Input::isMouseButtonDown(MouseButton button) { return getState().isMouseButtonDown(button); }

    glm::vec2 Input::getMousePosition() { return getState().getMousePosition(); }

    glm::vec2 Input::getMouseDelta() { return getState().getMouseDelta(); }

    bool Input::isViewportFocused() { return getState().isViewportFocused; }

    bool Input::isViewportHovered() { return getState().isViewportHovered; }

    bool Input::isCursorOverUI() { return getState().isCursorOverUI; }
} // namespace pxt::core