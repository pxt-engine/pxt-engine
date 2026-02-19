#pragma once

#include "editor_texture_registry.hpp"
#include "application.hpp"
#include "constants.hpp"
#include "core/filesystem.hpp"
#include "graphics/descriptors/descriptors.hpp"

namespace pxt::editor {
    EditorTextureRegistry::EditorTextureRegistry() {
        Context& context = Application::get().getContext();

        createDescriptorAllocator(context);

        m_defaultImageInfo.format = ImageFormat::RGBA8_SRGB;
        m_defaultImageInfo.channels = 4;
        m_defaultImageInfo.flags = ImageFlags::NoSampler;

        m_defaultSampler = VulkanSampler::createSimpleLinearSampler(context);

        loadEditorTextures(context);
    }

    EditorTextureRegistry::~EditorTextureRegistry() = default;

    void EditorTextureRegistry::createDescriptorAllocator(Context& context) {
        std::vector<PoolSizeRatio> poolSizes = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 200.f}, // pessimitic
        };

        uint32_t maxSets = 1024;

        m_descriptorAllocator = createUnique<DescriptorAllocatorGrowable>(context, maxSets, poolSizes);
    }

    void EditorTextureRegistry::loadEditorTextures(Context& context) {
        const std::vector<std::string> files = core::FileSystem::getAllFilesRecursive(EDITOR_TEXTURES_PATH, true);

        size_t initialCount = files.size();

        m_textures.reserve(initialCount);

        m_textureDescriptorSetLayout =
            DescriptorSetLayout::Builder(context)
                .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                .build();

        for (const auto& file : files) {
            add(context, file);
        }
    }

    VkDescriptorSet EditorTextureRegistry::createDescriptorSet(Context& context, VkDescriptorImageInfo imageInfo) {

        VkDescriptorSet textureDescriptorSet = VK_NULL_HANDLE;
        m_descriptorAllocator->allocate(m_textureDescriptorSetLayout->getHandle(), textureDescriptorSet);

        if (textureDescriptorSet == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to allocate descriptor set for texture inside editor pool");
        }

        DescriptorWriter(context, *m_textureDescriptorSetLayout)
            .writeImage(0, &imageInfo)
            .updateSet(textureDescriptorSet);

        return textureDescriptorSet;
    }

    VkDescriptorSet EditorTextureRegistry::get(const std::string& alias) const {
        auto it = m_alias2DescriptorSet.find(alias);
        if (it != m_alias2DescriptorSet.end()) {
            return it->second;
        }

        return getMissingTextureDescriptorSet();
    }

    void EditorTextureRegistry::add(Context& context, const std::string& alias, ImageInfo* imageInfo) {
        const std::string fullPath = EDITOR_TEXTURES_PATH + alias;

        if (imageInfo == nullptr) {
            imageInfo = &m_defaultImageInfo;
        }

        Unique<Texture2D> texture = TextureImporter::importTexture2D(fullPath, imageInfo);
        texture->setSampler(m_defaultSampler);

        VkDescriptorSet descriptorSet = createDescriptorSet(context, texture->getImageInfo());

        m_alias2DescriptorSet[alias] = descriptorSet;
        m_resourceId2DescriptorSet[texture->id] = descriptorSet;
        m_textures.push_back(std::move(texture));
    }

    VkDescriptorSet EditorTextureRegistry::getMissingTextureDescriptorSet() const {
        return m_alias2DescriptorSet.at(MISSING_TEXTURE);
    }

} // namespace pxt::editor