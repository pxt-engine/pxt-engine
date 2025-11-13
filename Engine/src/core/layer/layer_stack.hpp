#pragma once
#include "core/pch.hpp"
#include "core/events/event.hpp"
#include "core/layer/layer.hpp"
#include "graphics/frame_info.hpp"

namespace PXTEngine {
	class LayerStack {
	public:
		LayerStack() = default;
		~LayerStack();
		
		void onEvent(Event& event);
		void onUpdate(FrameInfo& frameInfo, GlobalUbo& ubo);
		void onPostFrameUpdate(FrameInfo& frameInfo);
		void onUpdateUi(FrameInfo& frameInfo);

		// we insert a layer with emplace before pos (layerInsertIndex + 1)
		template<typename TLayer>
		requires(std::is_base_of_v<Layer, TLayer>)
		TLayer* pushLayer(Unique<TLayer> layer) {
			TLayer* layerPtr = layer.get();
			m_layers.emplace(m_layers.begin() + m_layerInsertIndex, std::move(layer));
			m_layerInsertIndex++;
 
			PXT_DEBUG("Pushed layer: {}. Total layers: {}", layerPtr->getName(), m_layers.size());

			return layerPtr;
		}

		// an overlay is a layer on top of everything else
		// we thus use emplace back.
		template<typename TLayer>
		requires(std::is_base_of_v<Layer, TLayer>)
		TLayer* pushOverlay(Unique<TLayer> overlay) {
			TLayer* overlayPtr = overlay.get();
			m_layers.emplace_back(std::move(overlay));

			PXT_DEBUG("Pushed overlay: {}. Total layers: {}", overlayPtr->getName(), m_layers.size());

			return overlayPtr;
		}

		// check if correct
		void popLayer(Layer& layer) {
			// find and remove the layer
			auto it = std::find_if(m_layers.begin(), m_layers.end(),
				[&layer](const Unique<Layer>& l) { return l.get() == &layer; });
			if (it != m_layers.end()) {
				PXT_DEBUG("Popped layer: {}. Total layers: {}", layer.getName(), m_layers.size());

				m_layers.erase(it);
				m_layerInsertIndex--;
			}
		}

		void popOverlay(Layer& overlay) {
			// find and remove the overlay (search from the back)
			auto it = std::find_if(m_layers.rbegin(), m_layers.rend(),
				[&overlay](const Unique<Layer>& l) { return l.get() == &overlay; });

			if (it != m_layers.rend()) {
				PXT_DEBUG("Popped overlay: {}. Total layers: {}", overlay.getName(), m_layers.size());

				m_layers.erase(std::next(it).base()); // erase uses normal iterator
			}
		}

	private:
		std::vector<Unique<Layer>> m_layers;
		size_t m_layerInsertIndex = 0;
	};
}