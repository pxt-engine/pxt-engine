#pragma once

#include "core/pch.hpp"
#include "graphics/resources/material_registry.hpp"
#include "graphics/resources/blas_registry.hpp"
#include "graphics/resources/vk_buffer.hpp"
#include "graphics/frame_info.hpp"
#include "graphics/descriptors/descriptors.hpp"
#include "graphics/swap_chain.hpp"

namespace PXTEngine {
	struct alignas(16) MeshInstanceData {
		VkDeviceAddress vertexBufferAddress;		// offset 0, size 8
		VkDeviceAddress indexBufferAddress;			// offset 8, size 8
		uint32_t materialIndex;						// offset 16, size 4
		uint32_t emitterIndex;						// offset 20, size 4
		uint32_t volumeIndex;						// offset 24, size 4
		float textureTilingFactor;					// offset 28, size 4
		alignas(16) glm::vec4 textureTintColor;		// offset 32, size 16 (4 floats, 4 bytes each)
		alignas(16) glm::mat4 objectToWorldMatrix;	// offset 48, size 64 (4x4 matrix, 16 bytes per row)
		alignas(16) glm::mat4 worldToObjectMatrix;	// offset 112, size 64 (4x4 matrix, 16 bytes per row)
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
		RayTracingSceneManagerSystem(Context& context, MaterialRegistry& materialRegistry, BLASRegistry& blasRegistry, TextureRegistry& textureRegistry, Shared<DescriptorAllocatorGrowable> allocator);
		~RayTracingSceneManagerSystem();

		// Delete the copy constructor and copy assignment operator
		RayTracingSceneManagerSystem(const RayTracingSceneManagerSystem&) = delete;
		RayTracingSceneManagerSystem& operator=(const RayTracingSceneManagerSystem&) = delete;

		void createTLAS(FrameInfo& frameInfo);
		void updateTLAS() {} // to implement later
		VkDescriptorSet getTLASDescriptorSet(int frameIndex) const { return m_tlasDescriptorSets[frameIndex]; }
		VkDescriptorSetLayout getTLASDescriptorSetLayout() const { return m_tlasDescriptorSetLayout->getDescriptorSetLayout(); }

		VkDescriptorSet getMeshInstanceDescriptorSet(int frameIndex) const { return m_meshInstanceDescriptorSets[frameIndex]; }
		VkDescriptorSetLayout getMeshInstanceDescriptorSetLayout() const { return m_meshInstanceDescriptorSetLayout->getDescriptorSetLayout(); }

		VkDescriptorSet getEmittersDescriptorSet(int frameIndex) const { return m_emittersDescriptorSets[frameIndex]; }
		VkDescriptorSetLayout getEmittersDescriptorSetLayout() const { return m_emittersDescriptorSetLayout->getDescriptorSetLayout(); }

		VkDescriptorSet getVolumeDescriptorSet(int frameIndex) const { return m_volumesDescriptorSets[frameIndex]; }
		VkDescriptorSetLayout getVolumeDescriptorSetLayout() const { return m_volumesDescriptorSetLayout->getDescriptorSetLayout(); }
	private:
		void destroyTLAS(int frameIndex);
		VkTransformMatrixKHR glmToVkTransformMatrix(const glm::mat4& glmMatrix);

		void createTLASDescriptorSets();
		void updateTLASDescriptorSets(int frameIndex, VkAccelerationStructureKHR& newTlas);

		void createMeshInstanceDescriptorSets();
		void updateMeshInstanceDescriptorSets(int frameIndex);

		void createEmittersDescriptorSets();
		void updateEmittersDescriptorSets(int frameIndex);

		void createVolumesDescriptorSets();
		void updateVolumesDescriptorSets(int frameIndex);

		Context& m_context;
		MaterialRegistry& m_materialRegistry;
		BLASRegistry& m_blasRegistry;
		TextureRegistry& m_textureRegistry;

		std::vector<VkAccelerationStructureKHR> m_tlases{ SwapChain::MAX_FRAMES_IN_FLIGHT };
		Unique<VulkanBuffer> m_tlasBuffer;
		VkAccelerationStructureBuildSizesInfoKHR m_buildSizeInfo{};
		VkAccelerationStructureCreateInfoKHR m_createInfo{};

		Shared<DescriptorAllocatorGrowable> m_descriptorAllocator;
		Shared<DescriptorSetLayout> m_tlasDescriptorSetLayout = nullptr;
		std::vector<VkDescriptorSet> m_tlasDescriptorSets{ SwapChain::MAX_FRAMES_IN_FLIGHT };

		std::vector<MeshInstanceData> m_meshInstanceData;
		Shared<DescriptorSetLayout> m_meshInstanceDescriptorSetLayout = nullptr;
		std::vector<Unique<VulkanBuffer>> m_meshInstanceBuffers{ SwapChain::MAX_FRAMES_IN_FLIGHT };
		std::vector<VkDescriptorSet> m_meshInstanceDescriptorSets{ SwapChain::MAX_FRAMES_IN_FLIGHT };

		std::vector<EmitterData> m_emitters;
		Shared<DescriptorSetLayout> m_emittersDescriptorSetLayout = nullptr;
		std::vector<Unique<VulkanBuffer>> m_emittersBuffers{ SwapChain::MAX_FRAMES_IN_FLIGHT };
		std::vector<VkDescriptorSet> m_emittersDescriptorSets{ SwapChain::MAX_FRAMES_IN_FLIGHT };

		std::vector<VolumeData> m_volumes;
		Shared<DescriptorSetLayout> m_volumesDescriptorSetLayout = nullptr;
		std::vector<Unique<VulkanBuffer>> m_volumesBuffers{ SwapChain::MAX_FRAMES_IN_FLIGHT };
		std::vector<VkDescriptorSet> m_volumesDescriptorSets{ SwapChain::MAX_FRAMES_IN_FLIGHT };
	};
}