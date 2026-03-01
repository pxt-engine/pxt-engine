#include "graphics/resources/material_registry.hpp"

namespace pxt {

    MaterialRegistry::MaterialRegistry(Context& context, DescriptorManager& descriptorManager,
                                       TextureRegistry& textureRegistry)
        : m_context(context), m_descriptorManager(descriptorManager), m_textureRegistry(textureRegistry) {
    }

    uint32_t MaterialRegistry::add(const Shared<Material> material) {
        const auto index = static_cast<uint32_t>(m_materials.size());
        m_materials.push_back(material);
        m_idToIndex[material->id] = index;

        isBufferDirty.fill(true);

        return index;
    }

    uint32_t MaterialRegistry::getIndex(const ResourceId& id) const {
        auto it = m_idToIndex.find(id);
        return it != m_idToIndex.end() ? it->second : 0;
    }

    VkDescriptorSet MaterialRegistry::getDescriptorSet(uint32_t frameIndex) { return m_descriptorManager.getDescriptorSet(m_materialDescriptorSet, frameIndex); }

    VkDescriptorSetLayout MaterialRegistry::getDescriptorSetLayout() {
        return m_descriptorManager.getLayout(m_materialDescriptorSet).getHandle();
    }

    void MaterialRegistry::createDescriptorSets() {
        std::vector<DescriptorEntry> bindings = {
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR, 1}};
        
        m_materialDescriptorSet = m_descriptorManager.createSet(bindings);
    }

    void MaterialRegistry::updateDescriptorSet(uint32_t frameIndex) {
        // we do not update if the buffer is not dirty
        if (!isBufferDirty[frameIndex]) {
            return;
        }

        // if it is, we update it and reset this flag
        isBufferDirty[frameIndex] = false;
        
        std::vector<MaterialData> materialsData;
        for (const auto& material : m_materials) {
            materialsData.push_back(getMaterialData(material));
        }

        VkDeviceSize bufferSize = sizeof(MaterialData) * materialsData.size();

        Unique<VulkanBuffer> stagingBuffer =
            createUnique<VulkanBuffer>(m_context, bufferSize, 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        stagingBuffer->map();
        stagingBuffer->writeToBuffer(materialsData.data(), bufferSize);
        stagingBuffer->unmap();

        m_materialsGpuBuffers[frameIndex] = createUnique<VulkanBuffer>(
            m_context, bufferSize, 1, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        m_context.copyBuffer(stagingBuffer->getBuffer(), m_materialsGpuBuffers[frameIndex]->getBuffer(), bufferSize);

        auto bufferInfo = m_materialsGpuBuffers[frameIndex]->descriptorInfo();

        m_descriptorManager.submitUpdateSingle(m_materialDescriptorSet, 0, bufferInfo);
        m_descriptorManager.flushUpdatesForSet(m_materialDescriptorSet, frameIndex);
    }

    MaterialData MaterialRegistry::getMaterialData(Shared<Material> material) {

        constexpr uint32_t invalidIndex = std::numeric_limits<uint32_t>::max();

        MaterialData data;
        data.albedoColor = material->getAlbedoColor();
        data.emissiveColor = material->getEmissiveColor();
        data.albedoMapIndex = m_textureRegistry.getIndex(material->getAlbedoMap()->id);
        data.normalMapIndex = m_textureRegistry.getIndex(material->getNormalMap()->id);
        data.ambientOcclusionMapIndex = m_textureRegistry.getIndex(material->getAmbientOcclusionMap()->id);
        data.metallic = material->getMetallic();

        data.metallicMapIndex = invalidIndex;
        if (material->getMetallicMap()) {
            data.metallicMapIndex = m_textureRegistry.getIndex(material->getMetallicMap()->id);
        }

        data.roughness = material->getRoughness();

        data.roughnessMapIndex = invalidIndex;
        if (material->getRoughnessMap()) {
            data.roughnessMapIndex = m_textureRegistry.getIndex(material->getRoughnessMap()->id);
        }

        data.emissiveMapIndex = m_textureRegistry.getIndex(material->getEmissiveMap()->id);
        data.transmission = material->getTransmission();
        data.ior = material->getIndexOfRefraction();

        data.blinnPhongSpecularIntensity = material->getBlinnPhongSpecularIntensity();
        data.blinnPhongSpecularShininess = material->getBlinnPhongSpecularShininess();

        return data;
    }
} // namespace pxt
