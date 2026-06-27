# Implementation Plan: Slang Shader Reflection

## Overview

This implementation plan breaks down the Slang shader reflection feature into discrete coding tasks following the 5-phase architecture outlined in the design document. The implementation will integrate the Slang compiler library, extract shader reflection metadata, and enable automatic pipeline and descriptor set configuration. All tasks build incrementally, with checkpoints to validate progress and ensure the system remains functional throughout the migration.

## Tasks

- [x] 1. Phase 1: Slang Compiler Integration and Infrastructure
  - [x] 1.1 Integrate Slang library into CMake build system
    - Add Slang as a CMake dependency (find_package or add_subdirectory)
    - Link Slang library to PXT_Engine target
    - Configure Slang include directories
    - Verify Slang headers are accessible from engine code
    - Test build succeeds with Slang linked
    - _Requirements: 1.1, 1.2, 1.3_
  
  - [x] 1.2 Create SlangCompiler singleton class
    - Create `Engine/src/graphics/resources/slang_compiler.hpp` header
    - Create `Engine/src/graphics/resources/slang_compiler.cpp` implementation
    - Implement singleton pattern with getInstance() method
    - Initialize Slang global session in constructor
    - Create Slang session with Vulkan 1.3 and SPIR-V 1.4 target configuration
    - Implement addIncludePath() and clearIncludePaths() methods
    - Add include path management for assets/shaders directory structure
    - _Requirements: 1.1, 1.4, 1.5, 2.3, 13.3_
  
  - [x] 1.3 Implement Slang compilation from file
    - Implement compileFromFile() method in SlangCompiler
    - Load shader source code from file path
    - Infer shader stage from file extension (.slang.vert, .slang.frag, .slang.comp, etc.)
    - Create Slang compilation request with appropriate stage
    - Compile to SPIR-V bytecode
    - Capture compilation diagnostics (errors and warnings)
    - Return CompileResult with SPIR-V, diagnostics, and success flag
    - _Requirements: 2.1, 2.2, 2.4, 11.1_
  
  - [x] 1.4 Implement Slang compilation from source string
    - Implement compileFromSource() method in SlangCompiler
    - Accept source string, file name (for diagnostics), and shader stage
    - Create Slang compilation request from in-memory source
    - Compile to SPIR-V bytecode
    - Capture compilation diagnostics
    - Return CompileResult with SPIR-V, diagnostics, and success flag
    - _Requirements: 2.1, 2.4_
  
  - [x] 1.5 Add preprocessor definition support to SlangCompiler
    - Extend compileFromFile() to accept preprocessor definitions vector
    - Extend compileFromSource() to accept preprocessor definitions vector
    - Apply definitions to Slang compilation request
    - Test with sample shader using #ifdef directives
    - _Requirements: 2.6_
  
  - [x] 1.6 Implement helper methods for stage inference and mapping
    - Implement inferStageFromExtension() to map file extensions to VkShaderStageFlagBits
    - Support all extensions: .slang.vert, .slang.frag, .slang.comp, .slang.rgen, .slang.rchit, .slang.rmiss, .slang.rahit, .slang.rcall, .slang.rint
    - Implement mapVkStageToSlangStage() to convert Vulkan stages to Slang stages
    - _Requirements: 2.2, 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 10.1_

- [x] 2. Phase 1: Shader Reflection Data Structures
  - [x] 2.1 Create ShaderReflection data structures
    - Create `Engine/src/graphics/resources/shader_reflection.hpp` header
    - Define DescriptorBinding struct with set, binding, type, count, stages, name
    - Define PushConstantRange struct with offset, size, stages
    - Define VertexAttribute struct with location, format, offset, name
    - Define UniformMember struct with name, offset, size, format
    - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.7_
  
  - [x] 2.2 Implement ShaderReflection class
    - Create `Engine/src/graphics/resources/shader_reflection.cpp` implementation
    - Implement getDescriptorBindings() accessor
    - Implement getBindingsForSet() to filter bindings by set number
    - Implement getPushConstantRanges() accessor
    - Implement getVertexAttributes() accessor
    - Implement getUniformBuffers() accessor
    - Implement getStage() and getEntryPoint() accessors
    - Store shader stage and entry point name
    - _Requirements: 4.5, 4.6, 4.7_
  
  - [x] 2.3 Implement ShaderReflection compatibility validation
    - Implement isCompatibleWith() method to check cross-stage compatibility
    - Validate that same binding numbers have compatible types across stages
    - Validate that descriptor counts match for shared bindings
    - Return true if compatible, false otherwise
    - _Requirements: 6.4_

- [-] 3. Phase 1: Reflection Extraction from Slang
  - [x] 3.1 Create ReflectionExtractor utility class
    - Create `Engine/src/graphics/resources/reflection_extractor.hpp` header
    - Create `Engine/src/graphics/resources/reflection_extractor.cpp` implementation
    - Define static extract() method that takes Slang program and returns ShaderReflection
    - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.7_
  
  - [x] 3.2 Implement descriptor binding extraction
    - Implement extractDescriptorBindings() static method
    - Traverse Slang ProgramLayout to find all parameter bindings
    - Extract set number, binding number, descriptor type, count, and name
    - Map Slang resource types to VkDescriptorType using mapSlangResourceToVkDescriptorType()
    - Handle ConstantBuffer, StructuredBuffer, RWStructuredBuffer, Texture*, RWTexture*, SamplerState, RaytracingAccelerationStructure
    - Populate ShaderReflection descriptor bindings vector
    - _Requirements: 4.1_
  
  - [x] 3.3 Implement push constant extraction
    - Implement extractPushConstants() static method
    - Traverse Slang ProgramLayout to find push constant blocks
    - Extract offset, size, and shader stage for each push constant range
    - Populate ShaderReflection push constant ranges vector
    - _Requirements: 4.2_
  
  - [x] 3.4 Implement vertex attribute extraction
    - Implement extractVertexAttributes() static method
    - Traverse Slang EntryPointReflection to find vertex input parameters
    - Extract location, format, offset, and name for each attribute
    - Map Slang scalar types to VkFormat using mapSlangTypeToVkFormat()
    - Handle float, float2, float3, float4, int, int2, int3, int4, uint, uint2, uint3, uint4
    - Populate ShaderReflection vertex attributes vector
    - _Requirements: 4.3_
  
  - [x] 3.5 Implement uniform buffer member extraction
    - Traverse Slang reflection to find uniform buffer declarations
    - Extract member names, offsets, sizes, and types
    - Store in ShaderReflection uniform buffers map
    - _Requirements: 4.4_
  
  - [x] 3.6 Implement type mapping utilities
    - Implement mapSlangResourceToVkDescriptorType() with complete mapping table
    - Implement mapSlangTypeToVkFormat() with complete mapping table
    - Handle all Slang resource types and scalar types from design document
    - Log errors for unmapped types
    - _Requirements: 4.1, 4.3_
  
  - [x] 3.7 Integrate reflection extraction into SlangCompiler
    - Update compileFromFile() to call ReflectionExtractor::extract()
    - Update compileFromSource() to call ReflectionExtractor::extract()
    - Store extracted ShaderReflection in CompileResult
    - Handle reflection extraction failures gracefully
    - _Requirements: 4.7_

- [ ] 4. Phase 1: VulkanShader Integration
  - [~] 4.1 Add Slang compilation path to VulkanShader
    - Modify `Engine/src/graphics/resources/vk_shader.hpp` to add m_reflection member
    - Add getReflection() and hasReflection() methods
    - Add ShaderLanguage enum (GLSL, Slang)
    - Implement detectLanguage() to detect .slang.* vs .glsl extensions
    - _Requirements: 2.1, 8.1, 8.2_
  
  - [~] 4.2 Implement Slang compilation in VulkanShader
    - Modify `Engine/src/graphics/resources/vk_shader.cpp` to add compileSlang() method
    - Call SlangCompiler::getInstance().compileFromFile() for Slang shaders
    - Create VkShaderModule from compiled SPIR-V
    - Store reflection data in m_reflection member
    - Log compilation errors using PXT_ERROR
    - _Requirements: 2.1, 2.4, 2.5, 11.1, 11.5_
  
  - [~] 4.3 Maintain GLSL backward compatibility in VulkanShader
    - Keep existing compileGLSL() method unchanged
    - Route GLSL shaders to shaderc compilation path
    - Set m_reflection to nullptr for GLSL shaders
    - Ensure existing GLSL shader loading continues to work
    - _Requirements: 8.1, 8.2, 8.3_
  
  - [~] 4.4 Update VulkanShader constructor to support both languages
    - Detect shader language from file extension in constructor
    - Call compileSlang() for .slang.* files
    - Call compileGLSL() for .glsl or bare extension files
    - Preserve existing shader stage inference for GLSL
    - _Requirements: 2.1, 2.2, 8.1, 8.2, 8.3_

- [~] 5. Checkpoint - Verify Slang compilation and reflection extraction
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 6. Phase 2: PipelineBuilder for Reflection-Driven Configuration
  - [~] 6.1 Create PipelineBuilder class structure
    - Create `Engine/src/graphics/pipeline_builder.hpp` header
    - Create `Engine/src/graphics/pipeline_builder.cpp` implementation
    - Define MergedBinding internal struct
    - Add m_reflections vector to store shader reflections
    - Implement addShader() method to collect reflections from VulkanShader
    - _Requirements: 5.1, 5.2_
  
  - [~] 6.2 Implement descriptor binding merging
    - Implement mergeDescriptorBindings() private method
    - Collect all descriptor bindings from all shader reflections
    - Group bindings by (set, binding) pair
    - Merge shader stages for bindings with same (set, binding)
    - Validate that merged bindings have compatible types and counts
    - Return vector of MergedBinding structs
    - _Requirements: 6.3, 6.4_
  
  - [~] 6.3 Implement binding compatibility validation
    - Implement validateBindingCompatibility() private method
    - Check that descriptor types match for same (set, binding)
    - Check that descriptor counts match for same (set, binding)
    - Log detailed error messages for conflicts with both binding details
    - _Requirements: 6.4, 11.3_
  
  - [~] 6.4 Implement vertex input state generation
    - Implement generateVertexInputState() method
    - Extract vertex attributes from vertex shader reflection
    - Generate VkVertexInputBindingDescription for each binding
    - Generate VkVertexInputAttributeDescription for each attribute
    - Populate output vectors with binding and attribute descriptions
    - _Requirements: 5.1_
  
  - [~] 6.5 Implement descriptor set layout generation
    - Implement generateDescriptorSetLayouts() method
    - Call mergeDescriptorBindings() to get merged bindings
    - Group merged bindings by set number
    - Create VkDescriptorSetLayout for each set using Context
    - Return vector of VkDescriptorSetLayout handles
    - _Requirements: 6.1, 6.2, 6.5_
  
  - [~] 6.6 Implement push constant range generation
    - Implement generatePushConstantRanges() method
    - Collect push constant ranges from all shader reflections
    - Merge ranges with overlapping offsets and compatible stages
    - Return vector of VkPushConstantRange structs
    - _Requirements: 5.1_
  
  - [~] 6.7 Implement pipeline layout creation
    - Implement createPipelineLayout() method
    - Accept Context and descriptor set layouts as parameters
    - Generate push constant ranges
    - Create VkPipelineLayout using vkCreatePipelineLayout
    - Return VkPipelineLayout handle
    - _Requirements: 5.1_
  
  - [~] 6.8 Implement pipeline validation
    - Implement validate() method with error message output parameter
    - Check that all required shader stages are present for pipeline type
    - Check that vertex input is compatible with vertex shader
    - Check that descriptor bindings are valid
    - Return true if valid, false with error message otherwise
    - _Requirements: 5.3, 11.4_
  
  - [~] 6.9 Add manual override support
    - Implement overrideVertexInput() method
    - Store manual overrides in separate member variables
    - Use manual overrides in generateVertexInputState() if present
    - Log warnings when manual config conflicts with reflection
    - _Requirements: 5.5_

- [ ] 7. Phase 2: DescriptorSetBuilder Integration
  - [~] 7.1 Create DescriptorSetBuilder class
    - Create `Engine/src/graphics/descriptors/descriptor_set_builder.hpp` header
    - Create `Engine/src/graphics/descriptors/descriptor_set_builder.cpp` implementation
    - Add DescriptorManager reference member
    - Implement constructor accepting DescriptorManager reference
    - _Requirements: 6.1, 6.2, 6.5_
  
  - [~] 7.2 Implement descriptor entry conversion
    - Implement convertToDescriptorEntries() private method
    - Convert DescriptorBinding structs to DescriptorEntry format
    - Map VkDescriptorType to DescriptorManager's expected format
    - Handle descriptor counts and shader stages
    - _Requirements: 6.1, 6.2_
  
  - [~] 7.3 Implement descriptor set creation from reflection
    - Implement createFromReflection() method
    - Accept vector of ShaderReflection pointers and set number
    - Collect all bindings for the specified set number
    - Convert bindings to DescriptorEntry format
    - Call DescriptorManager to create descriptor set layout
    - Return DescriptorSetHandle
    - _Requirements: 6.1, 6.2, 6.5_
  
  - [~] 7.4 Implement descriptor set creation from bindings
    - Implement createFromBindings() method
    - Accept vector of DescriptorBinding structs
    - Convert to DescriptorEntry format
    - Call DescriptorManager to create descriptor set layout
    - Return DescriptorSetHandle
    - _Requirements: 6.1, 6.2, 6.5_

- [ ] 8. Phase 2: Update Pipeline class to use reflection (optional)
  - [~] 8.1 Integrate PipelineBuilder into Pipeline class
    - Modify `Engine/src/graphics/pipeline.hpp` to optionally use PipelineBuilder
    - Add method to create pipeline from shader reflections
    - Use PipelineBuilder to generate vertex input state
    - Use PipelineBuilder to generate pipeline layout
    - Maintain backward compatibility with manual configuration
    - _Requirements: 5.1, 5.2, 5.3, 5.4_

- [~] 9. Checkpoint - Verify reflection-driven pipeline creation
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 10. Phase 3: Offline Shader Compilation with CMake
  - [~] 10.1 Update CompileShaders.cmake for Slang support
    - Modify `cmake/CompileShaders.cmake` to detect .slang.* files
    - Find slangc executable (Slang command-line compiler)
    - Add CMake function to compile Slang shaders to SPIR-V
    - Configure include search paths for Slang compiler
    - Output compiled .spv files to out/shaders/ directory
    - _Requirements: 3.1, 3.2, 3.5_
  
  - [~] 10.2 Implement incremental Slang compilation
    - Add dependency tracking for Slang shader files
    - Recompile only modified .slang.* files
    - Track include file dependencies
    - Trigger recompilation when included files change
    - _Requirements: 3.4_
  
  - [~] 10.3 Add error handling for offline compilation
    - Capture slangc error output
    - Halt CMake build on compilation errors
    - Display descriptive error messages with file and line number
    - _Requirements: 3.6, 11.1_
  
  - [~] 10.4 Support mixed GLSL and Slang compilation
    - Keep existing GLSL compilation in CompileShaders.cmake
    - Compile both .glsl and .slang.* files in same build
    - Output all compiled shaders to out/shaders/
    - _Requirements: 8.5_
  
  - [~] 10.5 Test offline compilation workflow
    - Create sample .slang.vert and .slang.frag files
    - Run CMake build and verify .spv files generated
    - Modify shader and verify incremental recompilation
    - Introduce syntax error and verify build fails with error message
    - _Requirements: 3.1, 3.2, 3.4, 3.6_

- [ ] 11. Phase 3: Runtime Loading of Pre-Compiled Shaders
  - [~] 11.1 Update VulkanShader to load pre-compiled SPIR-V
    - Detect if .spv file exists for shader source file
    - Load pre-compiled SPIR-V if available
    - Fall back to runtime compilation if .spv not found
    - Extract reflection from SPIR-V (Slang embeds reflection in SPIR-V)
    - _Requirements: 13.4_
  
  - [~] 11.2 Implement reflection extraction from SPIR-V
    - Use Slang API to load SPIR-V and extract embedded reflection
    - Populate ShaderReflection from SPIR-V reflection data
    - Handle cases where reflection is not embedded
    - _Requirements: 4.7, 13.4_

- [~] 12. Checkpoint - Verify offline compilation and pre-compiled shader loading
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 13. Phase 4: Ray Tracing Pipeline Support
  - [~] 13.1 Add ray tracing shader stage support to SlangCompiler
    - Extend inferStageFromExtension() for .slang.rgen, .slang.rchit, .slang.rmiss, .slang.rahit, .slang.rcall, .slang.rint
    - Map ray tracing stages to Slang stages in mapVkStageToSlangStage()
    - Test compilation of ray generation, closest hit, miss, any hit, callable, intersection shaders
    - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6_
  
  - [~] 13.2 Extract reflection from ray tracing shaders
    - Verify ReflectionExtractor handles ray tracing shader stages
    - Extract descriptor bindings from ray tracing shaders
    - Extract push constants from ray tracing shaders
    - Test with sample ray tracing shader set
    - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6_
  
  - [~] 13.3 Update PipelineBuilder for ray tracing pipelines
    - Add support for ray tracing shader groups in PipelineBuilder
    - Validate that ray tracing pipelines have required stages (rgen at minimum)
    - Generate descriptor set layouts for ray tracing pipelines
    - _Requirements: 9.7_

- [ ] 14. Phase 4: Compute Pipeline Support
  - [~] 14.1 Add compute shader stage support to SlangCompiler
    - Verify .slang.comp extension handled in inferStageFromExtension()
    - Map compute stage to Slang stage in mapVkStageToSlangStage()
    - Test compilation of compute shaders
    - _Requirements: 10.1_
  
  - [~] 14.2 Extract reflection from compute shaders
    - Verify ReflectionExtractor handles compute shader stage
    - Extract descriptor bindings from compute shaders
    - Extract push constants from compute shaders
    - Extract workgroup size from compute shader reflection
    - _Requirements: 10.1, 10.3_
  
  - [~] 14.3 Update PipelineBuilder for compute pipelines
    - Add support for compute pipeline configuration in PipelineBuilder
    - Generate descriptor set layouts for compute pipelines
    - Validate compute pipeline has compute shader stage
    - _Requirements: 10.2_

- [ ] 15. Phase 4: Shader Reloading with Reflection Updates
  - [~] 15.1 Implement shader reloading in VulkanShader
    - Add reload() method to VulkanShader
    - Recompile shader source on reload
    - Extract new reflection metadata
    - Update VkShaderModule with new SPIR-V
    - Preserve existing shader module if recompilation fails
    - _Requirements: 7.1_
  
  - [~] 15.2 Implement pipeline rebuild on shader reload
    - Detect when shader reflection has changed
    - Trigger pipeline rebuild in affected pipelines
    - Regenerate descriptor set layouts if bindings changed
    - Log warnings for incompatible reflection changes
    - _Requirements: 7.2, 7.3, 7.4_
  
  - [~] 15.3 Preserve existing shader reload functionality
    - Ensure button-based manual reload continues to work
    - Integrate reflection update into existing reload workflow
    - Test shader reload in editor with reflection changes
    - _Requirements: 7.5_

- [ ] 16. Phase 4: Error Handling and Diagnostics
  - [~] 16.1 Implement comprehensive error logging
    - Log Slang compilation errors with file path, line number, description
    - Log reflection extraction failures with shader name and operation
    - Log descriptor binding conflicts with both conflicting bindings
    - Log missing shader stages with pipeline type and missing stage
    - Use PXT_ERROR and PXT_FATAL macros consistently
    - _Requirements: 11.1, 11.2, 11.3, 11.4, 11.5_
  
  - [~] 16.2 Add validation error messages
    - Implement detailed validation messages in PipelineBuilder::validate()
    - Include context in all error messages (shader names, binding details, etc.)
    - Provide actionable error messages for common issues
    - _Requirements: 11.1, 11.2, 11.3, 11.4_

- [~] 17. Checkpoint - Verify ray tracing, compute, and shader reloading
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 18. Phase 4: Testing and Validation
  - [~] 18.1 Create unit tests for SlangCompiler
    - Test session initialization
    - Test compilation of valid Slang shaders
    - Test compilation error handling
    - Test include file resolution
    - Test preprocessor definition handling
    - Test stage inference from file extensions
    - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.6_
  
  - [ ]* 18.2 Create unit tests for ReflectionExtractor
    - Test extraction of descriptor bindings
    - Test extraction of push constants
    - Test extraction of vertex attributes
    - Test type mapping (Slang → Vulkan)
    - Test handling of arrays and structs
    - Test handling of nested resources
    - _Requirements: 4.1, 4.2, 4.3, 4.4_
  
  - [ ]* 18.3 Create unit tests for PipelineBuilder
    - Test merging bindings from multiple shaders
    - Test binding conflict detection
    - Test vertex input generation
    - Test push constant range generation
    - Test descriptor set layout generation
    - _Requirements: 5.1, 5.2, 5.3, 6.3, 6.4_
  
  - [ ]* 18.4 Create unit tests for DescriptorSetBuilder
    - Test conversion to DescriptorEntry format
    - Test integration with DescriptorManager
    - Test descriptor set creation from reflection
    - _Requirements: 6.1, 6.2, 6.5_
  
  - [ ]* 18.5 Create integration tests for end-to-end compilation
    - Test compile simple Slang shader → verify SPIR-V output
    - Test compile shader with uniforms → verify reflection data
    - Test compile shader with textures → verify descriptor bindings
    - Test compile compute shader → verify workgroup size extraction
    - _Requirements: 2.1, 4.1, 4.7, 10.1_
  
  - [ ]* 18.6 Create integration tests for pipeline creation
    - Test create graphics pipeline from Slang shaders → verify success
    - Test create ray tracing pipeline from Slang shaders → verify success
    - Test create compute pipeline from Slang shader → verify success
    - Test mix GLSL and Slang shaders → verify backward compatibility
    - _Requirements: 5.1, 8.3, 8.4, 9.7, 10.2_
  
  - [ ]* 18.7 Create integration tests for shader reloading
    - Test load shader → modify → reload → verify new reflection
    - Test reload with binding changes → verify descriptor sets updated
    - Test reload with errors → verify fallback to previous version
    - _Requirements: 7.1, 7.2, 7.3_

- [ ] 19. Phase 4: Performance Validation
  - [~] 19.1 Measure and validate compilation performance
    - Measure runtime compilation time for typical shader (200 lines)
    - Verify compilation time within 100ms target
    - Measure offline compilation time for all engine shaders
    - Verify offline compilation within 10s target
    - _Requirements: 2.7, 12.1, 12.4_
  
  - [~] 19.2 Measure and validate reflection extraction performance
    - Measure reflection extraction time per shader
    - Verify extraction time within 10ms target
    - Profile reflection extraction for bottlenecks
    - _Requirements: 12.2_
  
  - [~] 19.3 Measure and validate pipeline creation performance
    - Measure descriptor set layout generation time
    - Verify generation time within 5ms target
    - Profile PipelineBuilder for bottlenecks
    - _Requirements: 12.3_

- [ ] 20. Phase 4: Documentation
  - [~] 20.1 Write API documentation
    - Document SlangCompiler class with usage examples
    - Document ShaderReflection data structures
    - Document PipelineBuilder usage and workflow
    - Document DescriptorSetBuilder usage
    - Document error handling patterns
    - _Requirements: 14.1, 14.2, 14.3, 14.4_
  
  - [~] 20.2 Write migration guide
    - Document Slang benefits and overview
    - Document file naming conventions (.slang.vert, .slang.frag, etc.)
    - Provide GLSL to Slang conversion examples
    - Document common pitfalls and solutions
    - Document performance considerations
    - Provide testing recommendations
    - _Requirements: 14.2, 14.3, 14.4, 14.5_
  
  - [~] 20.3 Write user guide for Slang shaders
    - Document how to write Slang shaders for PXT Engine
    - Document descriptor binding conventions
    - Document push constant usage
    - Document vertex attribute conventions
    - Document include file organization
    - Document shader hot-reload workflow
    - _Requirements: 14.2, 14.3_

- [~] 21. Final Checkpoint - Complete system validation
  - Ensure all tests pass, ask the user if questions arise.

## Notes

- Tasks marked with `*` are optional testing tasks and can be skipped for faster MVP
- Each task references specific requirements for traceability
- Checkpoints ensure incremental validation throughout implementation
- Phase 1 (tasks 1-5) establishes core infrastructure
- Phase 2 (tasks 6-9) enables reflection-driven configuration
- Phase 3 (tasks 10-12) adds offline compilation support
- Phase 4 (tasks 13-21) completes ray tracing, compute, testing, and documentation
- Phase 5 (shader conversion) is out of scope for this implementation plan
- All implementation uses C++23 following existing PXT Engine conventions
- Backward compatibility with GLSL shaders maintained throughout
