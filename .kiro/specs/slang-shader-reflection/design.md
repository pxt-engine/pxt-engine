# Design Document: Slang Shader Reflection

## Overview

This design describes the migration of PXT Engine's shader compilation system from GLSL/shaderc to Slang with comprehensive shader reflection capabilities. The migration enables automatic extraction of shader metadata (bindings, uniforms, push constants, vertex attributes) to drive pipeline and descriptor set configuration, eliminating manual setup and reducing maintenance burden.

**Key Design Goals:**
- Replace shaderc-based GLSL compilation with Slang compilation to SPIR-V
- Extract complete reflection metadata from compiled shaders
- Automatically configure Vulkan pipelines and descriptor sets from reflection data
- Maintain backward compatibility with existing GLSL shaders during transition
- Support all pipeline types: rasterization, ray tracing, and compute
- Preserve existing shader hot-reload functionality
- Integrate seamlessly with existing asset pipeline and build system

**Scope:**
- Slang compiler library integration (build system and runtime)
- Runtime shader compilation with reflection extraction
- Offline shader compilation (CMake build process)
- Reflection metadata representation and access
- Automatic pipeline configuration from reflection
- Automatic descriptor set layout generation from reflection
- Backward compatibility layer for GLSL shaders

**Out of Scope:**
- Conversion of existing GLSL shaders to Slang (separate migration task)
- Changes to shader rendering logic or algorithms
- Modifications to existing descriptor binding strategies (only automation)

## Architecture

### High-Level Architecture

The Slang integration follows a layered architecture that mirrors the existing shader system structure:

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                         │
│  (Render Systems, Pipeline Creation, Descriptor Binding)    │
└────────────────────┬────────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────────┐
│              Reflection-Driven Configuration                 │
│  • PipelineBuilder (auto-configures from reflection)        │
│  • DescriptorSetBuilder (generates layouts from reflection) │
└────────────────────┬────────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────────┐
│                 Shader Reflection Layer                      │
│  • ShaderReflection (metadata container)                    │
│  • ReflectionExtractor (Slang API wrapper)                  │
└────────────────────┬────────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────────┐
│                  Shader Compilation Layer                    │
│  • VulkanShader (runtime compilation)                       │
│  • SlangCompiler (Slang session management)                 │
│  • GLSLCompiler (backward compatibility)                    │
└────────────────────┬────────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────────┐
│                    Compiler Libraries                        │
│  • Slang (libslang.so/slang.dll)                           │
│  • shaderc (existing, for GLSL)                            │
└─────────────────────────────────────────────────────────────┘
```

### Component Interaction Flow

**Runtime Compilation Flow:**
```
1. Application requests shader → VulkanShader
2. VulkanShader detects language (.slang vs .glsl)
3a. Slang path: SlangCompiler compiles → SPIR-V + Reflection
3b. GLSL path: shaderc compiles → SPIR-V (no reflection)
4. VulkanShader creates VkShaderModule
5. ReflectionExtractor processes Slang reflection data
6. ShaderReflection stores structured metadata
7. PipelineBuilder/DescriptorSetBuilder consume metadata
```

**Offline Compilation Flow (CMake):**
```
1. CMake detects shader file changes
2. CompileShaders.cmake invokes slangc for .slang files
3. slangc produces .spv files in out/shaders/
4. Runtime loads pre-compiled .spv files
5. Reflection metadata embedded in SPIR-V (via Slang)
6. Runtime extracts reflection from SPIR-V
```

### Design Patterns

**Strategy Pattern:** Shader compilation uses strategy pattern to switch between Slang and GLSL compilers based on file extension.

**Builder Pattern:** PipelineBuilder and DescriptorSetBuilder use builder pattern to construct complex Vulkan objects from reflection metadata.

**Facade Pattern:** SlangCompiler provides simplified facade over Slang's COM-style API.

**Registry Pattern:** Shader reflection metadata cached in registry for reuse across pipeline rebuilds.

## Components and Interfaces

### 1. SlangCompiler

**Purpose:** Manages Slang global session, compilation sessions, and provides simplified interface for shader compilation.

**Location:** `Engine/src/graphics/resources/slang_compiler.hpp/cpp`

**Key Responsibilities:**
- Initialize Slang global session (singleton)
- Create and manage compilation sessions with target configuration
- Compile Slang source code to SPIR-V
- Extract reflection data from compiled programs
- Handle include file resolution
- Manage preprocessor definitions

**Interface:**
```cpp
class SlangCompiler {
public:
    // Singleton access
    static SlangCompiler& getInstance();
    
    // Compilation
    struct CompileResult {
        std::vector<uint32_t> spirv;
        Shared<ShaderReflection> reflection;
        std::string diagnostics;
        bool success;
    };
    
    CompileResult compileFromFile(
        const std::string& filePath,
        VkShaderStageFlagBits stage,
        const std::vector<std::pair<std::string, std::string>>& definitions = {}
    );
    
    CompileResult compileFromSource(
        const std::string& source,
        const std::string& fileName,
        VkShaderStageFlagBits stage,
        const std::vector<std::pair<std::string, std::string>>& definitions = {}
    );
    
    // Session management
    void addIncludePath(const std::string& path);
    void clearIncludePaths();
    
private:
    SlangCompiler();
    ~SlangCompiler();
    
    Slang::ComPtr<slang::IGlobalSession> m_globalSession;
    Slang::ComPtr<slang::ISession> m_session;
    std::vector<std::string> m_includePaths;
    
    void initializeSession();
    VkShaderStageFlagBits inferStageFromExtension(const std::string& fileName);
    slang::Stage mapVkStageToSlangStage(VkShaderStageFlagBits vkStage);
};
```

**Design Notes:**
- Singleton pattern ensures single Slang global session for caching
- Session reused across compilations for performance
- Include paths configured once at initialization
- Diagnostics captured and returned for error reporting

### 2. ShaderReflection

**Purpose:** Stores structured shader reflection metadata extracted from Slang.

**Location:** `Engine/src/graphics/resources/shader_reflection.hpp/cpp`

**Key Responsibilities:**
- Store descriptor set bindings with types, counts, and stages
- Store push constant ranges
- Store vertex input attributes
- Store uniform buffer layouts
- Provide query interface for pipeline and descriptor configuration

**Interface:**
```cpp
struct DescriptorBinding {
    uint32_t set;
    uint32_t binding;
    VkDescriptorType type;
    uint32_t count;
    VkShaderStageFlags stages;
    std::string name;
};

struct PushConstantRange {
    uint32_t offset;
    uint32_t size;
    VkShaderStageFlags stages;
};

struct VertexAttribute {
    uint32_t location;
    VkFormat format;
    uint32_t offset;
    std::string name;
};

struct UniformMember {
    std::string name;
    uint32_t offset;
    uint32_t size;
    VkFormat format; // for type information
};

class ShaderReflection {
public:
    // Descriptor bindings
    const std::vector<DescriptorBinding>& getDescriptorBindings() const;
    std::vector<DescriptorBinding> getBindingsForSet(uint32_t set) const;
    
    // Push constants
    const std::vector<PushConstantRange>& getPushConstantRanges() const;
    
    // Vertex attributes
    const std::vector<VertexAttribute>& getVertexAttributes() const;
    
    // Uniform buffers
    const std::unordered_map<std::string, std::vector<UniformMember>>& 
        getUniformBuffers() const;
    
    // Shader stage
    VkShaderStageFlagBits getStage() const;
    
    // Entry point
    const std::string& getEntryPoint() const;
    
    // Validation
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
```

**Design Notes:**
- Immutable after construction (populated by ReflectionExtractor)
- Provides filtered views (e.g., bindings for specific set)
- Compatibility checking for multi-shader pipelines
- Stores Vulkan-native types for direct use

### 3. ReflectionExtractor

**Purpose:** Extracts reflection metadata from Slang's reflection API and populates ShaderReflection.

**Location:** `Engine/src/graphics/resources/reflection_extractor.hpp/cpp`

**Key Responsibilities:**
- Traverse Slang reflection API structures
- Map Slang types to Vulkan types
- Extract descriptor bindings from program layout
- Extract push constants from program layout
- Extract vertex attributes from entry point parameters
- Handle nested structures and arrays

**Interface:**
```cpp
class ReflectionExtractor {
public:
    static Shared<ShaderReflection> extract(
        slang::IComponentType* program,
        uint32_t entryPointIndex,
        uint32_t targetIndex
    );
    
private:
    static void extractDescriptorBindings(
        slang::ProgramLayout* layout,
        ShaderReflection& reflection
    );
    
    static void extractPushConstants(
        slang::ProgramLayout* layout,
        ShaderReflection& reflection
    );
    
    static void extractVertexAttributes(
        slang::EntryPointReflection* entryPoint,
        ShaderReflection& reflection
    );
    
    static VkDescriptorType mapSlangResourceToVkDescriptorType(
        slang::TypeReflection* type
    );
    
    static VkFormat mapSlangTypeToVkFormat(
        slang::TypeReflection* type
    );
    
    static void traverseParameters(
        slang::VariableLayoutReflection* varLayout,
        std::function<void(const DescriptorBinding&)> callback
    );
};
```

**Design Notes:**
- Static utility class (no state)
- Recursive traversal of Slang reflection structures
- Type mapping tables for Slang → Vulkan conversion
- Handles complex cases (arrays, structs, nested resources)

### 4. Enhanced VulkanShader

**Purpose:** Extended to support both Slang and GLSL compilation with reflection extraction.

**Location:** `Engine/src/graphics/resources/vk_shader.hpp/cpp` (modified)

**Key Changes:**
```cpp
class VulkanShader {
public:
    VulkanShader(Context& context, const std::string_view& fileName,
                 const std::vector<std::pair<std::string, std::string>>& definitions = {});
    
    // NEW: Access reflection data
    Shared<ShaderReflection> getReflection() const { return m_reflection; }
    
    // NEW: Check if shader has reflection
    bool hasReflection() const { return m_reflection != nullptr; }
    
    // Existing methods unchanged
    VkPipelineShaderStageCreateInfo getShaderStageCreateInfo();
    VkShaderModule getShaderModule() { return m_module; }
    
private:
    enum class ShaderLanguage {
        GLSL,
        Slang
    };
    
    ShaderLanguage detectLanguage(const std::string_view& fileName);
    
    void compileSlang(const std::string& source, const std::string& fileName);
    void compileGLSL(const std::string& source, const std::string& fileName);
    
    // NEW: Reflection data
    Shared<ShaderReflection> m_reflection;
    
    // Existing members
    Context& m_context;
    VkShaderModule m_module;
    VkShaderStageFlagBits m_vkStage;
    // ... other existing members
};
```

**Design Notes:**
- Language detection based on file extension (.slang.* vs .glsl or bare extensions)
- Slang path produces reflection, GLSL path does not
- Backward compatible: existing code continues to work
- Reflection optional: checked with hasReflection()

### 5. PipelineBuilder

**Purpose:** New utility class that constructs pipeline configuration from shader reflection.

**Location:** `Engine/src/graphics/pipeline_builder.hpp/cpp`

**Key Responsibilities:**
- Generate vertex input state from reflection
- Merge descriptor bindings from multiple shaders
- Validate binding compatibility across stages
- Generate push constant ranges
- Create pipeline layouts from reflection

**Interface:**
```cpp
class PipelineBuilder {
public:
    PipelineBuilder& addShader(const VulkanShader& shader);
    
    // Generate vertex input configuration
    void generateVertexInputState(
        std::vector<VkVertexInputBindingDescription>& bindings,
        std::vector<VkVertexInputAttributeDescription>& attributes
    );
    
    // Generate descriptor set layouts
    std::vector<VkDescriptorSetLayout> generateDescriptorSetLayouts(
        Context& context
    );
    
    // Generate push constant ranges
    std::vector<VkPushConstantRange> generatePushConstantRanges();
    
    // Create complete pipeline layout
    VkPipelineLayout createPipelineLayout(
        Context& context,
        const std::vector<VkDescriptorSetLayout>& setLayouts
    );
    
    // Validation
    bool validate(std::string& errorMessage);
    
    // Manual overrides (for gradual migration)
    void overrideVertexInput(
        const std::vector<VkVertexInputBindingDescription>& bindings,
        const std::vector<VkVertexInputAttributeDescription>& attributes
    );
    
private:
    std::vector<Shared<ShaderReflection>> m_reflections;
    
    struct MergedBinding {
        uint32_t set;
        uint32_t binding;
        VkDescriptorType type;
        uint32_t count;
        VkShaderStageFlags stages; // merged from all shaders
    };
    
    std::vector<MergedBinding> mergeDescriptorBindings();
    bool validateBindingCompatibility(const MergedBinding& a, const MergedBinding& b);
};
```

**Design Notes:**
- Builder pattern for incremental configuration
- Merges reflection from multiple shader stages
- Validates cross-stage compatibility
- Supports manual overrides for gradual migration
- Logs warnings when reflection conflicts with manual config

### 6. DescriptorSetBuilder

**Purpose:** Generates descriptor set layouts from reflection metadata and integrates with existing DescriptorManager.

**Location:** `Engine/src/graphics/descriptors/descriptor_set_builder.hpp/cpp`

**Key Responsibilities:**
- Convert reflection bindings to DescriptorEntry format
- Group bindings by descriptor set number
- Create descriptor set layouts via DescriptorManager
- Handle descriptor set allocation

**Interface:**
```cpp
class DescriptorSetBuilder {
public:
    explicit DescriptorSetBuilder(DescriptorManager& manager);
    
    // Build from reflection
    DescriptorSetHandle createFromReflection(
        const std::vector<Shared<ShaderReflection>>& reflections,
        uint32_t setNumber
    );
    
    // Build from merged bindings
    DescriptorSetHandle createFromBindings(
        const std::vector<DescriptorBinding>& bindings
    );
    
private:
    DescriptorManager& m_manager;
    
    std::vector<DescriptorEntry> convertToDescriptorEntries(
        const std::vector<DescriptorBinding>& bindings
    );
};
```

**Design Notes:**
- Thin wrapper over DescriptorManager
- Converts reflection format to DescriptorManager format
- Reuses existing descriptor allocation infrastructure
- No changes to DescriptorManager itself

## Data Models

### Reflection Metadata Storage

**ShaderReflection** stores all metadata extracted from a single shader stage:

```
ShaderReflection
├── Descriptor Bindings (vector)
│   ├── set: uint32_t
│   ├── binding: uint32_t
│   ├── type: VkDescriptorType
│   ├── count: uint32_t
│   ├── stages: VkShaderStageFlags
│   └── name: string
├── Push Constant Ranges (vector)
│   ├── offset: uint32_t
│   ├── size: uint32_t
│   └── stages: VkShaderStageFlags
├── Vertex Attributes (vector)
│   ├── location: uint32_t
│   ├── format: VkFormat
│   ├── offset: uint32_t
│   └── name: string
├── Uniform Buffers (map<string, vector<UniformMember>>)
│   └── [buffer_name]
│       ├── name: string
│       ├── offset: uint32_t
│       ├── size: uint32_t
│       └── format: VkFormat
├── Stage: VkShaderStageFlagBits
└── Entry Point: string
```

### Type Mapping Tables

**Slang Resource Types → Vulkan Descriptor Types:**

| Slang Type | VkDescriptorType |
|------------|------------------|
| ConstantBuffer<T> | VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER |
| StructuredBuffer<T> | VK_DESCRIPTOR_TYPE_STORAGE_BUFFER |
| RWStructuredBuffer<T> | VK_DESCRIPTOR_TYPE_STORAGE_BUFFER |
| Texture1D/2D/3D/Cube | VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE |
| RWTexture1D/2D/3D | VK_DESCRIPTOR_TYPE_STORAGE_IMAGE |
| SamplerState | VK_DESCRIPTOR_TYPE_SAMPLER |
| SamplerComparisonState | VK_DESCRIPTOR_TYPE_SAMPLER |
| RaytracingAccelerationStructure | VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR |

**Slang Scalar Types → Vulkan Formats:**

| Slang Type | VkFormat |
|------------|----------|
| float | VK_FORMAT_R32_SFLOAT |
| float2 | VK_FORMAT_R32G32_SFLOAT |
| float3 | VK_FORMAT_R32G32B32_SFLOAT |
| float4 | VK_FORMAT_R32G32B32A32_SFLOAT |
| int | VK_FORMAT_R32_SINT |
| int2 | VK_FORMAT_R32G32_SINT |
| int3 | VK_FORMAT_R32G32B32_SINT |
| int4 | VK_FORMAT_R32G32B32A32_SINT |
| uint | VK_FORMAT_R32_UINT |
| uint2 | VK_FORMAT_R32G32_UINT |
| uint3 | VK_FORMAT_R32G32B32_UINT |
| uint4 | VK_FORMAT_R32G32B32A32_UINT |

## Error Handling

### Compilation Errors

**Slang Compilation Failures:**
- Capture diagnostics from Slang's IBlob
- Log with PXT_ERROR including file path, line number, error description
- Return empty CompileResult with success=false
- Preserve existing shader module if reloading fails

**GLSL Compilation Failures:**
- Existing shaderc error handling unchanged
- Continue using PXT_FATAL for critical errors

### Reflection Extraction Errors

**Missing Reflection Data:**
- Log warning if reflection expected but not available
- Fall back to manual configuration
- Continue pipeline creation with manual settings

**Type Mapping Failures:**
- Log error for unmapped Slang types
- Use sensible defaults where possible
- Fail pipeline creation if critical mapping missing

### Binding Conflicts

**Cross-Stage Binding Mismatches:**
- Detect when same binding has different types in different stages
- Log detailed error with both conflicting bindings
- Fail pipeline creation (cannot resolve automatically)

**Example:**
```
PXT_ERROR("Descriptor binding conflict at set=0, binding=1:");
PXT_ERROR("  Vertex shader: VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER");
PXT_ERROR("  Fragment shader: VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE");
```

### Validation Failures

**Pipeline Validation:**
- Validate all required stages present
- Validate vertex input compatibility with vertex shader
- Validate descriptor set layouts are valid
- Log specific validation failure with context

**Descriptor Set Validation:**
- Validate binding numbers don't exceed limits
- Validate descriptor counts within limits
- Validate descriptor types supported by device

## Testing Strategy

### Unit Tests

**SlangCompiler Tests:**
- Test session initialization
- Test compilation of valid Slang shaders
- Test compilation error handling
- Test include file resolution
- Test preprocessor definition handling
- Test stage inference from file extensions

**ReflectionExtractor Tests:**
- Test extraction of descriptor bindings
- Test extraction of push constants
- Test extraction of vertex attributes
- Test type mapping (Slang → Vulkan)
- Test handling of arrays and structs
- Test handling of nested resources

**PipelineBuilder Tests:**
- Test merging bindings from multiple shaders
- Test binding conflict detection
- Test vertex input generation
- Test push constant range generation
- Test descriptor set layout generation

**DescriptorSetBuilder Tests:**
- Test conversion to DescriptorEntry format
- Test integration with DescriptorManager
- Test descriptor set creation from reflection

### Integration Tests

**End-to-End Compilation:**
- Compile simple Slang shader → verify SPIR-V output
- Compile shader with uniforms → verify reflection data
- Compile shader with textures → verify descriptor bindings
- Compile compute shader → verify workgroup size extraction

**Pipeline Creation:**
- Create graphics pipeline from Slang shaders → verify success
- Create ray tracing pipeline from Slang shaders → verify success
- Create compute pipeline from Slang shader → verify success
- Mix GLSL and Slang shaders → verify backward compatibility

**Shader Reloading:**
- Load shader → modify → reload → verify new reflection
- Reload with binding changes → verify descriptor sets updated
- Reload with errors → verify fallback to previous version

### Manual Testing

**Visual Verification:**
- Render existing scenes with Slang shaders
- Verify identical output to GLSL versions
- Test shader hot-reload in editor
- Test all render systems (material, ray tracing, shadows, etc.)

**Performance Testing:**
- Measure compilation time: Slang vs shaderc
- Measure reflection extraction time
- Measure pipeline creation time with reflection
- Verify offline compilation performance acceptable

### Test Data

**Sample Shaders:**
- Simple vertex + fragment shader
- Shader with multiple descriptor sets
- Shader with push constants
- Shader with vertex attributes
- Compute shader with storage buffers
- Ray tracing shader set (rgen, rchit, rmiss)
- Shader with nested structs
- Shader with texture arrays

## Performance Considerations

### Compilation Performance

**Runtime Compilation:**
- Target: <100ms for typical shader (200 lines)
- Slang session reuse critical for performance
- Module caching within session
- Parallel compilation for independent shaders

**Offline Compilation:**
- Target: <10s for all ~50 engine shaders
- Incremental compilation (only modified files)
- Parallel compilation via CMake
- Pre-compiled shaders preferred for release builds

### Reflection Extraction Performance

**Target:** <10ms per shader for reflection extraction

**Optimization Strategies:**
- Cache reflection data with shader module
- Avoid redundant traversals of reflection structures
- Use flat data structures (vectors) over nested maps
- Lazy extraction: only extract what's needed

### Pipeline Creation Performance

**Target:** <5ms for descriptor set layout generation

**Optimization Strategies:**
- Cache descriptor set layouts by binding signature
- Reuse layouts across pipelines when possible
- Merge bindings once, cache result
- Avoid redundant validation

### Memory Considerations

**Slang Session:**
- Single global session (singleton)
- Session caches all loaded modules
- Memory grows with unique shader variants
- Consider session reset for long-running applications

**Reflection Data:**
- Stored per shader module
- Relatively small (<1KB per shader typically)
- Lifetime tied to shader module
- Acceptable overhead for automation benefits

## Migration Strategy

### Phase 1: Infrastructure (Week 1-2)

**Goals:**
- Integrate Slang library into build system
- Implement SlangCompiler class
- Implement ShaderReflection and ReflectionExtractor
- Add Slang compilation path to VulkanShader
- Maintain full backward compatibility

**Deliverables:**
- Slang library linked and headers available
- SlangCompiler compiles simple shaders
- Reflection extraction working for basic cases
- VulkanShader detects and compiles .slang files
- All existing GLSL shaders continue working

### Phase 2: Reflection-Driven Configuration (Week 3-4)

**Goals:**
- Implement PipelineBuilder
- Implement DescriptorSetBuilder
- Update Pipeline class to use reflection
- Update render systems to use reflection (optional)

**Deliverables:**
- PipelineBuilder generates configurations from reflection
- Descriptor sets created from reflection
- At least one render system using reflection
- Manual configuration still supported

### Phase 3: Offline Compilation (Week 5)

**Goals:**
- Update CompileShaders.cmake for Slang
- Support both GLSL and Slang in build
- Test incremental compilation
- Verify pre-compiled shader loading

**Deliverables:**
- CMake compiles .slang files to .spv
- Incremental compilation working
- Pre-compiled shaders load correctly
- Build time acceptable

### Phase 4: Testing and Validation (Week 6)

**Goals:**
- Comprehensive unit tests
- Integration tests
- Performance validation
- Documentation

**Deliverables:**
- Test suite passing
- Performance targets met
- Migration guide written
- API documentation complete

### Phase 5: Shader Conversion (Week 7+)

**Goals:**
- Convert existing GLSL shaders to Slang
- Validate visual output matches
- Remove GLSL shaders once converted
- Remove shaderc dependency (optional)

**Deliverables:**
- All shaders converted to Slang
- Visual parity verified
- GLSL support can be removed (or kept for compatibility)

## Backward Compatibility

### GLSL Shader Support

**Detection:**
- File extensions: `.vert`, `.frag`, `.comp`, `.rgen`, `.rchit`, `.rmiss`, etc. (without `.slang` prefix)
- Explicit `.glsl` extension

**Compilation:**
- Route to existing shaderc path
- No reflection data generated
- Manual pipeline configuration required

**Coexistence:**
- GLSL and Slang shaders can coexist
- Pipelines can mix GLSL and Slang shaders
- Reflection only available for Slang shaders
- Manual config used when reflection unavailable

### Gradual Migration

**Render System Updates:**
- Render systems can opt-in to reflection gradually
- PipelineBuilder supports manual overrides
- Existing manual configuration continues working
- No breaking changes to existing code

**Example Migration Path:**
```cpp
// Before (manual configuration):
RasterizationPipelineConfigInfo config{};
config.bindingDescriptions = {...};
config.attributeDescriptions = {...};
config.pipelineLayout = manualLayout;

// After (reflection-driven):
PipelineBuilder builder;
builder.addShader(vertShader);
builder.addShader(fragShader);
auto layout = builder.createPipelineLayout(context, descriptorSetLayouts);
config.pipelineLayout = layout;
// Vertex input auto-configured from reflection
```

## Documentation Requirements

### API Documentation

**SlangCompiler:**
- Class overview and purpose
- Method documentation with parameters and return values
- Usage examples
- Error handling patterns

**ShaderReflection:**
- Data structure documentation
- Query method documentation
- Compatibility checking explanation

**PipelineBuilder:**
- Builder pattern usage
- Reflection-driven workflow
- Manual override examples
- Validation and error handling

### Migration Guide

**Content:**
- Overview of Slang benefits
- File naming conventions (.slang.vert, .slang.frag, etc.)
- GLSL to Slang conversion examples
- Common pitfalls and solutions
- Performance considerations
- Testing recommendations

**Examples:**
- Converting simple vertex/fragment shader
- Converting shader with uniforms
- Converting shader with textures
- Converting compute shader
- Converting ray tracing shaders

### User Guide

**Content:**
- How to write Slang shaders for PXT Engine
- Descriptor binding conventions
- Push constant usage
- Vertex attribute conventions
- Include file organization
- Shader hot-reload workflow

## Security Considerations

**Shader Source Validation:**
- Slang compiler validates syntax and semantics
- No arbitrary code execution risk (shaders run on GPU)
- Include file paths restricted to configured search paths

**Reflection Data Trust:**
- Reflection data comes from Slang compiler (trusted)
- No user-provided reflection data
- Validation of reflection data before use

**Build System:**
- Offline compilation runs during build (trusted environment)
- No runtime compilation of untrusted shaders
- Pre-compiled shaders verified during load

## Future Enhancements

### Shader Variants

**Specialization Constants:**
- Use Slang's specialization support
- Generate shader variants at runtime
- Reduce shader permutations

**Preprocessor Reduction:**
- Replace #ifdef with Slang generics
- Improve compile-time performance
- Better type safety

### Advanced Reflection

**Uniform Buffer Introspection:**
- Expose uniform buffer member layouts
- Enable automatic uniform updates
- Reduce manual offset calculations

**Shader Debugging:**
- Integrate Slang's debugging features
- Source-level shader debugging
- Better error messages

### Cross-Platform

**Additional Targets:**
- DXIL for DirectX 12
- Metal for macOS/iOS
- WGSL for WebGPU
- Single shader source for all platforms

### Tooling

**Shader Editor:**
- Syntax highlighting for Slang
- Real-time compilation feedback
- Reflection data visualization

**Shader Profiler:**
- Integration with Tracy profiler
- Per-shader compilation metrics
- Reflection extraction metrics

---

**Design Status:** Complete
**Next Phase:** Implementation (Phase 1: Infrastructure)
**Estimated Timeline:** 6-7 weeks for full implementation and testing
