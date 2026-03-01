#pragma once

#include "core/pch.hpp"
#include "graphics/descriptors/descriptors.hpp"
#include "graphics/descriptors/descriptor_manager.hpp"
#include "graphics/resources/texture2d.hpp"
#include "resources/resource.hpp"
#include "resources/types/image.hpp"

namespace pxt {

    /**
     * @class TextureRegistry
     *
     * @brief Manages a collection of textures and their binding to GPU descriptor sets.
     */
    class TextureRegistry {
    public:
        explicit TextureRegistry(Context& context, DescriptorManager& descriptorManager);

        /**
         * @brief Adds a texture to the registry.
         *
         * Only 2D textures (Texture2D) are supported.
         * If the provided image is not a Texture2D, the function returns 0.
         *
         * @param image Shared pointer to the image.
         *
         * @return Index of the added texture if valid, otherwise 0.
         */
        uint32_t add(const Shared<Image>& image);

        /**
         * @brief Gets the index of a texture in the registry by its resource ID.
         *
         * @param id Resource ID of the texture.
         *
         * @return Index if found, otherwise 0.
         */
        [[nodiscard]] uint32_t getIndex(const ResourceId& id) const;

        /**
         * @brief Gets the index of a texture in the registry by its resource alias (string path or unique name).
         *
         * @param id Resource ID of the texture.
         *
         * @return Index if found, otherwise 0.
         */
        [[nodiscard]] uint32_t getIndex(const std::string& alias) const;

        uint32_t getTextureCount() const { return static_cast<uint32_t>(m_textures.size()); }

        /**
         * @brief Returns the Vulkan descriptor set that holds all texture bindings.
         * 
         * @param frameIndex The current frame index for double/triple buffering. This is used to retrieve the correct
         * descriptor set for the current frame in flight.
         * @note this set in particular is a bindless set, in the future the manager has to handle these.
         *
         * @return Vulkan descriptor set.
         */
        VkDescriptorSet getDescriptorSet(uint32_t frameIndex);

        /**
         * @brief Returns the Vulkan descriptor set layout used for texture bindings.
         *
         * @return Vulkan descriptor set layout.
         */
        VkDescriptorSetLayout getDescriptorSetLayout();

        /**
         * @brief Creates a Vulkan descriptor set for all registered textures.
         *
         * This function constructs a descriptor set layout with a combined image sampler binding,
         * prepares image info for each texture, allocates the descriptor set, and writes all image bindings to it.
         */
        void createDescriptorSets();

    private:
        std::vector<Shared<Image>> m_textures;
        std::unordered_map<ResourceId, uint32_t> m_idToIndex;
        std::unordered_map<std::string, uint32_t> m_aliasToIndex;

        Context& m_context;
        DescriptorManager& m_descriptorManager;
        DescriptorSetHandle m_textureDescriptorSet = core::UID::s_invalidId;
    };
} // namespace pxt
