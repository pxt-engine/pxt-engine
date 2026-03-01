#include "graphics/resources/texture_registry.hpp"

namespace pxt {

    TextureRegistry::TextureRegistry(Context& context, DescriptorManager& descriptorManager)
        : m_context(context), m_descriptorManager(descriptorManager) {}

    uint32_t TextureRegistry::add(const Shared<Image>& image) {
        auto* texture = dynamic_cast<Texture2D*>(image.get());

        if (!texture) {
            return 0;
        }

        const auto index = static_cast<uint32_t>(m_textures.size());
        m_textures.push_back(image);
        m_idToIndex[image->id] = index;

        if (!texture->alias.empty()) {
            m_aliasToIndex[texture->alias] = index;
        }

        return index;
    }

    uint32_t TextureRegistry::getIndex(const ResourceId& id) const {
        auto it = m_idToIndex.find(id);
        return it != m_idToIndex.end() ? it->second : 0;
    }

    uint32_t TextureRegistry::getIndex(const std::string& alias) const {
        auto it = m_aliasToIndex.find(alias);
        return it != m_aliasToIndex.end() ? it->second : 0;
    }

    VkDescriptorSet TextureRegistry::getDescriptorSet(uint32_t frameIndex) { return m_descriptorManager.getDescriptorSet(m_textureDescriptorSet, frameIndex); }

    VkDescriptorSetLayout TextureRegistry::getDescriptorSetLayout() {
        return m_descriptorManager.getLayout(m_textureDescriptorSet).getHandle();
    }

    void TextureRegistry::createDescriptorSets() {
        std::vector<DescriptorEntry> bindings = {
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            static_cast<uint32_t>(m_textures.size())
        }};
        m_textureDescriptorSet = m_descriptorManager.createSet(bindings);

        std::vector<VkDescriptorImageInfo> imageInfos;
        imageInfos.reserve(getTextureCount());
        for (const auto& image : m_textures) {
            const auto texture = std::static_pointer_cast<Texture2D>(image);

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = texture->getImageView();
            imageInfo.sampler = texture->getSamplerHandle();
            
            imageInfos.push_back(imageInfo);
        }

        m_descriptorManager.submitUpdateArray(m_textureDescriptorSet, 0, imageInfos);
    }
} // namespace pxt
