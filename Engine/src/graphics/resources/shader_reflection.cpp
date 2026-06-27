#include "shader_reflection.hpp"

namespace pxt {

const std::vector<DescriptorBinding>& ShaderReflection::getDescriptorBindings() const {
    return m_descriptorBindings;
}

std::vector<DescriptorBinding> ShaderReflection::getBindingsForSet(uint32_t set) const {
    std::vector<DescriptorBinding> result;
    result.reserve(m_descriptorBindings.size());
    
    for (const auto& binding : m_descriptorBindings) {
        if (binding.set == set) {
            result.push_back(binding);
        }
    }
    
    return result;
}

const std::vector<PushConstantRange>& ShaderReflection::getPushConstantRanges() const {
    return m_pushConstantRanges;
}

const std::vector<VertexAttribute>& ShaderReflection::getVertexAttributes() const {
    return m_vertexAttributes;
}

const std::unordered_map<std::string, std::vector<UniformMember>>& 
    ShaderReflection::getUniformBuffers() const {
    return m_uniformBuffers;
}

VkShaderStageFlagBits ShaderReflection::getStage() const {
    return m_stage;
}

const std::string& ShaderReflection::getEntryPoint() const {
    return m_entryPoint;
}

bool ShaderReflection::isCompatibleWith(const ShaderReflection& other) const {
    // Build a map of (set, binding) -> DescriptorBinding for this shader
    std::unordered_map<uint64_t, const DescriptorBinding*> thisBindings;
    for (const auto& binding : m_descriptorBindings) {
        uint64_t key = (static_cast<uint64_t>(binding.set) << 32) | binding.binding;
        thisBindings[key] = &binding;
    }
    
    // Check each binding in the other shader for compatibility
    for (const auto& otherBinding : other.m_descriptorBindings) {
        uint64_t key = (static_cast<uint64_t>(otherBinding.set) << 32) | otherBinding.binding;
        
        auto it = thisBindings.find(key);
        if (it != thisBindings.end()) {
            const DescriptorBinding* thisBinding = it->second;
            
            // Check that descriptor types match
            if (thisBinding->type != otherBinding.type) {
                return false;
            }
            
            // Check that descriptor counts match
            if (thisBinding->count != otherBinding.count) {
                return false;
            }
        }
    }
    
    return true;
}

} // namespace pxt
