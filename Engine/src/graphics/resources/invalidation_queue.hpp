#pragma once

#include "core/pch.hpp"
#include "resources/resource.hpp"

namespace pxt {
    // represents a resource that has been invalidated and needs to be updated
    struct ResourceInvalidation {
        Resource::Type type;
        ResourceId id;
    };

    class InvalidationQueue {

    public:
        explicit InvalidationQueue();

        /*
         *@brief Pushes a resource invalidation to the queue for the current frame
         *
         *@param invalidatedResource The resource invalidation to be executed later
         */
        void push(ResourceInvalidation&& invalidatedResource);

        /*
         *@brief Handles the invalidated resources for a specific frame index
         *
         *@param frameIndex The index of the frame whose resource are invalidated
         */
        void flush(const uint32_t frameIndex);

        void updateFrameIndex(const uint32_t newIndex) { m_currentFrame = newIndex; }

    private:
        uint32_t m_currentFrame = 0;
        std::vector<std::vector<ResourceInvalidation>> m_framesQueue;
    };
} // namespace pxt