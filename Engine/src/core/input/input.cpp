#include "core/input/input.hpp"

namespace pxt::core {
    bool Input::isKeyReleased(KeyCode key) { return Input::getState().isKeyReleased(key); }

    bool Input::isKeyPressed(KeyCode key) { return Input::getState().isKeyPressed(key); }

    bool Input::isKeyDown(KeyCode key) { return Input::getState().isKeyDown(key); }

    bool Input::isMouseButtonPressed(MouseButton button) { return Input::getState().isMouseButtonPressed(button); }

    bool Input::isMouseButtonReleased(MouseButton button) { return Input::getState().isMouseButtonReleased(button); }

    bool Input::isMouseButtonDown(MouseButton button) { return Input::getState().isMouseButtonDown(button); }

    glm::vec2 Input::getMousePosition() { return Input::getState().getMousePosition(); }

    glm::vec2 Input::getMouseDelta() { return Input::getState().getMouseDelta(); }
} // namespace pxt::core