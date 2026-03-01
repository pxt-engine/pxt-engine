#pragma once

#include "graphics/descriptors/descriptor_allocator.hpp"
#include "graphics/descriptors/descriptor_pool.hpp"
#include "graphics/descriptors/descriptor_set_layout.hpp"
#include "graphics/descriptors/descriptor_writer.hpp"

namespace pxt {
    /*
     * @brief A struct that represents a single descriptor binding entry, used for building descriptor set layouts and
     * writing to descriptor sets.
	*/
	struct DescriptorEntry {
		uint32_t binding;
		VkDescriptorType descriptorType;
		VkShaderStageFlags stageFlags;
		uint32_t count;
    };
}