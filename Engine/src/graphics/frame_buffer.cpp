#include "graphics/frame_buffer.hpp"

namespace pxt {
    FrameBuffer::FrameBuffer(Context& context, VkFramebufferCreateInfo& createInfo, std::string name,
                             Shared<VulkanImage> colorAttachment, Shared<VulkanImage> depthAttachment)
        : m_context(context), m_name(name), m_createInfo(createInfo), m_colorAttachment(colorAttachment),
          m_depthAttachment(depthAttachment) {

        PXT_DEBUG("Creating VkFrameBuffer: {}", m_name);

        if (!m_colorAttachment) {
            throw std::runtime_error("[FrameBuffer] Color attachment cannot be null for FrameBuffer: " + m_name);
        }

        if (createInfo.pAttachments == nullptr || createInfo.attachmentCount == 0) {
            throw std::runtime_error("[FrameBuffer] No attachments specified for FrameBuffer: " + m_name);
        }

        if (vkCreateFramebuffer(m_context.getDevice(), &m_createInfo, nullptr, &m_frameBuffer) != VK_SUCCESS) {
            throw std::runtime_error("[FrameBuffer] Failed to create VkFrameBuffer: " + m_name);
        }
        PXT_DEBUG("VkFrameBuffer {} created successfully.", m_name);
    }

    FrameBuffer::~FrameBuffer() {
        if (m_frameBuffer != VK_NULL_HANDLE) {
            m_context.getDeletionQueue().push([device = m_context.getDevice(), framebuffer = m_frameBuffer, name = m_name]() {
                 vkDestroyFramebuffer(device, framebuffer, nullptr);
                 PXT_DEBUG("VkFrameBuffer {} destroyed.", name);
            });
        }
    }
} // namespace pxt