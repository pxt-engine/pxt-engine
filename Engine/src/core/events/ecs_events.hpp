#pragma once

#include "core/events/event.hpp"
#include "core/pch.hpp"

namespace pxt::core {
    class EntityDestroyedEvent : public Event {
    public:
        EntityDestroyedEvent(UID destroyedEntityUID) : m_destroyedEntityUID(destroyedEntityUID) {}

        [[nodiscard]] UID getDestroyedEntityUID() const { return m_destroyedEntityUID; }

        Event::Type getEventType() const override { return Event::Type::EntityDestroyed; }

        std::string getName() const override { return "EntityDestroyed"; }

        static Event::Type getStaticType() { return Event::Type::EntityDestroyed; }

    private:
        UID m_destroyedEntityUID;
    };
}