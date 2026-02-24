#pragma once

#include "core/pch.hpp"
#include "graphics/descriptors/descriptors.hpp"
#include "graphics/descriptors/descriptor_manager.hpp"
#include "graphics/resources/texture_registry.hpp"
#include "graphics/resources/vk_buffer.hpp"
#include "graphics/swap_chain.hpp"
#include "resources/resource.hpp"
#include "resources/types/material.hpp"

namespace pxt {

    /**
     * @struct MaterialData
     *
     * @brief This struct is the GPU-side representation of a material's properties.
     * It is designed to be tightly packed and uploaded to a Shader Storage Buffer Object (SSBO).
     *
     * @note The alignas(16) specifier is crucial. It ensures that the struct's size is a multiple
     * of 16 bytes, matching the std430 layout rules for SSBOs in GLSL. This prevents memory
     * alignment issues on the GPU when accessing an array of these structs.
     */
    struct alignas(16) MaterialData {
        glm::vec4 albedoColor;
        glm::vec4 emissiveColor;
        int albedoMapIndex;
        int normalMapIndex;
        int ambientOcclusionMapIndex;
        float metallic;
        int metallicMapIndex;
        float roughness;
        int roughnessMapIndex;
        int emissiveMapIndex;
        float transmission;
        float ior;
        float blinnPhongSpecularIntensity;
        float blinnPhongSpecularShininess;
    };

    /**
     * @class MaterialRegistry
     *
     * @brief This class is a central manager for all Material resources in the application.
     * It orchestrates the conversion of CPU-side Material objects into a GPU-consumable buffer and
     * provides the necessary Vulkan descriptors for shaders to access this data.
     */
    class MaterialRegistry {
    public:
        explicit MaterialRegistry(Context& context, DescriptorManager& descriptorManager, TextureRegistry& textureRegistry);

        /**
         * @brief Adds a material to the registry.
         *
         * @param material Shared pointer to the material to add.
         *
         * @return Index of the added material in the registry.
         */
        uint32_t add(const Shared<Material> material);

        /**
         * @brief Retrieves the index of a material by its resource ID.
         *
         * @param id The resource ID of the material.
         *
         * @return Index of the material if found, otherwise 0.
         */
        uint32_t getIndex(const ResourceId& id) const;

        /**
         * @brief Gets the Vulkan descriptor set used for the materials in the frame frameIndex.
         *
         * @return The Vulkan descriptor set.
         */
        VkDescriptorSet getDescriptorSet(uint32_t frameIndex);

        /**
         * @brief Gets the Vulkan descriptor set layout used for the materials.
         *
         * @return The Vulkan descriptor set layout.
         */
        VkDescriptorSetLayout getDescriptorSetLayout();

        /**
         * @brief Creates the descriptor set and GPU buffer for the registered materials.
         *
         * This method prepares the material data for use in shaders by uploading it to GPU memory
         * and writing it into a Vulkan descriptor set.
         */
        void createDescriptorSets();

        void updateDescriptorSet(uint32_t frameIndex);

    private:
        /**
         * @brief Converts a Material object into its corresponding GPU-ready MaterialData structure.
         *
         * @param material Shared pointer to the material.
         *
         * @return A MaterialData struct containing data for GPU usage.
         */
        MaterialData getMaterialData(Shared<Material> material);

        Context& m_context;
        DescriptorManager& m_descriptorManager;
        TextureRegistry& m_textureRegistry;

        std::vector<Shared<Material>> m_materials;
        std::unordered_map<ResourceId, uint32_t> m_idToIndex;

        // we use this to determine whether to update the descriptor set or not
        //TODO: the update is now called by the render system, it should become a centralized sync point when
        //      this class becomes a field of the resource manager
        std::array<bool, SwapChain::MAX_FRAMES_IN_FLIGHT> isBufferDirty{false};

        std::vector<Unique<VulkanBuffer>> m_materialsGpuBuffers{SwapChain::MAX_FRAMES_IN_FLIGHT};
        DescriptorSetHandle m_materialDescriptorSet = core::UID::s_invalidId;
    };
} // namespace pxt
