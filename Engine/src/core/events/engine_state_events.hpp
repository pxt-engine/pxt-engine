#pragma once

#include "core/events/event.hpp"
#include "core/engine_mode.hpp"
#include "core/pch.hpp"

namespace pxt::core {
    class EngineModeChangedEvent : public Event {
    public:
        EngineModeChangedEvent(EngineMode newEngineMode) : m_engineMode(newEngineMode) {}

        [[nodiscard]] EngineMode getNewEngineMode() const { return m_engineMode; }

        [[nodiscard]] Event::Type getEventType() const override { return Event::Type::EngineModeChanged; }

        [[nodiscard]] std::string getName() const override { return "EngineModeChanged"; }

        static Event::Type getStaticType() { return Event::Type::EngineModeChanged; }

    private:
        EngineMode m_engineMode;
    };

    class RequestEngineModeChangeEvent : public Event {
    public:
        RequestEngineModeChangeEvent(EngineMode newEngineMode) : m_engineMode(newEngineMode) {}

        [[nodiscard]] EngineMode getNewEngineMode() const { return m_engineMode; }

        [[nodiscard]] Event::Type getEventType() const override { return Event::Type::RequestEngineModeChange; }

        [[nodiscard]] std::string getName() const override { return "RequestEngineModeChange"; }

        static Event::Type getStaticType() { return Event::Type::RequestEngineModeChange; }

    private:
        EngineMode m_engineMode;
    };
}