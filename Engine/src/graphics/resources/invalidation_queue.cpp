#include "graphics/resources/invalidation_queue.hpp"

#include "application.hpp"
#include "graphics/swap_chain.hpp"

namespace pxt {
    InvalidationQueue::InvalidationQueue() { m_framesQueue.resize(SwapChain::MAX_FRAMES_IN_FLIGHT); }

    void InvalidationQueue::push(ResourceInvalidation&& invalidatedResource) {
        m_framesQueue[m_currentFrame].push_back(std::move(invalidatedResource));
    }

    void InvalidationQueue::flush(const uint32_t frameIndex) {
        // frameIndex is the one the GPU just finished
        for (auto& resource : m_framesQueue[frameIndex]) {
            switch (resource.type) {
            case Resource::Type::Image:
                PXT_WARN("Image invalidation is not implemented yet");
                break;
            case Resource::Type::Model:
                PXT_WARN("Model invalidation is not implemented yet");
                break;
            case Resource::Type::Mesh:
                PXT_WARN("Mesh invalidation is not implemented yet");
                break;
            case Resource::Type::Material:
                Application::get().getMaterialRegistry().markBufferDirty(frameIndex);
                break;
            default:
                PXT_ASSERT(false, "Unknown resource type for invalidation");
                break;
            }
        }
        m_framesQueue[frameIndex].clear();
    }
} // namespace pxt