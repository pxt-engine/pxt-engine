#pragma once
#include "core/events/event.hpp"
#include "core/pch.hpp"
#include "graphics/frame_info.hpp"

namespace pxt::core {
    class Layer {
    public:
        Layer(const std::string& name = "Unnamed-Layer");
        virtual ~Layer() = default;

        virtual void onAttach() {}

        virtual void onDetach() {}

        virtual void onBeginFrame(float deltaTime) {}

        virtual void onUpdate(FrameInfo& frameInfo, GlobalUbo& ubo) {}

        virtual void onPostFrameUpdate(FrameInfo& frameInfo) {}

        virtual void onUpdateUi(FrameInfo& frameInfo) {}

        virtual void onEvent(Event& event) {}

        [[nodiscard]] const std::string& getName() const { return m_name; }

    private:
        std::string m_name;
    };
} // namespace pxt::core