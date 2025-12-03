#pragma once

#include "core/pch.hpp"

#include "application.hpp"
#include "core/input/key_code.hpp"
#include "core/input/input_state.hpp"
#include "core/input/mapper/glfw_input_mapper.hpp"

namespace pxt::core {

    /**
     * @class Input
     * @brief Class for handling input events.
     * 
     * The Input class provides static methods for querying input events such as key presses, 
     * mouse button presses, and mouse movement.
     */
    class Input {
    public:
        static InputState& getState() {
            static InputState s_inputState;
            return s_inputState;
        }

        /**
         * @brief Checks if a key is released.
         * 
         * @param key The key to check.
         * @return True if the key is released, false otherwise.
         */
        static bool isKeyReleased(KeyCode key);
        
        /**
         * @brief Checks if a key is currently pressed.
         * 
         * @param key The key to check.
         * @return True if the key is currently pressed, false otherwise.
         */
        static bool isKeyPressed(KeyCode key);

        /**
         * @brief Checks if a key is being repeated (held down).
         * 
         * @param key The key to check.
         * @return True if the key is being repeated, false otherwise.
         */
        static bool isKeyDown(KeyCode key);

        /**
         * @brief Checks if a mouse button is pressed.
         * 
         * @param button The mouse button to check.
         * @return True if the mouse button is pressed, false otherwise.
         */
        static bool isMouseButtonPressed(MouseButton button);

		/**
		 * @brief Checks if a mouse button is released.
		 *
		 * @param button The mouse button to check.
		 * @return True if the mouse button is released, false otherwise.
		 */
		static bool isMouseButtonReleased(MouseButton button);

		/**
		 * @brief Checks if a mouse button is being held down.
		 *
		 * @param button The mouse button to check.
		 * @return True if the mouse button is being held down, false otherwise.
		 */
		static bool isMouseButtonDown(MouseButton button);

        /**
            * @brief Gets the current mouse position.
            *
            * @return The current mouse position.
            */
        static glm::vec2 getMousePosition();

		/**
		 * @brief Gets the mouse movement delta since the last frame.
		 *
		 * @return The mouse movement delta.
		 */
		static glm::vec2 getMouseDelta();

    private:
        static GLFWwindow* getWindow() {
            return Application::get().getWindow().getBaseWindow();
        }
    };
}