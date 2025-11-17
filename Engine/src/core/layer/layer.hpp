#pragma once
#include "core/pch.hpp"
#include "core/events/event.hpp"
#include "graphics/frame_info.hpp"

namespace PXTEngine {
	class Layer {
	public:
		Layer(const std::string& name = "Unnamed-Layer");
		virtual ~Layer() = default;

		virtual void onAttach() {}
		virtual void onDetach() {}
		virtual void onUpdate(FrameInfo& frameInfo, GlobalUbo& ubo) {}
		virtual void onPostFrameUpdate(FrameInfo& frameInfo) {}
		virtual void onUpdateUi(FrameInfo& frameInfo) {}
		virtual void onEvent(Event& event) {}
		
		[[nodiscard]] const std::string& getName() const { return m_name; }
	private:
		std::string m_name;
	};
}