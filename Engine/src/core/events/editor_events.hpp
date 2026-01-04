#pragma once

#include "core/events/event.hpp"
#include "core/pch.hpp"

namespace pxt::core {
    class PickObjectAtEvent : public Event {
    public:
        PickObjectAtEvent(uint32_t px, uint32_t py) : m_pixelX(px), m_pixelY(py) {}

        [[nodiscard]] uint32_t getPixelX() const { return m_pixelX; }

        [[nodiscard]] uint32_t getPixelY() const { return m_pixelY; }

        Event::Type getEventType() const override { return Event::Type::PickObjectAt; }

        std::string getName() const override { return "PickObjectAt"; }

        static Event::Type getStaticType() { return Event::Type::PickObjectAt; }

    private:
        uint32_t m_pixelX, m_pixelY;
    };

    class SelectedEntityChangedEvent : public Event {
    public:
        SelectedEntityChangedEvent(UUID selectedEntityUUID) : m_selectedEntityUUID(selectedEntityUUID) {}

        [[nodiscard]] UUID getSelectedEntityUUID() const { return m_selectedEntityUUID; }

        Event::Type getEventType() const override { return Event::Type::SelectedEntityChanged; }

        std::string getName() const override { return "SelectedEntityChanged"; }

        static Event::Type getStaticType() { return Event::Type::SelectedEntityChanged; }

    private:
        UUID m_selectedEntityUUID;
    };

    class ViewportResizeEvent : public Event {
    public:
        ViewportResizeEvent(uint32_t width, uint32_t height) : m_width(width), m_height(height) {}

        [[nodiscard]] uint32_t getWidth() const { return m_width; }

        [[nodiscard]] uint32_t getHeight() const { return m_height; }

        [[nodiscard]] Event::Type getEventType() const override { return Event::Type::ImGuiViewportResize; }

        [[nodiscard]] std::string getName() const override { return "ImGuiViewportResize"; }

        static Event::Type getStaticType() { return Event::Type::ImGuiViewportResize; }

    private:
        uint32_t m_width, m_height;
    };
} // namespace pxt::core