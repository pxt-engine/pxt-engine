#pragma once

#include "core/pch.hpp"
#include "graphics/descriptors/descriptors.hpp"
#include "graphics/resources/cube_map.hpp"
#include "scene/skybox.hpp"

namespace pxt {

    class VulkanSkybox : public Skybox {
    public:
        static Unique<VulkanSkybox> create(const std::array<std::string, 6>& paths);

        VulkanSkybox(Context& context, const std::array<std::string, 6>& paths);

        ~VulkanSkybox() override = default;

        void createDescriptorSet(DescriptorAllocatorGrowable& descriptorAllocator);

        VkDescriptorImageInfo getDescriptorImageInfo() const;

        VkDescriptorSet getDescriptorSet() const { return m_skyboxDescriptorSet; }

        VkDescriptorSet getDebugDescriptorSet(uint32_t faceIndex) const {
            return m_skyboxDebugDescriptorSets[faceIndex];
        }

        VkDescriptorSetLayout getDescriptorSetLayout() const {
            return m_skyboxDescriptorSetLayout->getDescriptorSetLayout();
        }

    private:
        void loadTextures(const std::array<std::string, 6>& paths);

        Context& m_context;

        uint32_t m_size = 0;
        Unique<CubeMap> m_cubeMap;

        VkDescriptorSet m_skyboxDescriptorSet;
        Unique<DescriptorSetLayout> m_skyboxDescriptorSetLayout;

        // TODO: move these to a descriptor set manager or something
        std::array<VkDescriptorSet, 6> m_skyboxDebugDescriptorSets;
        Unique<DescriptorSetLayout> m_skyboxDebugDescriptorSetLayout;
    };
} // namespace pxt