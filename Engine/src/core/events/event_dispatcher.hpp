#pragma once

#include "core/pch.hpp"

#include "core/events/event.hpp"

namespace pxt::core {

    /**
     * @class EventDispatcher
     * @brief A utility class for dispatching events to appropriate handlers.
     *
     * The EventDispatcher class is used to dispatch events to their corresponding
     * event handlers based on the event type. It holds a reference to an Event object
     * and provides a method to dispatch the event if it matches a specific type.
     */
    class EventDispatcher {

        // Type alias for event handling functions
        // The return type is bool to indicate if the event was handled successfully
        template <typename E>
        requires(std::is_base_of_v<core::Event, E>)
        using EventFunction = std::function<bool(E&)>;

    public:
        /**
         * @brief Constructs an EventDispatcher with the given event.
         * @param event The event to be dispatched.
         */
        explicit EventDispatcher(Event& event) : m_event(event) {}

        /**
         * @brief Dispatches the event if it matches the specified type.
         *
         * This method checks if the event type matches the specified type E.
         * If it does, it calls the provided function and marks the event as handled
         * if the function returns true.
         *
         * @tparam E The type of the event to dispatch.
         * @param eventFunction The function to call if the event type matches.
         * @return true if the event was dispatched; false otherwise.
         */
        template <typename E>
        requires(std::is_base_of_v<core::Event, E>)
        bool dispatch(EventFunction<E> eventFunction) {
            // Ensure the event type matches and hasn't been handled yet
            if (m_event.getEventType() == E::getStaticType() && !m_event.isHandled()) {

                // Cast the event to the correct type and invoke the callback function
                if (eventFunction(static_cast<E&>(m_event))) {
                    m_event.markHandled();
                }
                return true;
            }

            // Event type does not match or has already been handled
            return false;
        }

    private:
        Event& m_event;
    };

} // namespace pxt::core
