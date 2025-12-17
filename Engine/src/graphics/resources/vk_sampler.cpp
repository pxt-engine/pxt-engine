#include "graphics/resources/vk_sampler.hpp"
#include "vk_sampler.hpp"

namespace pxt {
	VulkanSampler::VulkanSampler(Context& context,
		VkFilter filter,
		VkSamplerAddressMode addressMode,
		bool unnormalizedCoordinates,
		VkSamplerMipmapMode mipMapMode,
		VkBorderColor borderColor) :
		m_context(context)
	{
		m_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		m_info.magFilter = filter;
		m_info.minFilter = filter;
		m_info.addressModeU = addressMode;
		m_info.addressModeV = addressMode;
		m_info.addressModeW = addressMode;
		m_info.anisotropyEnable = unnormalizedCoordinates ? VK_FALSE : m_context.supportsAnisotropy();
		m_info.maxAnisotropy = unnormalizedCoordinates ? 0.0 : m_context.getMaxSamplerAnisotropy();
		m_info.borderColor = borderColor;
		m_info.unnormalizedCoordinates = unnormalizedCoordinates ? VK_TRUE : VK_FALSE;
		m_info.compareEnable = VK_FALSE;
		m_info.compareOp = VK_COMPARE_OP_ALWAYS;
		m_info.mipmapMode = mipMapMode;
		m_info.mipLodBias = 0.0f;
		m_info.minLod = 0.0f;
		m_info.maxLod = 0.0f;

		m_sampler = context.createSampler(m_info);
	}

	VulkanSampler::VulkanSampler(Context& context,
		const VkSamplerCreateInfo& samplerCreateInfo) :
		m_context(context),
		m_info(samplerCreateInfo)
	{
		m_sampler = context.createSampler(m_info);
	}

	Unique<VulkanSampler> VulkanSampler::createSimpleLinearSampler(Context& context, bool unnormalizedCoordinates) {
		return createUnique<VulkanSampler>(
			context,
			VK_FILTER_LINEAR,
			VK_SAMPLER_ADDRESS_MODE_REPEAT,
			unnormalizedCoordinates,
			VK_SAMPLER_MIPMAP_MODE_LINEAR
		);
	}

	Unique<VulkanSampler> VulkanSampler::createSimpleNearestSampler(Context& context, bool unnormalizedCoordinates) {
		return createUnique<VulkanSampler>(
			context,
			VK_FILTER_LINEAR,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			unnormalizedCoordinates,
			VK_SAMPLER_MIPMAP_MODE_NEAREST
		);
	}

	VulkanSampler::~VulkanSampler() {
		if (m_sampler != VK_NULL_HANDLE) {
			vkDestroySampler(m_context.getDevice(), m_sampler, nullptr);
			m_sampler = VK_NULL_HANDLE;
		}
	}

}