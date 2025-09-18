#pragma once

#include "core/pch.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/swap_chain.hpp"
#include "graphics/frame_info.hpp"
#include "graphics/descriptors/descriptors.hpp"
#include "graphics/resources/texture_registry.hpp"
#include "graphics/resources/material_registry.hpp"
#include "graphics/resources/vk_skybox.hpp"
#include "graphics/render_systems/raytracing_scene_manager_system.hpp"
#include "graphics/renderer.hpp"
#include "scene/scene.hpp"
#include "scene/environment.hpp"

namespace PXTEngine {

    class RayTracingRenderSystem {
    public:
        RayTracingRenderSystem(Context& context, Shared<DescriptorAllocatorGrowable> descriptorAllocator, TextureRegistry& textureRegistry, MaterialRegistry& materialRegistry, BLASRegistry& blasRegistry, Shared<Environment> environment, DescriptorSetLayout& globalSetLayout, Shared<VulkanImage> sceneImage);
        ~RayTracingRenderSystem();

        RayTracingRenderSystem(const RayTracingRenderSystem&) = delete;
        RayTracingRenderSystem& operator=(const RayTracingRenderSystem&) = delete;

        void update(FrameInfo& frameInfo);
        void render(FrameInfo& frameInfo, Renderer& renderer);
		void transitionImageToShaderReadOnlyOptimal(FrameInfo& frameInfo, VkPipelineStageFlagBits lastStage);
		void reloadShaders();

		void updateUi();

        void updateSceneImage(Shared<VulkanImage> sceneImage);

    private:
		void createDescriptorSets();
		void defineShaderGroups();
        void createPipelineLayout(DescriptorSetLayout& setLayout);
        void createPipeline(bool useCompiledSpirvFiles = true);
		void createShaderBindingTable();

		void retrieveBlueNoiseTextureIndeces();

        Context& m_context;
        TextureRegistry& m_textureRegistry;
		MaterialRegistry& m_materialRegistry;
		BLASRegistry& m_blasRegistry; // used only to initialize tlasBuildSystem
		Shared<Environment> m_environment = nullptr;
		Shared<VulkanSkybox> m_skybox = nullptr;
        
        Shared<DescriptorAllocatorGrowable> m_descriptorAllocator = nullptr;
        
        RayTracingSceneManagerSystem m_rtSceneManager{m_context, m_materialRegistry, m_blasRegistry, m_textureRegistry, m_descriptorAllocator};

        Unique<Pipeline> m_pipeline;
        VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

        std::vector<ShaderGroupInfo> m_shaderGroups{};
        Unique<VulkanBuffer> m_sbtBuffer = nullptr;
        VkStridedDeviceAddressRegionKHR m_raygenRegion;
        VkStridedDeviceAddressRegionKHR m_missRegion;
        VkStridedDeviceAddressRegionKHR m_hitRegion;
        VkStridedDeviceAddressRegionKHR m_callableRegion; // empty for now

        Shared<VulkanImage> m_sceneImage = nullptr;
		VkDescriptorSet m_storageImageDescriptorSet = VK_NULL_HANDLE;
		Unique<DescriptorSetLayout> m_storageImageDescriptorSetLayout = nullptr;

		// Blue noise textures
		VkDescriptorSet m_blueNoiseDescriptorSet = VK_NULL_HANDLE;
		Unique<DescriptorSetLayout> m_blueNoiseDescriptorSetLayout = nullptr;
		Unique<VulkanBuffer> m_blueNoiseIndecesBuffer = nullptr;

		// Ui variables
		uint32_t m_noiseType = 0; // 0 for white noise, 1 for blue noise
		uint32_t m_blueNoiseTextureIndeces[BLUE_NOISE_TEXTURE_COUNT]; // Indices of the blue noise textures in the texture registry
		uint32_t m_blueNoiseDebugIndex = 0; // Index of the blue noise texture to use
		VkBool32 m_selectSingleBlueNoiseTextures = VK_FALSE; // Whether to select single textures or use different blue noise textures every frame
		
		const std::vector<ShaderGroupInfo> SHADER_GROUPS_BASIC = {
			// General RayGen Group
			{
				VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
				{
				// Shader stages + filepaths
				// only one shader stage for raygen is permitted
				{VK_SHADER_STAGE_RAYGEN_BIT_KHR, "primary.rgen"}
			}
		},
			// Main Miss Group
			{
				VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
				{
				// Shader stages + filepaths
				// here we can have multiple miss shaders
				{VK_SHADER_STAGE_MISS_BIT_KHR, "primary.rmiss"}
			}
		},
			// Shadow Miss Group
			{
				VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
				{
				// Shader stages + filepaths
				// here we can have multiple miss shaders
				{VK_SHADER_STAGE_MISS_BIT_KHR, "shadow.rmiss"}
			}
		},
			// Closest Hit Group (Triangle Hit Group)
			{
				VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
				{
				// Shader stages + filepaths
				// here there can be a chit, ahit or intersection shader (every combination of these)
				{VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "primary.rchit"}
			}
		},
		};

		const std::vector<ShaderGroupInfo> SHADER_GROUPS_PT = {
				// General RayGen Group
				{
					VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
					{
					// Shader stages + filepaths
					// only one shader stage for raygen is permitted
					{VK_SHADER_STAGE_RAYGEN_BIT_KHR, "pathtracing.rgen"}
				}
			},
				// General Miss Group
				{
					VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
					{
					// Shader stages + filepaths
					// here we can have multiple miss shaders
					{VK_SHADER_STAGE_MISS_BIT_KHR, "pathtracing.rmiss"}
				}
			},
				// Visibility Miss Group
				{
					VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
					{
					// Shader stages + filepaths
					// here we can have multiple miss shaders
					{VK_SHADER_STAGE_MISS_BIT_KHR, "visibility.rmiss"}
				}
			},
				// Distance Miss Group
				{
					VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
					{
					// Shader stages + filepaths
					// here we can have multiple miss shaders
					{VK_SHADER_STAGE_MISS_BIT_KHR, "distance.rmiss"}
				}
			},
				// Closest Hit Group (Triangle Hit Group)
				{
					VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
					{
					// Shader stages + filepaths
					// here there can be a chit, ahit or intersection shader (every combination of these)
					{VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "pathtracing.rchit"}
				}
			},
				// Closest Hit Group (Visibility Hit Group)
				{
					VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
					{
					// Shader stages + filepaths
					// here there can be a chit, ahit or intersection shader (every combination of these)
					{VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "visibility.rchit"}
				}
			},
				// Closest Hit Group (Distance Hit Group)
				{
					VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
					{
					// Shader stages + filepaths
					// here there can be a chit, ahit or intersection shader (every combination of these)
					{VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "distance.rchit"}
				}
			},
		};

		const std::vector<ShaderGroupInfo> SHADER_GROUPS_VOL_PT = {
				// General RayGen Group
				{
					VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
					{
					// Shader stages + filepaths
					// only one shader stage for raygen is permitted
					{VK_SHADER_STAGE_RAYGEN_BIT_KHR, "vol_pathtracing.rgen"}
				}
			},
				// General Miss Group
				{
					VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
					{
					// Shader stages + filepaths
					// here we can have multiple miss shaders
					{VK_SHADER_STAGE_MISS_BIT_KHR, "vol_pathtracing.rmiss"}
				}
			},
				// Visibility Miss Group
				{
					VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
					{
					// Shader stages + filepaths
					// here we can have multiple miss shaders
					{VK_SHADER_STAGE_MISS_BIT_KHR, "visibility.rmiss"}
				}
			},
				// Distance Miss Group
				{
					VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
					{
					// Shader stages + filepaths
					// here we can have multiple miss shaders
					{VK_SHADER_STAGE_MISS_BIT_KHR, "distance.rmiss"}
				}
			},
				// Closest Hit Group (Triangle Hit Group)
				{
					VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
					{
					// Shader stages + filepaths
					// here there can be a chit, ahit or intersection shader (every combination of these)
					{VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "vol_pathtracing.rchit"}
				}
			},
				// Closest Hit Group (Visibility Hit Group)
				{
					VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
					{
					// Shader stages + filepaths
					// here there can be a chit, ahit or intersection shader (every combination of these)
					{VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "visibility.rchit"}
				}
			},
				// Closest Hit Group (Distance Hit Group)
				{
					VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
					{
					// Shader stages + filepaths
					// here there can be a chit, ahit or intersection shader (every combination of these)
					{VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "distance.rchit"}
				}
			},
		};
    };
}