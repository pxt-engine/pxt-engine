#pragma once

#include "core/pch.hpp"
#include "core/events/event.hpp"

namespace pxt::core {

    class ImGuiViewportResizeEvent : public Event {
    public:
        ImGuiViewportResizeEvent(uint32_t width, uint32_t height)
            : m_width(width), m_height(height) {
        }

        uint32_t getWidth() const { return m_width; }
        uint32_t getHeight() const { return m_height; }

        Event::Type getEventType() const override { return Event::Type::ImGuiViewportResize; }
        std::string getName() const override { return "ImGuiViewportResize"; }

        static Event::Type getStaticType() { return Event::Type::ImGuiViewportResize; }

    private:
        uint32_t m_width, m_height;
    };
}