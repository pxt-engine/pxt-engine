#pragma once

#include "core/pch.hpp"
#include "graphics/context/context.hpp"

namespace pxt {
    class VulkanSampler {
    public:
        VulkanSampler(Context& context, VkFilter filter, VkSamplerAddressMode addressMode,
                      bool unnormalizedCoordinates = false,
                      VkSamplerMipmapMode mipMapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                      VkBorderColor borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK);

        VulkanSampler(Context& context, const VkSamplerCreateInfo& samplerCreateInfo);

        static Unique<VulkanSampler> createSimpleLinearSampler(Context& context, bool unnormalizedCoordinates = false);

        static Unique<VulkanSampler> createSimpleNearestSampler(Context& context, bool unnormalizedCoordinates = true);

        ~VulkanSampler();

        VkSampler getHandle() const { return m_sampler; }

        VkSamplerCreateInfo& getCreateInfo() { return m_info; }

    private:
        Context& m_context;

        VkSampler m_sampler = VK_NULL_HANDLE;

        VkSamplerCreateInfo m_info{};
    };
} // namespace pxt