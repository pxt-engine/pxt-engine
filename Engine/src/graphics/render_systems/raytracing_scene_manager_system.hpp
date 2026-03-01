#pragma once

#include "core/pch.hpp"
#include "graphics/descriptors/descriptors.hpp"
#include "graphics/descriptors/descriptor_manager.hpp"
#include "graphics/frame_info.hpp"
#include "graphics/resources/blas_registry.hpp"
#include "graphics/resources/material_registry.hpp"
#include "graphics/resources/vk_buffer.hpp"
#include "graphics/swap_chain.hpp"

namespace pxt {
    struct alignas(16) MeshInstanceData {
        VkDeviceAddress vertexBufferAddress;       // offset 0, size 8
        VkDeviceAddress indexBufferAddress;        // offset 8, size 8
        uint32_t materialIndex;                    // offset 16, size 4
        uint32_t emitterIndex;                     // offset 20, size 4
        uint32_t volumeIndex;                      // offset 24, size 4
        float textureTilingFactor;                 // offset 28, size 4
        alignas(16) glm::vec4 textureTintColor;    // offset 32, size 16 (4 floats, 4 bytes each)
        alignas(16) glm::mat4 objectToWorldMatrix; // offset 48, size 64 (4x4 matrix, 16 bytes per row)
        alignas(16) glm::mat4 worldToObjectMatrix; // offset 112, size 64 (4x4 matrix, 16 bytes per row)
    };

    struct alignas(uint32_t) EmitterData {
        uint32_t instanceIndex;
        uint32_t numberOfFaces;
    };

    struct alignas(16) VolumeData {
        glm::vec4 absorption;
        glm::vec4 scattering;
        float phaseFunctionG;
        uint32_t densityTextureId;
        uint32_t detailTextureId;
        uint32_t instanceIndex;
    };

    class RayTracingSceneManagerSystem {
    public:
        RayTracingSceneManagerSystem(Context& context, MaterialRegistry& materialRegistry, BLASRegistry& blasRegistry,
                                     TextureRegistry& textureRegistry, DescriptorManager& descriptorManager);
        ~RayTracingSceneManagerSystem();

        // Delete the copy constructor and copy assignment operator
        RayTracingSceneManagerSystem(const RayTracingSceneManagerSystem&) = delete;
        RayTracingSceneManagerSystem& operator=(const RayTracingSceneManagerSystem&) = delete;

        void createTLAS(FrameInfo& frameInfo);

        void updateTLAS() {} // to implement later

        VkDescriptorSet getTLASDescriptorSet(uint32_t frameIndex) const {
            return m_descriptorManager.getDescriptorSet(m_tlasDescriptorSet, frameIndex);
        }

        VkDescriptorSetLayout getTLASDescriptorSetLayout() const {
            return m_descriptorManager.getLayout(m_tlasDescriptorSet).getHandle();
        }

        VkDescriptorSet getMeshInstanceDescriptorSet(uint32_t frameIndex) const {
            return m_descriptorManager.getDescriptorSet(m_meshInstanceDescriptorSet, frameIndex);
        }

        VkDescriptorSetLayout getMeshInstanceDescriptorSetLayout() const {
            return m_descriptorManager.getLayout(m_meshInstanceDescriptorSet).getHandle();
        }

        VkDescriptorSet getEmittersDescriptorSet(uint32_t frameIndex) const {
            return m_descriptorManager.getDescriptorSet(m_emittersDescriptorSet, frameIndex);
        }

        VkDescriptorSetLayout getEmittersDescriptorSetLayout() const {
            return m_descriptorManager.getLayout(m_emittersDescriptorSet).getHandle();
        }

        VkDescriptorSet getVolumeDescriptorSet(uint32_t frameIndex) const {
            return m_descriptorManager.getDescriptorSet(m_volumesDescriptorSet, frameIndex);
        }

        VkDescriptorSetLayout getVolumeDescriptorSetLayout() const {
            return m_descriptorManager.getLayout(m_volumesDescriptorSet).getHandle();
        }

    private:
        void destroyTLAS(uint32_t frameIndex);
        VkTransformMatrixKHR glmToVkTransformMatrix(const glm::mat4& glmMatrix);

        void createTLASDescriptorSets();
        void updateTLASDescriptorSets(uint32_t frameIndex, VkAccelerationStructureKHR& newTlas);

        void createMeshInstanceDescriptorSets();
        void updateMeshInstanceDescriptorSets(uint32_t frameIndex);

        void createEmittersDescriptorSets();
        void updateEmittersDescriptorSets(uint32_t frameIndex);

        void createVolumesDescriptorSets();
        void updateVolumesDescriptorSets(uint32_t frameIndex);

        Context& m_context;
        MaterialRegistry& m_materialRegistry;
        BLASRegistry& m_blasRegistry;
        TextureRegistry& m_textureRegistry;

        std::vector<VkAccelerationStructureKHR> m_tlases{SwapChain::MAX_FRAMES_IN_FLIGHT};
        Unique<VulkanBuffer> m_tlasBuffer;
        VkAccelerationStructureBuildSizesInfoKHR m_buildSizeInfo{};
        VkAccelerationStructureCreateInfoKHR m_createInfo{};

        DescriptorManager& m_descriptorManager;
        DescriptorSetHandle m_tlasDescriptorSet = core::UID::s_invalidId;

        std::vector<MeshInstanceData> m_meshInstanceData;
        std::vector<Unique<VulkanBuffer>> m_meshInstanceBuffers{SwapChain::MAX_FRAMES_IN_FLIGHT};
        DescriptorSetHandle m_meshInstanceDescriptorSet = core::UID::s_invalidId;

        std::vector<EmitterData> m_emitters;
        std::vector<Unique<VulkanBuffer>> m_emittersBuffers{SwapChain::MAX_FRAMES_IN_FLIGHT};
        DescriptorSetHandle m_emittersDescriptorSet = core::UID::s_invalidId;

        std::vector<VolumeData> m_volumes;
        std::vector<Unique<VulkanBuffer>> m_volumesBuffers{SwapChain::MAX_FRAMES_IN_FLIGHT};
        DescriptorSetHandle m_volumesDescriptorSet = core::UID::s_invalidId;
    };
} // namespace pxt