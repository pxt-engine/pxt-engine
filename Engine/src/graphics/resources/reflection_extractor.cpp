#include "graphics/resources/reflection_extractor.hpp"

namespace pxt {

    Shared<ShaderReflection> ReflectionExtractor::extract(slang::IComponentType* program, uint32_t entryPointIndex,
                                                          uint32_t targetIndex) {

        if (!program) {
            PXT_ERROR("Cannot extract reflection: program is null");
            return nullptr;
        }

        // Get program layout for reflection data
        slang::ProgramLayout* layout = program->getLayout(targetIndex);
        if (!layout) {
            PXT_ERROR("Failed to get program layout for reflection extraction");
            return nullptr;
        }

        // Get entry point reflection
        slang::EntryPointReflection* entryPoint = layout->getEntryPointByIndex(entryPointIndex);
        if (!entryPoint) {
            PXT_ERROR("Failed to get entry point reflection at index {}", entryPointIndex);
            return nullptr;
        }

        // Create new ShaderReflection object
        auto reflection = std::make_shared<ShaderReflection>();

        // Extract entry point name and stage
        reflection->m_entryPoint = entryPoint->getName();
        reflection->m_stage = mapSlangStageToVkStage(entryPoint->getStage());

        // Extract all reflection metadata
        extractDescriptorBindings(layout, *reflection);
        extractPushConstants(layout, *reflection);
        extractVertexAttributes(entryPoint, *reflection);
        extractUniformBuffers(layout, *reflection);

        PXT_INFO("Extracted reflection for entry point '{}' (stage: {})", reflection->m_entryPoint,
                 static_cast<int>(reflection->m_stage));

        return reflection;
    }

    void ReflectionExtractor::extractDescriptorBindings(slang::ProgramLayout* layout, ShaderReflection& reflection) {

        // Traverse all parameters in the program layout
        SlangUInt parameterCount = layout->getParameterCount();

        for (SlangUInt i = 0; i < parameterCount; ++i) {
            slang::VariableLayoutReflection* paramLayout = layout->getParameterByIndex(i);
            if (!paramLayout)
                continue;

            // Traverse this parameter and its nested members
            traverseParameters(paramLayout, [&reflection](const DescriptorBinding& binding) {
                reflection.m_descriptorBindings.push_back(binding);
            });
        }

        PXT_INFO("Extracted {} descriptor binding(s)", reflection.m_descriptorBindings.size());
    }

    void ReflectionExtractor::extractPushConstants(slang::ProgramLayout* layout, ShaderReflection& reflection) {

        // Push constants are not yet fully implemented in this extraction
        // TODO: Implement push constant extraction when needed
        // For now, we'll look for parameters with push constant category

        PXT_INFO("Extracted {} push constant range(s)", reflection.m_pushConstantRanges.size());
    }

    void ReflectionExtractor::extractVertexAttributes(slang::EntryPointReflection* entryPoint,
                                                      ShaderReflection& reflection) {

        // Only extract vertex attributes for vertex shaders
        if (entryPoint->getStage() != SLANG_STAGE_VERTEX) {
            return;
        }

        // Traverse entry point parameters to find vertex inputs
        SlangUInt paramCount = entryPoint->getParameterCount();

        for (SlangUInt i = 0; i < paramCount; ++i) {
            slang::VariableLayoutReflection* paramLayout = entryPoint->getParameterByIndex(i);
            if (!paramLayout)
                continue;

            slang::TypeReflection* typeRefl = paramLayout->getTypeLayout()->getType();
            if (!typeRefl)
                continue;

            // Check if this is a vertex input (has location semantic)
            // TODO: Implement proper vertex input detection

            VertexAttribute attr;
            attr.name = paramLayout->getVariable()->getName();
            attr.location = 0; // TODO: Extract actual location from semantics
            attr.offset = 0;   // TODO: Calculate offset
            attr.format = mapSlangTypeToVkFormat(typeRefl);

            if (attr.format != VK_FORMAT_UNDEFINED) {
                reflection.m_vertexAttributes.push_back(attr);
            }
        }

        PXT_INFO("Extracted {} vertex attribute(s)", reflection.m_vertexAttributes.size());
    }

    void ReflectionExtractor::extractUniformBuffers(slang::ProgramLayout* layout, ShaderReflection& reflection) {

        // Traverse parameters to find uniform buffers
        SlangUInt parameterCount = layout->getParameterCount();

        for (SlangUInt i = 0; i < parameterCount; ++i) {
            slang::VariableLayoutReflection* paramLayout = layout->getParameterByIndex(i);
            if (!paramLayout)
                continue;

            slang::TypeLayoutReflection* typeLayout = paramLayout->getTypeLayout();
            if (!typeLayout)
                continue;

            slang::TypeReflection* type = typeLayout->getType();
            if (!type)
                continue;

            // Check if this is a constant buffer
            if (type->getKind() == slang::TypeReflection::Kind::ConstantBuffer) {
                std::string bufferName = paramLayout->getVariable()->getName();
                std::vector<UniformMember> members;

                // Extract members from the constant buffer
                slang::TypeReflection* elementType = type->getElementType();
                if (elementType) {
                    SlangUInt fieldCount = elementType->getFieldCount();
                    for (SlangUInt j = 0; j < fieldCount; ++j) {
                        slang::VariableReflection* field = elementType->getFieldByIndex(j);
                        if (!field)
                            continue;

                        UniformMember member;
                        member.name = field->getName();
                        member.offset =
                            static_cast<uint32_t>(typeLayout->getElementTypeLayout()->getFieldByIndex(j)->getOffset());
                        member.size = static_cast<uint32_t>(
                            typeLayout->getElementTypeLayout()->getFieldByIndex(j)->getTypeLayout()->getSize());
                        member.format = mapSlangTypeToVkFormat(field->getType());

                        members.push_back(member);
                    }
                }

                reflection.m_uniformBuffers[bufferName] = members;
            }
        }

        PXT_INFO("Extracted {} uniform buffer(s)", reflection.m_uniformBuffers.size());
    }

    VkDescriptorType ReflectionExtractor::mapSlangResourceToVkDescriptorType(slang::TypeReflection* type) {

        if (!type) {
            PXT_ERROR("Cannot map null type to VkDescriptorType");
            return VK_DESCRIPTOR_TYPE_MAX_ENUM;
        }

        slang::TypeReflection::Kind kind = type->getKind();

        switch (kind) {
        case slang::TypeReflection::Kind::ConstantBuffer:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

        case slang::TypeReflection::Kind::Resource: {
            // For resource types, we need to check the resource shape and access
            SlangResourceShape shape = type->getResourceShape();
            SlangResourceAccess access = type->getResourceAccess();

            // Structured buffers
            if (shape == SLANG_STRUCTURED_BUFFER) {
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            }

            // Textures
            if (shape & SLANG_TEXTURE_1D || shape & SLANG_TEXTURE_2D || shape & SLANG_TEXTURE_3D ||
                shape & SLANG_TEXTURE_CUBE) {

                // Check if it's read-write or read-only
                if (access == SLANG_RESOURCE_ACCESS_READ_WRITE) {
                    return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                } else {
                    return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                }
            }

            // Byte address buffer
            if (shape == SLANG_BYTE_ADDRESS_BUFFER) {
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            }

            break;
        }

        case slang::TypeReflection::Kind::SamplerState:
            return VK_DESCRIPTOR_TYPE_SAMPLER;

        case slang::TypeReflection::Kind::Specialized: {
            // Handle specialized types (e.g., RaytracingAccelerationStructure)
            const char* typeName = type->getName();
            if (typeName && std::string(typeName).find("AccelerationStructure") != std::string::npos) {
                return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            }
            break;
        }

        default:
            break;
        }

        PXT_WARN("Unmapped Slang resource type (kind: {}) to VkDescriptorType", static_cast<int>(kind));
        return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }

    VkFormat ReflectionExtractor::mapSlangTypeToVkFormat(slang::TypeReflection* type) {
        if (!type) {
            return VK_FORMAT_UNDEFINED;
        }

        slang::TypeReflection::ScalarType scalarType = type->getScalarType();
        SlangUInt rowCount = type->getRowCount();
        SlangUInt colCount = type->getColumnCount();

        // For now, handle vectors (colCount represents vector width for basic types)
        if (rowCount == 1) {
            switch (scalarType) {
            case slang::TypeReflection::ScalarType::Float32:
                switch (colCount) {
                case 1:
                    return VK_FORMAT_R32_SFLOAT;
                case 2:
                    return VK_FORMAT_R32G32_SFLOAT;
                case 3:
                    return VK_FORMAT_R32G32B32_SFLOAT;
                case 4:
                    return VK_FORMAT_R32G32B32A32_SFLOAT;
                }
                break;

            case slang::TypeReflection::ScalarType::Int32:
                switch (colCount) {
                case 1:
                    return VK_FORMAT_R32_SINT;
                case 2:
                    return VK_FORMAT_R32G32_SINT;
                case 3:
                    return VK_FORMAT_R32G32B32_SINT;
                case 4:
                    return VK_FORMAT_R32G32B32A32_SINT;
                }
                break;

            case slang::TypeReflection::ScalarType::UInt32:
                switch (colCount) {
                case 1:
                    return VK_FORMAT_R32_UINT;
                case 2:
                    return VK_FORMAT_R32G32_UINT;
                case 3:
                    return VK_FORMAT_R32G32B32_UINT;
                case 4:
                    return VK_FORMAT_R32G32B32A32_UINT;
                }
                break;

            case slang::TypeReflection::ScalarType::Float16:
                switch (colCount) {
                case 1:
                    return VK_FORMAT_R16_SFLOAT;
                case 2:
                    return VK_FORMAT_R16G16_SFLOAT;
                case 3:
                    return VK_FORMAT_R16G16B16_SFLOAT;
                case 4:
                    return VK_FORMAT_R16G16B16A16_SFLOAT;
                }
                break;

            default:
                break;
            }
        }

        PXT_WARN("Unmapped Slang type (scalar: {}, rows: {}, cols: {}) to VkFormat", static_cast<int>(scalarType),
                 rowCount, colCount);
        return VK_FORMAT_UNDEFINED;
    }

    void ReflectionExtractor::traverseParameters(slang::VariableLayoutReflection* varLayout,
                                                 std::function<void(const DescriptorBinding&)> callback) {

        if (!varLayout)
            return;

        slang::TypeLayoutReflection* typeLayout = varLayout->getTypeLayout();
        if (!typeLayout)
            return;

        slang::TypeReflection* type = typeLayout->getType();
        if (!type)
            return;

        // Check if this parameter has descriptor set bindings
        slang::ParameterCategory category = varLayout->getCategory();

        if (category == slang::ParameterCategory::DescriptorTableSlot) {
            // This is a descriptor binding
            DescriptorBinding binding;
            binding.set = static_cast<uint32_t>(varLayout->getBindingSpace());
            binding.binding = static_cast<uint32_t>(varLayout->getBindingIndex());
            binding.type = mapSlangResourceToVkDescriptorType(type);
            binding.count = 1; // TODO: Handle arrays properly

            if (type->getKind() == slang::TypeReflection::Kind::Array) {
                binding.count = static_cast<uint32_t>(type->getElementCount());
            }

            binding.stages = VK_SHADER_STAGE_ALL; // Will be refined during pipeline building
            binding.name = varLayout->getVariable() ? varLayout->getVariable()->getName() : "";

            if (binding.type != VK_DESCRIPTOR_TYPE_MAX_ENUM) {
                callback(binding);
            }
        }

        // Recursively traverse struct members
        if (type->getKind() == slang::TypeReflection::Kind::Struct) {
            SlangUInt fieldCount = type->getFieldCount();
            for (SlangUInt i = 0; i < fieldCount; ++i) {
                slang::VariableLayoutReflection* fieldLayout = typeLayout->getFieldByIndex(i);
                if (fieldLayout) {
                    traverseParameters(fieldLayout, callback);
                }
            }
        }
    }

    VkShaderStageFlagBits ReflectionExtractor::mapSlangStageToVkStage(SlangStage stage) {
        switch (stage) {
        case SLANG_STAGE_VERTEX:
            return VK_SHADER_STAGE_VERTEX_BIT;
        case SLANG_STAGE_FRAGMENT:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case SLANG_STAGE_COMPUTE:
            return VK_SHADER_STAGE_COMPUTE_BIT;
        case SLANG_STAGE_RAY_GENERATION:
            return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        case SLANG_STAGE_CLOSEST_HIT:
            return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        case SLANG_STAGE_MISS:
            return VK_SHADER_STAGE_MISS_BIT_KHR;
        case SLANG_STAGE_ANY_HIT:
            return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        case SLANG_STAGE_CALLABLE:
            return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
        case SLANG_STAGE_INTERSECTION:
            return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        case SLANG_STAGE_GEOMETRY:
            return VK_SHADER_STAGE_GEOMETRY_BIT;
        case SLANG_STAGE_HULL:
            return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case SLANG_STAGE_DOMAIN:
            return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        default:
            PXT_ERROR("Unmapped Slang stage: {}", static_cast<int>(stage));
            return VK_SHADER_STAGE_ALL;
        }
    }

} // namespace pxt
