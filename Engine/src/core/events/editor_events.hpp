#pragma once

#include "core/pch.hpp"
#include "core/events/event.hpp"

namespace pxt::core {
    class PickObjectAtEvent : public Event {
    public:
        PickObjectAtEvent(uint32_t px, uint32_t py)
            : m_pixelX(px), m_pixelY(py) {
        }

        [[nodiscard]] uint32_t getPixelX() const { return m_pixelX; }
        [[nodiscard]] uint32_t getPixelY() const { return m_pixelY; }

        Event::Type getEventType() const override { return Event::Type::PickObjectAt; }
        std::string getName() const override { return "PickObjectAt"; }

        static Event::Type getStaticType() { return Event::Type::PickObjectAt; }

    private:
        uint32_t m_pixelX, m_pixelY;
    };
}