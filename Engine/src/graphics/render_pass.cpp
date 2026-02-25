#include "graphics/render_pass.hpp"

namespace pxt {
    RenderPass::RenderPass(Context& context, const VkRenderPassCreateInfo& createInfo,
                           const VkAttachmentDescription colorAttachmentDescription,
                           const VkAttachmentDescription depthAttachmentDescription, std::string name)
        : m_context(context), m_createInfo(createInfo), m_colorAttachmentDescription(colorAttachmentDescription),
          m_depthAttachmentDescription(depthAttachmentDescription), m_name(name) {
        PXT_DEBUG("Creating VkRenderPass: {}", m_name);
        if (vkCreateRenderPass(m_context.getDevice(), &m_createInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
            throw std::runtime_error("[RenderPass] Failed to create VkRenderPass: " + m_name);
        }
        PXT_DEBUG("VkRenderPass {} created successfully.", m_name);
    }

    RenderPass::RenderPass(Context& context, const VkRenderPassCreateInfo& createInfo,
                           const VkAttachmentDescription colorAttachmentDescription, std::string name)
        : m_context(context), m_createInfo(createInfo), m_colorAttachmentDescription(colorAttachmentDescription),
          m_name(name) {
        PXT_DEBUG("Creating VkRenderPass (no depth): {}", m_name);
        if (vkCreateRenderPass(m_context.getDevice(), &m_createInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
            throw std::runtime_error("[RenderPass] Failed to create VkRenderPass: " + m_name);
        }
        PXT_DEBUG("VkRenderPass {} created successfully.", m_name);

        m_hasDepth = false;
    }

    RenderPass::~RenderPass() {
        if (m_renderPass != VK_NULL_HANDLE) {
            m_context.getDeletionQueue().push([device = m_context.getDevice(), renderPass = m_renderPass, name = m_name]() {
                 vkDestroyRenderPass(device, renderPass, nullptr);
                 PXT_DEBUG("VkRenderPass: {} destroyed.", name);
            });
        }
    }
} // namespace pxt