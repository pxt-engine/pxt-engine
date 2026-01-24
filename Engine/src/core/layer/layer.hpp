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

        virtual void onBeginFrame([[maybe_unused]] float deltaTime) {}

        virtual void onUpdate([[maybe_unused]] FrameInfo& frameInfo, [[maybe_unused]] GlobalUbo& ubo) {}

        virtual void onPostFrameUpdate([[maybe_unused]] FrameInfo& frameInfo) {}

        virtual void onUpdateUi([[maybe_unused]] FrameInfo& frameInfo) {}

        virtual void onEvent([[maybe_unused]] Event& event) {}

        [[nodiscard]] const std::string& getName() const { return m_name; }

    private:
        std::string m_name;
    };
} // namespace pxt::core