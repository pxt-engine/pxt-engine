#pragma once

#include "core/pch.hpp"
#include "core/events/event.hpp"

namespace pxt::core {
	class EventQueue {
	public:
		using AppCallbackFunction = std::function<void(Event&)>;

		void setMainCallbackFunction(AppCallbackFunction callbackFunction);
		void pollEvents();
		
		template<typename E>
		requires (std::is_base_of_v<core::Event, std::decay_t<E>>)
		void queueEvent(E&& event) {
			m_queuedEvents.emplace(
				createUnique<std::decay_t<E>>(std::forward<E>(event))
			);
		}

	private:
		void processOldestEvent();

		std::queue<Unique<Event>> m_queuedEvents{};
		AppCallbackFunction m_mainCallbackFunction = nullptr;
	};
}