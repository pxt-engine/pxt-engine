# Requirements Document

## Introduction

This document specifies the requirements for migrating the PXT Engine's shader system from GLSL to Slang with shader reflection capabilities. The migration will replace the current shaderc-based GLSL compilation pipeline with Slang's cross-compilation system, enabling automatic extraction of shader metadata to drive pipeline and descriptor set creation. This eliminates manual configuration and reduces maintenance burden while preserving all existing rendering capabilities (rasterization, ray tracing, compute).

## Glossary

- **Slang_Compiler**: The Slang shader compiler library that compiles Slang shading language to SPIR-V
- **Shader_Reflection_System**: The system that extracts metadata from compiled shaders (uniforms, bindings, push constants, vertex inputs, shader stages)
- **VulkanShader**: The existing C++ class that manages shader compilation and module creation
- **Pipeline_Builder**: The system that constructs Vulkan graphics/compute/ray-tracing pipelines
- **Descriptor_Set_Manager**: The system that manages descriptor set layouts and allocations, we already have that inside graphics/descriptors/descriptor_manager. What we need is a class that build descriptors from metadata using the manager.
- **Runtime_Compiler**: The component that compiles shaders at runtime during engine execution, now inside vk_shader file
- **Offline_Compiler**: The CMake-based build system component that compiles shaders during the build process
- **Shader_Module**: A Vulkan VkShaderModule object created from compiled SPIR-V bytecode. currently is a member of the VulkanShader class in vk_shader file (graphics/resources)
- **Reflection_Metadata**: Extracted shader information including bindings, uniforms, push constants, vertex attributes, and stage information
- **SPIR-V**: Standard Portable Intermediate Representation for Vulkan shaders
- **Asset_Pipeline**: The engine's system for loading and managing shader files from the assets directory

## Requirements

### Requirement 1: Slang Compiler Integration

**User Story:** As an engine developer, I want to integrate the Slang compiler library into the build system, so that the engine can compile Slang shaders to SPIR-V.

#### Acceptance Criteria

1. THE Build_System SHALL link the Slang compiler library to the PXT_Engine target
2. THE Build_System SHALL provide Slang header files to the engine source code
3. WHEN the engine is built, THE Build_System SHALL verify Slang library availability
4. THE Slang_Compiler SHALL support Vulkan 1.3 as the target environment
5. THE Slang_Compiler SHALL support SPIR-V 1.4 or higher as the output format

### Requirement 2: Slang Shader Compilation (Runtime)

**User Story:** As an engine developer, I want to compile Slang shaders at runtime, so that shaders can be loaded and reloaded during engine execution.

#### Acceptance Criteria

1. WHEN a Slang shader file is provided, THE VulkanShader SHALL compile it to SPIR-V bytecode
2. THE VulkanShader SHALL infer shader stage from file extensions (.slang.vert, .slang.frag, .slang.comp, .slang.rgen, .slang.rchit, .slang.rmiss, .slang.rahit, .slang.rcall, .slang.rint)
3. THE VulkanShader SHALL support include directives for modular shader code
4. WHEN compilation fails, THE VulkanShader SHALL log descriptive error messages with file name and line number
5. THE VulkanShader SHALL create a valid Vulkan Shader_Module from the compiled SPIR-V
6. THE VulkanShader SHALL support preprocessor definitions passed at compile time
7. THE Runtime_Compiler SHALL maintain compilation performance comparable to the existing shaderc implementation (within 20% overhead)

### Requirement 3: Slang Shader Compilation (Offline)

**User Story:** As an engine developer, I want to compile Slang shaders during the build process, so that pre-compiled shaders are available at runtime for faster loading.

#### Acceptance Criteria

1. THE Offline_Compiler SHALL compile all Slang shader files in the assets/shaders directory to SPIR-V
2. THE Offline_Compiler SHALL output compiled shaders to the out/shaders directory with .spv extension
3. THE Offline_Compiler SHALL support the same file extensions as the Runtime_Compiler
4. WHEN a shader file is modified, THE Offline_Compiler SHALL recompile only the modified shader
5. THE Offline_Compiler SHALL pass include search paths to the Slang compiler
6. WHEN offline compilation fails, THE Build_System SHALL halt the build with a descriptive error message

### Requirement 4: Shader Reflection Data Extraction

**User Story:** As an engine developer, I want to extract reflection metadata from compiled shaders, so that pipeline and descriptor configurations can be generated automatically.

#### Acceptance Criteria

1. WHEN a shader is compiled, THE Shader_Reflection_System SHALL extract all descriptor set bindings with their binding numbers, descriptor types, and shader stages
2. THE Shader_Reflection_System SHALL extract push constant ranges with their sizes, offsets, and shader stages
3. THE Shader_Reflection_System SHALL extract vertex input attributes with their locations, formats, and offsets
4. THE Shader_Reflection_System SHALL extract uniform buffer members with their names, types, sizes, and offsets
5. THE Shader_Reflection_System SHALL identify the shader stage (vertex, fragment, compute, ray generation, closest hit, miss, any hit, callable, intersection)
6. THE Shader_Reflection_System SHALL extract entry point names from the compiled shader
7. THE Reflection_Metadata SHALL be stored in a structured format accessible to the Pipeline_Builder and Descriptor_Set_Manager

### Requirement 5: Automatic Pipeline Configuration

**User Story:** As an engine developer, I want pipelines to be configured automatically from reflection metadata, so that I don't need to manually specify shader stages and vertex inputs.

#### Acceptance Criteria

1. WHEN creating a graphics pipeline, THE Pipeline_Builder SHALL use reflection metadata to configure vertex input bindings and attributes
2. THE Pipeline_Builder SHALL use reflection metadata to determine which shader stages are present
3. THE Pipeline_Builder SHALL validate that all required shader stages for a pipeline type are present
4. WHEN reflection metadata conflicts with manual configuration, THE Pipeline_Builder SHALL log a warning and use the reflection metadata
5. THE Pipeline_Builder SHALL support overriding reflection-derived configurations when explicitly specified

### Requirement 6: Automatic Descriptor Set Layout Generation

**User Story:** As an engine developer, I want descriptor set layouts to be generated automatically from reflection metadata, so that binding configurations stay synchronized with shader code.

#### Acceptance Criteria

1. WHEN shaders are compiled, THE Descriptor_Set_Manager SHALL generate descriptor set layouts from reflection metadata
2. THE Descriptor_Set_Manager SHALL group bindings by descriptor set number
3. THE Descriptor_Set_Manager SHALL merge bindings from multiple shader stages in the same pipeline
4. WHEN bindings conflict across shader stages, THE Descriptor_Set_Manager SHALL validate compatibility and report errors
5. THE Descriptor_Set_Manager SHALL create VkDescriptorSetLayout objects from the generated layouts

### Requirement 7: Shader Reloading with Reflection

**User Story:** As an engine developer, I want to reload shaders and update their reflection metadata, so that pipeline configurations update when shaders change.

#### Acceptance Criteria

1. WHEN a shader is reloaded, THE VulkanShader SHALL recompile the shader and extract new reflection metadata
2. THE Pipeline_Builder SHALL rebuild affected pipelines using the new reflection metadata
3. THE Descriptor_Set_Manager SHALL regenerate descriptor set layouts if bindings have changed
4. WHEN reflection metadata changes incompatibly, THE Shader_Reflection_System SHALL log warnings about affected pipelines
5. THE VulkanShader SHALL preserve existing shader reloading functionality (button-based manual reload)

### Requirement 8: Backward Compatibility with Existing Shaders

**User Story:** As an engine developer, I want to maintain support for existing GLSL shaders during the migration, so that the engine remains functional while shaders are being converted.

#### Acceptance Criteria

1. THE VulkanShader SHALL detect shader language from file extension (.glsl vs .slang)
2. WHEN a GLSL shader is provided, THE VulkanShader SHALL use the existing shaderc compilation path
3. WHEN a Slang shader is provided, THE VulkanShader SHALL use the new Slang compilation path
4. THE Pipeline_Builder SHALL support pipelines with mixed GLSL and Slang shaders during the transition period
5. THE Offline_Compiler SHALL compile both GLSL and Slang shaders in the same build

### Requirement 9: Ray Tracing Pipeline Support

**User Story:** As an engine developer, I want ray tracing pipelines to use reflection metadata, so that ray tracing shaders benefit from automatic configuration.

#### Acceptance Criteria

1. THE Shader_Reflection_System SHALL extract reflection metadata from ray generation shaders
2. THE Shader_Reflection_System SHALL extract reflection metadata from closest hit shaders
3. THE Shader_Reflection_System SHALL extract reflection metadata from miss shaders
4. THE Shader_Reflection_System SHALL extract reflection metadata from any hit shaders
5. THE Shader_Reflection_System SHALL extract reflection metadata from callable shaders
6. THE Shader_Reflection_System SHALL extract reflection metadata from intersection shaders
7. THE Pipeline_Builder SHALL use reflection metadata to configure ray tracing pipeline shader groups

### Requirement 10: Compute Pipeline Support

**User Story:** As an engine developer, I want compute pipelines to use reflection metadata, so that compute shaders benefit from automatic configuration.

#### Acceptance Criteria

1. THE Shader_Reflection_System SHALL extract reflection metadata from compute shaders
2. THE Pipeline_Builder SHALL use reflection metadata to configure compute pipeline descriptor sets
3. THE Pipeline_Builder SHALL extract workgroup size information from compute shader reflection metadata

### Requirement 11: Error Handling and Diagnostics

**User Story:** As an engine developer, I want clear error messages when shader compilation or reflection fails, so that I can quickly identify and fix shader issues.

#### Acceptance Criteria

1. WHEN Slang compilation fails, THE VulkanShader SHALL log the error message with file path, line number, and error description
2. WHEN reflection extraction fails, THE Shader_Reflection_System SHALL log the shader name and the specific reflection operation that failed
3. WHEN descriptor bindings conflict, THE Descriptor_Set_Manager SHALL log both conflicting bindings with their shader stages and binding details
4. WHEN a required shader stage is missing, THE Pipeline_Builder SHALL log which stage is missing and which pipeline requires it
5. THE VulkanShader SHALL continue to use the existing PXT_ERROR and PXT_FATAL logging macros for consistency

### Requirement 12: Performance Requirements

**User Story:** As an engine developer, I want shader compilation and reflection to have minimal performance impact, so that engine startup and shader reloading remain responsive.

#### Acceptance Criteria

1. THE Runtime_Compiler SHALL compile and reflect a typical shader (200 lines) within 100 milliseconds on a modern development machine
2. THE Shader_Reflection_System SHALL extract reflection metadata within 10 milliseconds per shader
3. THE Descriptor_Set_Manager SHALL generate descriptor set layouts within 5 milliseconds per pipeline
4. THE Offline_Compiler SHALL compile all engine shaders (approximately 50 files) within 10 seconds during a clean build

### Requirement 13: Asset Pipeline Integration

**User Story:** As an engine developer, I want Slang shaders to integrate seamlessly with the existing asset pipeline, so that shader loading follows established patterns.

#### Acceptance Criteria

1. THE Asset_Pipeline SHALL load Slang shader files from the assets/shaders directory
2. THE Asset_Pipeline SHALL support the same directory structure for Slang shaders as GLSL shaders (common/, lighting/, material/, raytracing/, etc.)
3. THE VulkanShader SHALL support the same include search paths for Slang as for GLSL
4. THE Asset_Pipeline SHALL support loading pre-compiled SPIR-V files with embedded reflection metadata

### Requirement 14: Documentation and Migration Path

**User Story:** As an engine developer, I want documentation on the Slang migration, so that I understand how to write and convert shaders.

#### Acceptance Criteria

1. THE Documentation SHALL describe the Slang file naming conventions and extensions
2. THE Documentation SHALL provide examples of converting GLSL shaders to Slang
3. THE Documentation SHALL document the reflection metadata structure and how to access it
4. THE Documentation SHALL describe the automatic pipeline configuration behavior
5. THE Documentation SHALL provide a migration checklist for converting existing shaders
