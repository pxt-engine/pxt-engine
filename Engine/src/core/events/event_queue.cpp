#include "core/events/event_queue.hpp"

namespace pxt::core {
    void EventQueue::setMainCallbackFunction(AppCallbackFunction callbackFunction) {
        m_mainCallbackFunction = callbackFunction;
    }

    void EventQueue::pollEvents() {
        while (!m_queuedEvents.empty()) {
            processOldestEvent();
        }
    }

    void EventQueue::processOldestEvent() {
        Unique<Event> event = std::move(m_queuedEvents.front());
        m_queuedEvents.pop();
        if (m_mainCallbackFunction) {
            m_mainCallbackFunction(*event);
        }
    }
} // namespace pxt::core