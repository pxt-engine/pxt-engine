#pragma once

#include "core/pch.hpp"
#include "graphics/descriptors/descriptors.hpp"
#include "graphics/resources/cube_map.hpp"
#include "graphics/swap_chain.hpp"
#include "resources/resource_manager.hpp"
#include "scene/skybox.hpp"

namespace pxt {

    class VulkanSkybox : public Skybox {
    public:
        static Unique<VulkanSkybox> create(const std::array<std::string, 6>& paths);

        VulkanSkybox(Context& context, ResourceManager& rm, const std::array<std::string, 6>& paths);

        ~VulkanSkybox() override = default;

        void createDescriptorSet(DescriptorAllocatorGrowable& descriptorAllocator);

        VkDescriptorImageInfo getDescriptorImageInfo() const;

        VkDescriptorSet getDescriptorSet(uint32_t frameIndex) const { return m_skyboxDescriptorSet[frameIndex]; }

        VkDescriptorSet getDebugDescriptorSet(uint32_t faceIndex) const {
            return m_skyboxDebugDescriptorSets[faceIndex];
        }

        VkDescriptorSetLayout getDescriptorSetLayout() const {
            return m_skyboxDescriptorSetLayout->getDescriptorSetLayout();
        }

        const CubeMap& getCubeMap() const { return *m_cubeMap; }

        void replace(const std::array<std::string, 6>& skyboxTextures) override;

        void updateDescriptorSets(uint32_t frameIndex);

    private:
        void loadTextures(const std::array<std::string, 6>& paths, ResourceManager& rm);

        Context& m_context;

        uint32_t m_size = 0;

        // TODO: maybe just an id and then we use resource manager to get the cubemap???
        Shared<CubeMap> m_cubeMap;

        std::array<VkDescriptorSet, SwapChain::MAX_FRAMES_IN_FLIGHT> m_skyboxDescriptorSet;
        Unique<DescriptorSetLayout> m_skyboxDescriptorSetLayout;

        // TODO: move these to a descriptor set manager or something
        std::array<VkDescriptorSet, 6> m_skyboxDebugDescriptorSets;
        Unique<DescriptorSetLayout> m_skyboxDebugDescriptorSetLayout;
    };
} // namespace pxt