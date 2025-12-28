#pragma once

#include "core/pch.hpp"
#include "graphics/descriptors/descriptors.hpp"
#include "graphics/resources/texture2d.hpp"
#include "resources/resource.hpp"
#include "resources/types/image.hpp"

namespace pxt::editor {
    /**
     * @class EditorTextureRegistry
     *
     * @brief Manages textures in the editor
     */
    class EditorTextureRegistry {
    public:
        EditorTextureRegistry();
        ~EditorTextureRegistry();

        VkDescriptorSet get(const std::string& alias);

        void add(Context& context, const std::string& alias, ImageInfo* imageInfo = nullptr);

        VkDescriptorSet getMissingTextureDescriptorSet();

    private:
        void loadEditorTextures(Context& context);
        void createDescriptorAllocator(Context& context);
        VkDescriptorSet createDescriptorSet(Context& context, VkDescriptorImageInfo imageInfo);

        std::vector<Unique<Texture2D>> m_textures;
        std::unordered_map<ResourceId, VkDescriptorSet> m_resourceId2DescriptorSet;
        std::unordered_map<std::string, VkDescriptorSet> m_alias2DescriptorSet;

        Unique<DescriptorAllocatorGrowable> m_descriptorAllocator = nullptr;
        Unique<DescriptorSetLayout> m_textureDescriptorSetLayout = nullptr;

        Shared<VulkanSampler> m_defaultSampler = nullptr;
        ImageInfo m_defaultImageInfo{};
    };
} // namespace pxt::editor