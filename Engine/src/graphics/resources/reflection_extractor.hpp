#pragma once

#include "core/pch.hpp"
#include "graphics/resources/shader_reflection.hpp"

#include <slang-com-ptr.h>
#include <slang.h>

namespace pxt {

    /**
     * @brief Utility class for extracting shader reflection metadata from Slang
     *
     * ReflectionExtractor traverses Slang's reflection API structures and populates
     * ShaderReflection objects with descriptor bindings, push constants, vertex
     * attributes, and uniform buffer layouts.
     *
     * This class uses static methods and maintains no state. It handles type mapping
     * from Slang types to Vulkan types and provides comprehensive error reporting
     * for unmapped or unsupported types.
     */
    class ReflectionExtractor {
    public:
        /**
         * @brief Extract shader reflection metadata from a compiled Slang program
         *
         * Traverses the Slang program's reflection data and extracts all relevant
         * metadata including descriptor bindings, push constants, vertex attributes,
         * and uniform buffer layouts.
         *
         * @param program The compiled Slang program component
         * @param entryPointIndex Index of the entry point to extract reflection from
         * @param targetIndex Index of the compilation target (usually 0 for single target)
         * @return Shared pointer to populated ShaderReflection, or nullptr on failure
         */
        static Shared<ShaderReflection> extract(slang::IComponentType* program, uint32_t entryPointIndex,
                                                uint32_t targetIndex);

    private:
        /**
         * @brief Extract descriptor bindings from program layout
         *
         * Traverses the Slang program layout to find all descriptor set bindings,
         * extracting set number, binding number, descriptor type, count, and name.
         *
         * @param layout The Slang program layout containing reflection data
         * @param reflection The ShaderReflection object to populate
         */
        static void extractDescriptorBindings(slang::ProgramLayout* layout, ShaderReflection& reflection);

        /**
         * @brief Extract push constant ranges from program layout
         *
         * Traverses the Slang program layout to find push constant blocks and
         * extracts their offset, size, and shader stage information.
         *
         * @param layout The Slang program layout containing reflection data
         * @param reflection The ShaderReflection object to populate
         */
        static void extractPushConstants(slang::ProgramLayout* layout, ShaderReflection& reflection);

        /**
         * @brief Extract vertex input attributes from entry point
         *
         * Traverses the Slang entry point reflection to find vertex input parameters
         * and extracts their location, format, offset, and name.
         *
         * @param entryPoint The Slang entry point reflection
         * @param reflection The ShaderReflection object to populate
         */
        static void extractVertexAttributes(slang::EntryPointReflection* entryPoint, ShaderReflection& reflection);

        /**
         * @brief Extract uniform buffer member layouts
         *
         * Traverses Slang reflection to find uniform buffer declarations and
         * extracts member names, offsets, sizes, and types.
         *
         * @param layout The Slang program layout containing reflection data
         * @param reflection The ShaderReflection object to populate
         */
        static void extractUniformBuffers(slang::ProgramLayout* layout, ShaderReflection& reflection);

        /**
         * @brief Map Slang resource type to Vulkan descriptor type
         *
         * Converts Slang resource types (ConstantBuffer, StructuredBuffer, Texture, etc.)
         * to their corresponding VkDescriptorType values.
         *
         * @param type The Slang type reflection to map
         * @return Corresponding VkDescriptorType, or VK_DESCRIPTOR_TYPE_MAX_ENUM if unmapped
         */
        static VkDescriptorType mapSlangResourceToVkDescriptorType(slang::TypeReflection* type);

        /**
         * @brief Map Slang scalar type to Vulkan format
         *
         * Converts Slang scalar types (float, float2, int, uint, etc.) to their
         * corresponding VkFormat values for vertex attributes and uniform members.
         *
         * @param type The Slang type reflection to map
         * @return Corresponding VkFormat, or VK_FORMAT_UNDEFINED if unmapped
         */
        static VkFormat mapSlangTypeToVkFormat(slang::TypeReflection* type);

        /**
         * @brief Recursively traverse variable layout parameters
         *
         * Traverses nested variable layouts (structs, arrays) and invokes a callback
         * for each descriptor binding found.
         *
         * @param varLayout The variable layout to traverse
         * @param callback Function to call for each descriptor binding found
         */
        static void traverseParameters(slang::VariableLayoutReflection* varLayout,
                                       std::function<void(const DescriptorBinding&)> callback);

        /**
         * @brief Get Vulkan shader stage flag from Slang stage
         *
         * Converts Slang stage enumeration to corresponding VkShaderStageFlagBits.
         *
         * @param stage The Slang stage enumeration
         * @return Corresponding VkShaderStageFlagBits
         */
        static VkShaderStageFlagBits mapSlangStageToVkStage(SlangStage stage);
    };

} // namespace pxt
