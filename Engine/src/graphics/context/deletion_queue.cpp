#include "graphics/context/deletion_queue.hpp"

#include "graphics/swap_chain.hpp"

namespace pxt {
    DeletionQueue::DeletionQueue() { 
        m_framesQueue.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    }

    void DeletionQueue::push(DeletionTask&& task) { 
        m_framesQueue[m_currentFrame].push_back(std::move(task));
    }

    void DeletionQueue::flush(const uint32_t frameIndex) {
        // frameIndex is the one the GPU just finished
        for (auto& task : m_framesQueue[frameIndex]) {
            task(); // Calls the lambda (e.g., vkDestroyImage)
        }
        m_framesQueue[frameIndex].clear();
    }

    void DeletionQueue::flushAllImmediate() {
        for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
            flush(i);
        }
    }
}