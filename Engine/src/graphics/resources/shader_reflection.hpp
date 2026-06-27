#pragma once

#include "core/pch.hpp"

namespace pxt {

    /**
     * @brief Descriptor binding information extracted from shader reflection
     *
     * Represents a single descriptor binding with its set number, binding number,
     * type, count, shader stages, and name.
     */
    struct DescriptorBinding {
        uint32_t set;              // Descriptor set number
        uint32_t binding;          // Binding number within the set
        VkDescriptorType type;     // Vulkan descriptor type
        uint32_t count;            // Number of descriptors (for arrays)
        VkShaderStageFlags stages; // Shader stages that use this binding
        std::string name;          // Binding name from shader source
    };

    /**
     * @brief Push constant range information extracted from shader reflection
     *
     * Represents a push constant range with its offset, size, and shader stages.
     */
    struct PushConstantRange {
        uint32_t offset;           // Offset in bytes
        uint32_t size;             // Size in bytes
        VkShaderStageFlags stages; // Shader stages that use this range
    };

    /**
     * @brief Vertex input attribute information extracted from shader reflection
     *
     * Represents a vertex attribute with its location, format, offset, and name.
     */
    struct VertexAttribute {
        uint32_t location; // Vertex attribute location
        VkFormat format;   // Vulkan format (e.g., VK_FORMAT_R32G32B32_SFLOAT)
        uint32_t offset;   // Offset in bytes within the vertex buffer
        std::string name;  // Attribute name from shader source
    };

    /**
     * @brief Uniform buffer member information extracted from shader reflection
     *
     * Represents a member within a uniform buffer with its name, offset, size, and format.
     */
    struct UniformMember {
        std::string name; // Member name
        uint32_t offset;  // Offset in bytes within the uniform buffer
        uint32_t size;    // Size in bytes
        VkFormat format;  // Vulkan format for type information
    };

    /**
     * @brief Shader reflection metadata container
     *
     * Stores all reflection metadata extracted from a compiled shader, including
     * descriptor bindings, push constants, vertex attributes, uniform buffers,
     * shader stage, and entry point name.
     *
     * This class is immutable after construction and is populated by ReflectionExtractor.
     */
    class ShaderReflection {
    public:
        /**
         * @brief Get all descriptor bindings from this shader
         * @return Vector of descriptor bindings
         */
        const std::vector<DescriptorBinding>& getDescriptorBindings() const;

        /**
         * @brief Get descriptor bindings for a specific descriptor set
         * @param set Descriptor set number to filter by
         * @return Vector of descriptor bindings for the specified set
         */
        std::vector<DescriptorBinding> getBindingsForSet(uint32_t set) const;

        /**
         * @brief Get all push constant ranges from this shader
         * @return Vector of push constant ranges
         */
        const std::vector<PushConstantRange>& getPushConstantRanges() const;

        /**
         * @brief Get all vertex input attributes from this shader
         * @return Vector of vertex attributes
         */
        const std::vector<VertexAttribute>& getVertexAttributes() const;

        /**
         * @brief Get all uniform buffers with their member layouts
         * @return Map of uniform buffer names to their member vectors
         */
        const std::unordered_map<std::string, std::vector<UniformMember>>& getUniformBuffers() const;

        /**
         * @brief Get the shader stage for this shader
         * @return Vulkan shader stage flags
         */
        VkShaderStageFlagBits getStage() const;

        /**
         * @brief Get the entry point name for this shader
         * @return Entry point name string
         */
        const std::string& getEntryPoint() const;

        /**
         * @brief Check if this shader reflection is compatible with another
         *
         * Validates that bindings with the same (set, binding) pair have compatible
         * types and counts across shader stages. Used for multi-stage pipeline validation.
         *
         * @param other Another shader reflection to check compatibility with
         * @return true if compatible, false otherwise
         */
        bool isCompatibleWith(const ShaderReflection& other) const;

    private:
        friend class ReflectionExtractor;

        std::vector<DescriptorBinding> m_descriptorBindings;
        std::vector<PushConstantRange> m_pushConstantRanges;
        std::vector<VertexAttribute> m_vertexAttributes;
        std::unordered_map<std::string, std::vector<UniformMember>> m_uniformBuffers;
        VkShaderStageFlagBits m_stage;
        std::string m_entryPoint;
    };

} // namespace pxt
