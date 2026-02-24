#pragma once

#include "core/pch.hpp"
#include "graphics/descriptors/descriptor_pool.hpp"
#include "graphics/descriptors/descriptor_set_layout.hpp"

namespace pxt {
    class DescriptorWriter {
    public:
        explicit DescriptorWriter(Context& context);

        /**
         * @brief Writes a single buffer descriptor to a destination descriptor set.
         *
         * @param setLayout Descriptor set layout describing the binding.
         * @param dstSet Destination descriptor set to write to.
         * @param binding The binding index.
         * @param bufferInfo Pointer to the buffer descriptor info.
         *
         * @return Reference to the DescriptorWriter instance.
         */
        DescriptorWriter& writeBuffer(DescriptorSetLayout& setLayout, VkDescriptorSet dstSet, uint32_t binding,
                                      VkDescriptorBufferInfo* bufferInfo) {
            return write(setLayout, dstSet, binding, bufferInfo, 1);
        }

        /**
         * @brief Writes multiple buffer descriptors to a destination descriptor set.
         */
        DescriptorWriter& writeBuffers(DescriptorSetLayout& setLayout, VkDescriptorSet dstSet, uint32_t binding,
                                       VkDescriptorBufferInfo* buffersInfo, uint32_t count) {
            return write(setLayout, dstSet, binding, buffersInfo, count);
        }

        /**
         * @brief Writes a single image descriptor to a destination descriptor set.
         */
        DescriptorWriter& writeImage(DescriptorSetLayout& setLayout, VkDescriptorSet dstSet, uint32_t binding,
                                     VkDescriptorImageInfo* imageInfo) {
            return write(setLayout, dstSet, binding, imageInfo, 1);
        }

        /**
         * @brief Writes multiple image descriptors to a destination descriptor set.
         */
        DescriptorWriter& writeImages(DescriptorSetLayout& setLayout, VkDescriptorSet dstSet, uint32_t binding,
                                      VkDescriptorImageInfo* imagesInfo, uint32_t count) {
            return write(setLayout, dstSet, binding, imagesInfo, count);
        }

        /**
         * @brief Writes a single acceleration structure descriptor to a destination descriptor set.
         */
        DescriptorWriter& writeTLAS(DescriptorSetLayout& setLayout, VkDescriptorSet dstSet, uint32_t binding,
                                    VkWriteDescriptorSetAccelerationStructureKHR writeInfo) {
            return write(setLayout, dstSet, binding, &writeInfo, 1);
        }

        /**
         * @brief Generic helper for recording descriptor writes.
         *
         * @tparam T Descriptor info type.
         * @param setLayout Descriptor set layout describing the binding.
         * @param dstSet Destination descriptor set.
         * @param binding Binding index.
         * @param info Pointer to descriptor info.
         * @param count Number of descriptors.
         */
        template <typename T>
        DescriptorWriter& write(DescriptorSetLayout& setLayout, VkDescriptorSet dstSet, uint32_t binding, T* info,
                                uint32_t count) {
            size_t bindingCount = setLayout.m_bindings.count(binding);
            PXT_ASSERT(bindingCount == 1, "Layout does not contain specified binding");

            auto& bindingDescription = setLayout.m_bindings[binding];
            PXT_ASSERT(bindingDescription.descriptorCount == count, "Binding descriptor info count mismatch");

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = dstSet;
            write.dstBinding = binding;
            write.descriptorType = bindingDescription.descriptorType;
            write.descriptorCount = count;

            if constexpr (std::is_same_v<T, VkDescriptorBufferInfo>) {
                write.pBufferInfo = info;
            } else if constexpr (std::is_same_v<T, VkDescriptorImageInfo>) {
                write.pImageInfo = info;
            } else if constexpr (std::is_same_v<T, VkWriteDescriptorSetAccelerationStructureKHR>) {
                write.pNext = info;
            } else {
                PXT_STATIC_ASSERT(false, "Unsupported type for descriptor write");
            }

            m_writes.push_back(write);
            return *this;
        }

        /**
         * @brief Applies all accumulated descriptor writes to their respective destination sets.
         *
         * This issues a single vkUpdateDescriptorSets call covering all recorded writes.
         * The caller is responsible for ensuring descriptor sets are not in use.
         */
        void updateAll();

    private:
        Context& m_context;
        std::vector<VkWriteDescriptorSet> m_writes;
    };
} // namespace pxt
