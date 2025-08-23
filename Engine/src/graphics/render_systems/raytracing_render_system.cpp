#include "graphics/render_systems/raytracing_render_system.hpp"

namespace PXTEngine {
	struct RayTracingPushConstantData {
		uint32_t noiseType = 0;
		uint32_t blueNoiseTextureCount = 0; // Number of blue noise textures available
		uint32_t blueNoiseTextureSize = 0;  // Size of each blue noise texture (assumed square)
		VkBool32 selectSingleTextures = VK_FALSE;  // Whether to select single textures or use
									    // different blue noise textures every frame

		float metalness;
		float roughness;
		float transmission;
		float ior;
		glm::vec4 albedo;

		uint32_t blueNoiseDebugIndex = 0; // Index of the blue noise texture to use in case selectSingleTextures is true
	};

	RayTracingRenderSystem::RayTracingRenderSystem(
		Context& context, Shared<DescriptorAllocatorGrowable> descriptorAllocator,
		TextureRegistry& textureRegistry, MaterialRegistry& materialRegistry,
		BLASRegistry& blasRegistry, Shared<Environment> environment,
		DescriptorSetLayout& globalSetLayout, Shared<VulkanImage> sceneImage)
		: m_context(context),
		m_textureRegistry(textureRegistry),
		m_materialRegistry(materialRegistry),
		m_blasRegistry(blasRegistry),
		m_environment(environment),
		m_descriptorAllocator(descriptorAllocator),
		m_sceneImage(sceneImage)
	{
		m_skybox = std::static_pointer_cast<VulkanSkybox>(m_environment->getSkybox());

		createDescriptorSets();
		defineShaderGroups();
		createPipelineLayout(globalSetLayout);
		createPipeline();
		createShaderBindingTable();
	}

	RayTracingRenderSystem::~RayTracingRenderSystem() {
		vkDestroyPipelineLayout(m_context.getDevice(), m_pipelineLayout, nullptr);
	}


	void RayTracingRenderSystem::createDescriptorSets() {
		// Create storage image descriptor set
		m_storageImageDescriptorSetLayout = DescriptorSetLayout::Builder(m_context)
			.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				VK_SHADER_STAGE_RAYGEN_BIT_KHR,
				1)
			.build();

		VkDescriptorImageInfo descriptorImageInfo;
		descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		descriptorImageInfo.imageView = m_sceneImage->getImageView();
		descriptorImageInfo.sampler = VK_NULL_HANDLE;

		m_descriptorAllocator->allocate(m_storageImageDescriptorSetLayout->getDescriptorSetLayout(), m_storageImageDescriptorSet);

		DescriptorWriter(m_context, *m_storageImageDescriptorSetLayout)
			.writeImage(0, &descriptorImageInfo)
			.updateSet(m_storageImageDescriptorSet);

		// Create blue noise indeces descriptor sets
		// TODO: separate blue noise descriptor set from textures descriptor set
		retrieveBlueNoiseTextureIndeces();

		m_blueNoiseDescriptorSetLayout = DescriptorSetLayout::Builder(m_context)
			.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 1)
			.build();

		m_descriptorAllocator->allocate(m_blueNoiseDescriptorSetLayout->getDescriptorSetLayout(), m_blueNoiseDescriptorSet);

		VkDeviceSize bufferSize = sizeof(m_blueNoiseTextureIndeces);

		Unique<VulkanBuffer> stagingBuffer = createUnique<VulkanBuffer>(
			m_context,
			bufferSize,
			1,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);
		stagingBuffer->map();
		stagingBuffer->writeToBuffer(m_blueNoiseTextureIndeces, bufferSize);
		stagingBuffer->unmap();

		m_blueNoiseIndecesBuffer = createUnique<VulkanBuffer>(
			m_context,
			bufferSize,
			1,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);

		m_context.copyBuffer(stagingBuffer->getBuffer(), m_blueNoiseIndecesBuffer->getBuffer(), bufferSize);

		auto bufferInfo = m_blueNoiseIndecesBuffer->descriptorInfo();

		DescriptorWriter(m_context, *m_blueNoiseDescriptorSetLayout)
			.writeBuffer(0, &bufferInfo)
			.updateSet(m_blueNoiseDescriptorSet);
	}

	void RayTracingRenderSystem::updateSceneImage(Shared<VulkanImage> sceneImage) {
		VkDescriptorImageInfo descriptorImageInfo;
		descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		descriptorImageInfo.imageView = sceneImage->getImageView();
		descriptorImageInfo.sampler = VK_NULL_HANDLE;

		DescriptorWriter(m_context, *m_storageImageDescriptorSetLayout)
			.writeImage(0, &descriptorImageInfo)
			.updateSet(m_storageImageDescriptorSet);
		
		m_sceneImage = sceneImage;
	}

	void RayTracingRenderSystem::defineShaderGroups() {
		m_shaderGroups = SHADER_GROUPS_VOL_PT;
	}

	void RayTracingRenderSystem::createPipelineLayout(DescriptorSetLayout& setLayout) {
		std::vector<VkDescriptorSetLayout> descriptorSetLayouts{
			setLayout.getDescriptorSetLayout(),
			m_rtSceneManager.getTLASDescriptorSetLayout(),
			m_textureRegistry.getDescriptorSetLayout(),
			m_storageImageDescriptorSetLayout->getDescriptorSetLayout(),
			m_materialRegistry.getDescriptorSetLayout(),
			m_skybox->getDescriptorSetLayout(),
			m_rtSceneManager.getMeshInstanceDescriptorSetLayout(),
			m_rtSceneManager.getEmittersDescriptorSetLayout(),
			m_rtSceneManager.getVolumeDescriptorSetLayout(),
			m_blueNoiseDescriptorSetLayout->getDescriptorSetLayout()
		};

		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(RayTracingPushConstantData);

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
		pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
		pipelineLayoutInfo.pushConstantRangeCount = 1; 
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

		if (vkCreatePipelineLayout(m_context.getDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create raytracingRenderSystem pipeline layout!");
		}
	}

	void RayTracingRenderSystem::createPipeline(bool useCompiledSpirvFiles) {
		RayTracingPipelineConfigInfo pipelineConfig{};
		pipelineConfig.shaderGroups = m_shaderGroups;
		pipelineConfig.pipelineLayout = m_pipelineLayout;
		pipelineConfig.maxPipelineRayRecursionDepth = 2; // for now

		const std::string baseShaderPath = useCompiledSpirvFiles ? SPV_SHADERS_PATH : SHADERS_PATH + "raytracing/";
		const std::string filenameSuffix = useCompiledSpirvFiles ? ".spv" : "";

		for (auto& group : pipelineConfig.shaderGroups) {
			for (auto& stage : group.stages) {
				stage.second = baseShaderPath + stage.second + filenameSuffix;
			}
		}

		m_pipeline = createUnique<Pipeline>(
			m_context,
			pipelineConfig
		);
	}

	// Helper function to align values
	inline uint32_t alignUp(uint32_t value, uint32_t alignment) {
		return (value + alignment - 1) & ~(alignment - 1);
	}

	void RayTracingRenderSystem::createShaderBindingTable() {
		// request phy device properties
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtPipelineProps = {};
		rtPipelineProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
		VkPhysicalDeviceProperties2 deviceProps2 = {};
		deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		deviceProps2.pNext = &rtPipelineProps;
		vkGetPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &deviceProps2);

		// --- 5. Create Shader Binding Table (SBT) ---
		// // Following Vulkan Docs, when we call vkCmdTraceRaysKHR, we have to pass three
		// VkStridedDeviceAddressRegionKHR structures. 
		// (https://registry.khronos.org/vulkan/specs/latest/man/html/vkCmdTraceRaysKHR.html#_c_specification)
		// In theory are just addresses so its up to us to decide logically what they are.
		// We decide to see them as regions (one for each shader group type:
		// raygen, miss, hit, callable) of the same SBT buffer.
		// In this context, when we say "SBT region", we mean a SBT buffer that contains
		// data for a specific shader group type (raygen, miss, hit, callable).
		// Inside a SBT region, we have a number of shader groups which represent SBT entries.
		// 
		// There are a lot of other options and stuff to consider when creating the SBT,
		// especially in how we assign every shader (most difficult is hit group) to
		// each geometry inside BLAS instances. For now everything uses same shader.
		// 
		// When creating the SBT, we need to know the size of the shader group handles and
		// their alignment in GPU memory.
		// The handle size is the size of a single shader group handle (example: 32 bytes)
		uint32_t handleSize = rtPipelineProps.shaderGroupHandleSize;
		// The HandleAlignment is the required alignment for a shader group handle inside a SBT region.
		// For example if we have two Raygen groups in the Raygen region,
		// handleSize = 32 bytes, and alignment = 64 bytes:
		// --> we would have to add an extra 32 bytes of padding to the first handle,
		//     so that the second handle starts at 64 bytes (we dont need to pad the last handle,
		//     as it is the last one in the region, it will be padded when aligning the whole region)
		uint32_t handleAlignment = rtPipelineProps.shaderGroupHandleAlignment;
		// The base alignment is the required alignment for the start of a SBT region.
		// For example, if we have a Raygen region with 2 groups, the base alignment is 90 bytes
		// while the handleAlignment is 64 bytes:
		// --> the first group will be aligned to 64 bytes (32 + 32 padding),
		//     and the second group will be aligned to 128 bytes (64 last group padded + 32 + 32 padding)
		//	   and then lastly the entire region will be aligned to 180 bytes
		//     (128 last aligned group + 52 padding)
		uint32_t baseAlignment = rtPipelineProps.shaderGroupBaseAlignment;

		// So that we have it already aligned for each handle
		uint32_t handleSizeAligned = alignUp(handleSize, handleAlignment);

		uint8_t rayGenGroupsCount = 0;
		uint8_t missGroupsCount = 0;
		uint8_t hitGroupsCount = 0;
		uint8_t callableGroupsCount = 0;

		// Get types from ShaderGroupsInfo
		for (const auto& group : m_shaderGroups) {
			switch (group.stages[0].first) {
				case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
					rayGenGroupsCount++; // Only one raygen group
					break;
				case VK_SHADER_STAGE_MISS_BIT_KHR:
					missGroupsCount++;
					break;
				case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
				case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
				case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
					hitGroupsCount++;
					break;
				case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
					callableGroupsCount++;
					break;
			}
		}

		uint32_t rayGenSectionSize = alignUp(handleSizeAligned * rayGenGroupsCount, baseAlignment);
		uint32_t missSectionSize = alignUp(handleSizeAligned * missGroupsCount, baseAlignment);
		uint32_t hitSectionSize = alignUp(handleSizeAligned * hitGroupsCount, baseAlignment);
		uint32_t callableSectionSize = 0; // No callable shaders for now

		uint32_t sbtSize = rayGenSectionSize + missSectionSize + hitSectionSize + callableSectionSize;

		// Get Shader Group Handles
		uint32_t numGroups = static_cast<uint32_t>(m_shaderGroups.size());

		// Fetch raw handles
		// We need to do this because the handles returned are tightly packed
		// so we need to copy them in a buffer accounting for the alignment
		uint32_t rawHandlesDataSize = numGroups * handleSize;
		std::vector<uint8_t> rawHandles(rawHandlesDataSize);
		if (vkGetRayTracingShaderGroupHandlesKHR(m_context.getDevice(), m_pipeline->getHandle(), 0, numGroups, rawHandlesDataSize, rawHandles.data()) != VK_SUCCESS) {
			throw std::runtime_error("Failed to get ray tracing shader group handles!");
		}

		// Prepare staging buffer data
		std::vector<uint8_t> sbtBufferData(sbtSize); // Zero initialized
		uintptr_t currentOffset = 0;
		uint32_t handleIdx = 0;

		// Copy RayGen handles
		for (uint8_t i = 0; i < rayGenGroupsCount; ++i) {
			memcpy(sbtBufferData.data() + currentOffset, rawHandles.data() + handleIdx * handleSize, handleSize);
			currentOffset += handleSizeAligned;
			handleIdx++;
		}
		currentOffset = rayGenSectionSize; // Move to the start of the miss section

		// Copy Miss handles
		for (uint8_t i = 0; i < missGroupsCount; ++i) {
			memcpy(sbtBufferData.data() + currentOffset, rawHandles.data() + handleIdx * handleSize, handleSize);
			currentOffset += handleSizeAligned;
			handleIdx++;
		}
		currentOffset = rayGenSectionSize + missSectionSize; // Move to hit

		// Copy Hit handles
		for (uint8_t i = 0; i < hitGroupsCount; ++i) {
			memcpy(sbtBufferData.data() + currentOffset, rawHandles.data() + handleIdx * handleSize, handleSize);
			currentOffset += handleSizeAligned;
			handleIdx++;
		}

		VulkanBuffer stagingBuffer{
			m_context,
			sbtSize,
			1,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		};

		stagingBuffer.map();
		stagingBuffer.writeToBuffer(sbtBufferData.data(), sbtSize);
		stagingBuffer.unmap();

		// Create final SBT buffer on GPU
		m_sbtBuffer = createUnique<VulkanBuffer>(
			m_context,
			sbtSize,
			1,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);

		m_context.copyBuffer(stagingBuffer.getBuffer(), m_sbtBuffer->getBuffer(), sbtSize);

		// Get SBT buffer device address, start of SBT
		VkDeviceAddress sbtAddress = m_sbtBuffer->getDeviceAddress();

		// Define SBT Regions for vkCmdTraceRaysKHR
		// (https://docs.vulkan.org/spec/latest/chapters/raytracing.html#shader-binding-table-indexing-rules)
		m_raygenRegion.deviceAddress = sbtAddress;
		m_raygenRegion.stride = rayGenSectionSize; // Stride equals the section size for raygen section
		m_raygenRegion.size = rayGenSectionSize;   // (https://docs.vulkan.org/spec/latest/chapters/raytracing.html#_ray_generation_shaders)

		m_missRegion.deviceAddress = sbtAddress + rayGenSectionSize;
		m_missRegion.stride = handleSizeAligned; // Stride between multiple miss shaders (if any)
		m_missRegion.size = missSectionSize;

		m_hitRegion.deviceAddress = sbtAddress + rayGenSectionSize + missSectionSize;
		m_hitRegion.stride = handleSizeAligned; // Stride between multiple hit groups (if any)
		m_hitRegion.size = hitSectionSize;

		m_callableRegion.deviceAddress = 0; // No callable shaders
		m_callableRegion.stride = 0;
		m_callableRegion.size = 0;
	}

	void RayTracingRenderSystem::retrieveBlueNoiseTextureIndeces() {
		std::string blueNoiseFile;
		for (uint32_t i = 0; i < BLUE_NOISE_TEXTURE_COUNT; ++i) {
			blueNoiseFile = BLUE_NOISE_FILE + std::to_string(i) + BLUE_NOISE_FILE_EXT;
			m_blueNoiseTextureIndeces[i] = m_textureRegistry.getIndex(blueNoiseFile);
		}
	}
	
	void RayTracingRenderSystem::update(FrameInfo& frameInfo) {
		m_rtSceneManager.createTLAS(frameInfo);

		m_sceneImage->transitionImageLayout(
			frameInfo.commandBuffer,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR
		);
	}



	void RayTracingRenderSystem::render(FrameInfo& frameInfo, Renderer& renderer) {
		m_pipeline->bind(frameInfo.commandBuffer);

		std::array<VkDescriptorSet, 10> descriptorSets = { 
			frameInfo.globalDescriptorSet, 
			m_rtSceneManager.getTLASDescriptorSet(), 
			m_textureRegistry.getDescriptorSet(),
			m_storageImageDescriptorSet,
			m_materialRegistry.getDescriptorSet(),
			m_skybox->getDescriptorSet(),
			m_rtSceneManager.getMeshInstanceDescriptorSet(),
			m_rtSceneManager.getEmittersDescriptorSet(),
			m_rtSceneManager.getVolumeDescriptorSet(),
			m_blueNoiseDescriptorSet
		};
	
		vkCmdBindDescriptorSets(
			frameInfo.commandBuffer,
			VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
			m_pipelineLayout,
			0,
			static_cast<uint32_t>(descriptorSets.size()),
			descriptorSets.data(),
			0,
			nullptr
		);

		// Push constants for blue noise texture index
		RayTracingPushConstantData pushConstants;
		pushConstants.noiseType = m_noiseType;
		pushConstants.blueNoiseTextureCount = BLUE_NOISE_TEXTURE_COUNT;
		pushConstants.blueNoiseTextureSize = BLUE_NOISE_TEXTURE_SIZE;
		pushConstants.selectSingleTextures = m_selectSingleBlueNoiseTextures;
		pushConstants.blueNoiseDebugIndex = m_blueNoiseDebugIndex;

		pushConstants.metalness = m_metalness;
		pushConstants.roughness = m_roughness;
		pushConstants.transmission = m_transmission;
		pushConstants.ior = m_ior;
		pushConstants.albedo = m_albedo;

		vkCmdPushConstants(
			frameInfo.commandBuffer,
			m_pipelineLayout,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
			0,
			sizeof(RayTracingPushConstantData),
			&pushConstants
		);

		vkCmdTraceRaysKHR(
			frameInfo.commandBuffer,
			&m_raygenRegion,
			&m_missRegion,
			&m_hitRegion,
			&m_callableRegion,
			renderer.getSwapChainExtent().width,
			renderer.getSwapChainExtent().height,
			1
		);
	}

	void RayTracingRenderSystem::transitionImageToShaderReadOnlyOptimal(FrameInfo& frameInfo, VkPipelineStageFlagBits lastStage) {
		// transition output image to shader read only layout for imgui
		m_sceneImage->transitionImageLayout(
			frameInfo.commandBuffer,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			lastStage,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
		);
	}

	void RayTracingRenderSystem::reloadShaders() {
		PXT_INFO("Reloading shaders...");

		createPipeline(false);
		createShaderBindingTable();
	}

	void RayTracingRenderSystem::updateUi() {
		ImGui::InputInt("Noise Type (0 -> white, 1 -> blue noise)", reinterpret_cast<int*>(&m_noiseType));
		if (m_noiseType == 1) {
			ImGui::Text("Blue Noise is currently only used in jitter\nand still doesn't work properly (most probably)");
			ImGui::Checkbox("Select Single Blue Noise Textures (for Debug)", reinterpret_cast<bool*>(&m_selectSingleBlueNoiseTextures));
			if (m_selectSingleBlueNoiseTextures) {
				std::string inputMessage = "Blue Noise Texture Index (0 to " + std::to_string(BLUE_NOISE_TEXTURE_COUNT - 1) + ")";
				ImGui::InputInt(inputMessage.c_str(), reinterpret_cast<int*>(&m_blueNoiseDebugIndex));
			}
		}

		ImGui::SeparatorText("Material Properties Debug (applied to id = 0 object)");
		ImGui::SliderFloat("Metalness", &m_metalness, 0.0f, 1.0f, "%.5f");
		ImGui::SliderFloat("Roughness", &m_roughness, 0.0001f, 1.0f, "%.5f");
		ImGui::SliderFloat("Transmission", &m_transmission, 0.0f, 1.0f);
		ImGui::SliderFloat("Ior", &m_ior, 1.0f, 3.0f, "%.5f");
		ImGui::ColorEdit3("Albedo", &m_albedo.x, ImGuiColorEditFlags_Float);
	}
}