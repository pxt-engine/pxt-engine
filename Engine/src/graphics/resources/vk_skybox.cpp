#include "graphics/resources/vk_skybox.hpp"

#include "application.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <stdexcept>

namespace pxt {
    Unique<VulkanSkybox> VulkanSkybox::create(const std::array<std::string, 6>& paths) {
        Context& context = Application::get().getContext();
        ResourceManager& rm = Application::get().getResourceManager();

        return createUnique<VulkanSkybox>(context, rm, paths);
    }

    VulkanSkybox::VulkanSkybox(Context& context, ResourceManager& rm, const std::array<std::string, 6>& paths)
        : m_context(context) {
        // Load the skybox textures from the provided faces
        loadTextures(paths, rm);
    }

    void VulkanSkybox::loadTextures(const std::array<std::string, 6>& paths, ResourceManager& rm) {
        VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
        int width, height, channels;
        uint8_t* pixels[6] = {nullptr};

        for (int i = 0; i < 6; ++i) {
            pixels[i] = stbi_load(paths[i].c_str(), &width, &height, &channels, STBI_rgb_alpha);

            if (!pixels[i]) {
                // Cleanup previously loaded images before throwing
                for (int j = 0; j < i; ++j) {
                    if (pixels[j])
                        stbi_image_free(pixels[j]);
                }
                throw std::runtime_error("Failed to load skybox texture face: " + paths[i] + " - " +
                                         stbi_failure_reason());
            }

            if (i == 0) {
                m_size = static_cast<uint32_t>(width);
                if (width != height) {

                    // Cleanup
                    for (int j = 0; j <= i; ++j)
                        if (pixels[j])
                            stbi_image_free(pixels[j]);

                    throw std::runtime_error("Skybox faces must be square. Face 0 (" + paths[0] + ") is " +
                                             std::to_string(width) + "x" + std::to_string(height));
                }
            } else {
                if (static_cast<uint32_t>(width) != m_size || static_cast<uint32_t>(height) != m_size) {

                    // Cleanup
                    for (int j = 0; j <= i; ++j)
                        if (pixels[j])
                            stbi_image_free(pixels[j]);

                    throw std::runtime_error("Skybox faces must have consistent dimensions. Face " + std::to_string(i) +
                                             " (" + paths[i] + ") is " + std::to_string(width) + "x" +
                                             std::to_string(height) + ", expected " + std::to_string(m_size) + "x" +
                                             std::to_string(m_size));
                }
            }
        }

        VkDeviceSize faceImageSizes = m_size * m_size * 4;
        VkDeviceSize totalImageSize = faceImageSizes * 6;

        m_cubeMap = createShared<CubeMap>(m_context, m_size, format,
                                          VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        // add cubemap resource to resource manager
        // TODO: better skybox resource management, this is really just a hack to get the cubemap into the resource
        // manager so it can be used by the debug system
        rm.add(m_cubeMap, paths[0] + ".cubemap");

        VulkanBuffer stagingBuffer(m_context, totalImageSize,
                                   1, // instance count
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        stagingBuffer.map();

        VkDeviceSize currentOffset = 0;
        for (int i = 0; i < 6; ++i) {
            stagingBuffer.writeToBuffer((void*)pixels[i], faceImageSizes, currentOffset);
            stbi_image_free(pixels[i]); // Free CPU-side image data
            pixels[i] = nullptr;        // Avoid double free
            currentOffset += faceImageSizes;
        }

        VkImageSubresourceRange cubemapSubresourceRange{};
        cubemapSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        cubemapSubresourceRange.baseMipLevel = 0;
        cubemapSubresourceRange.levelCount = 1;
        cubemapSubresourceRange.baseArrayLayer = 0;
        cubemapSubresourceRange.layerCount = 6;

        m_cubeMap->transitionImageLayoutSingleTimeCmd(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                      cubemapSubresourceRange);

        m_context.copyBufferToImage(stagingBuffer.getBuffer(), m_cubeMap->getVkImage(), m_size, m_size, 6);

        m_cubeMap->transitionImageLayoutSingleTimeCmd(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                      VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, cubemapSubresourceRange);
    }

    void VulkanSkybox::createDescriptorSet(DescriptorAllocatorGrowable& descriptorAllocator) {
        m_skyboxDescriptorSetLayout = DescriptorSetLayout::Builder(m_context)
                                          .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                      VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_MISS_BIT_KHR |
                                                          VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
                                          .build();

        // Get the VkDescriptorImageInfo from the Skybox object
        VkDescriptorImageInfo skyboxImageInfo = getDescriptorImageInfo();

        for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
            descriptorAllocator.allocate(m_skyboxDescriptorSetLayout->getDescriptorSetLayout(),
                                         m_skyboxDescriptorSet[i]);

            DescriptorWriter(m_context, *m_skyboxDescriptorSetLayout)
                .writeImage(0, &skyboxImageInfo)
                .updateSet(m_skyboxDescriptorSet[i]);
        }

        // Create debug descriptor sets for each face of the cube map
        m_skyboxDebugDescriptorSetLayout =
            DescriptorSetLayout::Builder(m_context)
                .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                .build();

        for (size_t i = 0; i < m_skyboxDebugDescriptorSets.size(); i++) {
            descriptorAllocator.allocate(m_skyboxDebugDescriptorSetLayout->getDescriptorSetLayout(),
                                         m_skyboxDebugDescriptorSets[i]);
            // Create a descriptor image info for the current face of the cube map
            VkDescriptorImageInfo debugImageInfo{};
            debugImageInfo.sampler = m_cubeMap->getSamplerHandle();
            debugImageInfo.imageView = m_cubeMap->getFaceImageView(i);
            debugImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            DescriptorWriter(m_context, *m_skyboxDebugDescriptorSetLayout)
                .writeImage(0, &debugImageInfo)
                .updateSet(m_skyboxDebugDescriptorSets[i]);
        }
    }

    VkDescriptorImageInfo VulkanSkybox::getDescriptorImageInfo() const {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = m_cubeMap->getSamplerHandle();
        imageInfo.imageView = m_cubeMap->getImageView();
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        return imageInfo;
    }

    void VulkanSkybox::updateDescriptorSets(uint32_t frameIndex) {
        // Main skybox descriptor
        VkDescriptorImageInfo skyboxImageInfo = getDescriptorImageInfo();

        DescriptorWriter(m_context, *m_skyboxDescriptorSetLayout)
            .writeImage(0, &skyboxImageInfo)
            .updateSet(m_skyboxDescriptorSet[frameIndex]);

        // Debug face descriptor sets
        for (size_t i = 0; i < m_skyboxDebugDescriptorSets.size(); i++) {
            VkDescriptorImageInfo debugImageInfo{};
            debugImageInfo.sampler = m_cubeMap->getSamplerHandle();
            debugImageInfo.imageView = m_cubeMap->getFaceImageView(i);
            debugImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            DescriptorWriter(m_context, *m_skyboxDebugDescriptorSetLayout)
                .writeImage(0, &debugImageInfo)
                .updateSet(m_skyboxDebugDescriptorSets[i]);
        }
    }

    void VulkanSkybox::replace(const std::array<std::string, 6>& skyboxTexturesPaths, uint32_t frameIndex) {
        // Ensure GPU is not using old resources
        vkDeviceWaitIdle(m_context.getDevice());

        // Reload textures (creates new CubeMap)
        loadTextures(skyboxTexturesPaths, Application::get().getResourceManager());
    }

} // namespace pxt