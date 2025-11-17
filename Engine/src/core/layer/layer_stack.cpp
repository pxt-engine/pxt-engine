#include "core/layer/layer_stack.hpp"

namespace PXTEngine {
	// TODO: maybe orderer destruction of layers here? see later
	LayerStack::~LayerStack() {
		m_layers.clear();
	}

	// we propagate the event from top to bottom
	void LayerStack::onEvent(Event& event) {
		for (auto it = m_layers.rbegin(); it != m_layers.rend(); ++it) {
			if (event.isHandled())
				break;
			(*it)->onEvent(event);
		}
	}

	void LayerStack::onUpdate(FrameInfo& frameInfo, GlobalUbo& ubo) {
		for (auto& layer : m_layers) {
			layer->onUpdate(frameInfo, ubo);
		}
	}

	void LayerStack::onPostFrameUpdate(FrameInfo& frameInfo) {
		for (auto& layer : m_layers) {
			layer->onPostFrameUpdate(frameInfo);
		}
	}

	void LayerStack::onUpdateUi(FrameInfo& frameInfo) {
		for (auto& layer : m_layers) {
			layer->onUpdateUi(frameInfo);
		}
	}
}