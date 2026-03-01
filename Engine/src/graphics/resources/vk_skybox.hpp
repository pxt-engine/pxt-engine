#pragma once

#include "core/pch.hpp"
#include "graphics/descriptors/descriptors.hpp"
#include "graphics/descriptors/descriptor_manager.hpp"
#include "graphics/resources/cube_map.hpp"
#include "graphics/swap_chain.hpp"
#include "resources/resource_manager.hpp"
#include "scene/skybox.hpp"

namespace pxt {

    class VulkanSkybox : public Skybox {
    public:
        static Unique<VulkanSkybox> create(const std::array<std::string, 6>& paths);

        VulkanSkybox(Context& context, ResourceManager& rm, DescriptorManager& descriptorManager, const std::array<std::string, 6>& paths);

        ~VulkanSkybox() override = default;

        void createDescriptorSet();

        VkDescriptorImageInfo getDescriptorImageInfo() const;

        VkDescriptorSet getDescriptorSet(uint32_t frameIndex) const { return m_descriptorManager.getDescriptorSet(m_skyboxDescriptorSet, frameIndex); }

        VkDescriptorSet getDebugDescriptorSet(uint32_t faceIndex, uint32_t frameIndex) const {
            return m_descriptorManager.getDescriptorSet(m_skyboxDebugDescriptorSets[faceIndex], frameIndex);
        }

        VkDescriptorSetLayout getDescriptorSetLayout() const { 
            return m_descriptorManager.getLayout(m_skyboxDescriptorSet).getHandle();
        }

        const CubeMap& getCubeMap() const { return *m_cubeMap; }

        void replace(const std::array<std::string, 6>& skyboxTextures) override;

        void updateDescriptorSets();

    private:
        void loadTextures(const std::array<std::string, 6>& paths, ResourceManager& rm);

        Context& m_context;
        DescriptorManager& m_descriptorManager;

        uint32_t m_size = 0;

        // TODO: maybe just an id and then we use resource manager to get the cubemap???
        Shared<CubeMap> m_cubeMap;

        DescriptorSetHandle m_skyboxDescriptorSet = core::UID::s_invalidId;

        std::array<DescriptorSetHandle, 6> m_skyboxDebugDescriptorSets;
    };
} // namespace pxt