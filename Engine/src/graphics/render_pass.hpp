#pragma once

#include "core/pch.hpp"
#include "graphics/context/context.hpp"

// RenderPass Wrapper Class
// mainly used to save the render pass creation info and the
// resources linked to it
namespace pxt {
    class RenderPass {
    public:
        RenderPass(Context& context, const VkRenderPassCreateInfo& createInfo,
                   const VkAttachmentDescription colorAttachmentDescription,
                   const VkAttachmentDescription depthAttachmentDescription, std::string name);
        RenderPass(Context& context, const VkRenderPassCreateInfo& createInfo,
                   const VkAttachmentDescription colorAttachmentDescription, std::string name);

        ~RenderPass();

        // Disable copy constructor and assignment operator for proper resource management
        RenderPass(const RenderPass&) = delete;
        RenderPass& operator=(const RenderPass&) = delete;

        VkRenderPass getHandle() const { return m_renderPass; }

        const VkRenderPassCreateInfo& getCreateInfo() const { return m_createInfo; }

        const std::string& getName() const { return m_name; }

        const VkAttachmentDescription getColorAttachmentDescription() const { return m_colorAttachmentDescription; }

        std::optional<VkAttachmentDescription> getDepthAttachmentDescription() const {
            if (!m_hasDepth) {
                return std::nullopt;
            }
            return m_depthAttachmentDescription;
        }

        const VkImageLayout getColorAttachmentInitialLayout() const {
            return m_colorAttachmentDescription.initialLayout;
        }

        const VkImageLayout getColorAttachmentFinalLayout() const { return m_colorAttachmentDescription.finalLayout; }

        const bool hasDepth() const { return m_hasDepth; }

        const VkImageLayout getDepthAttachmentInitialLayout() const {
            return m_hasDepth ? m_depthAttachmentDescription.initialLayout : VK_IMAGE_LAYOUT_UNDEFINED;
        }

        const VkImageLayout getDepthAttachmentFinalLayout() const {
            return m_hasDepth ? m_depthAttachmentDescription.finalLayout : VK_IMAGE_LAYOUT_UNDEFINED;
        }

    private:
        Context& m_context;
        std::string m_name; // Name for logging
        VkRenderPassCreateInfo m_createInfo;
        VkAttachmentDescription m_colorAttachmentDescription{};
        VkAttachmentDescription m_depthAttachmentDescription{};
        VkRenderPass m_renderPass = VK_NULL_HANDLE; // renderPass Vulkan handle

        bool m_hasDepth = true;
    };
} // namespace pxt