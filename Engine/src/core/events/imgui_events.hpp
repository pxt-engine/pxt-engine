#pragma once

#include "core/events/event.hpp"
#include "core/pch.hpp"

namespace pxt::core {

    class ImGuiViewportResizeEvent : public Event {
    public:
        ImGuiViewportResizeEvent(uint32_t width, uint32_t height) : m_width(width), m_height(height) {}

        [[nodiscard]] uint32_t getWidth() const { return m_width; }

        [[nodiscard]] uint32_t getHeight() const { return m_height; }

        [[nodiscard]] Event::Type getEventType() const override { return Event::Type::ImGuiViewportResize; }

        [[nodiscard]] std::string getName() const override { return "ImGuiViewportResize"; }

        static Event::Type getStaticType() { return Event::Type::ImGuiViewportResize; }

    private:
        uint32_t m_width, m_height;
    };
} // namespace pxt::core