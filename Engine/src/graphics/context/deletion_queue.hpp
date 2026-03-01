#pragma once

#include "core/pch.hpp"

namespace pxt {
    class DeletionQueue {
        using DeletionTask = std::function<void()>;

    public:
        explicit DeletionQueue();

        /*
         *@brief Pushes a deletion task to the queue for the current frame
         * 
         *@param task The deletion task to be executed later
         */
        void push(DeletionTask&& task);

        /*
         *@brief Flushes the deletion tasks for a specific frame index
         *
         *@param frameIndex The index of the frame whose deletion tasks should be executed
         */
        void flush(const uint32_t frameIndex);

        /*
          *@brief Flushes all pending deletion tasks for all frames
          *
          * This method should be called during application shutdown to ensure that all resources are properly released.
          * It must be called after the device is idle and before it is destroyed.
         */
        void flushAllImmediate();

        void updateFrameIndex(const uint32_t newIndex) { m_currentFrame = newIndex; }

    private:
        uint32_t m_currentFrame = 0;
        // this can become array of vectors if we move Swapchain::MAX_FRAMES_IN_FLIGHT
        // to a constants file so there is no recursive imports
        std::vector<std::vector<DeletionTask>> m_framesQueue;
    };
}