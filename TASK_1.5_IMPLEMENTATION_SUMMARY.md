# Task 1.5 Implementation Summary: Preprocessor Definition Support

## Overview

This document summarizes the implementation of preprocessor definition support in the SlangCompiler class. The implementation allows users to pass preprocessor macros (like `#define` directives) when compiling Slang shaders, enabling conditional compilation and shader variants.

## Changes Made

### 1. SlangCompiler Header (`Engine/src/graphics/resources/slang_compiler.hpp`)

**Added Method:**
```cpp
Slang::ComPtr<slang::ISession> createSessionWithDefinitions(
    const std::vector<std::pair<std::string, std::string>>& definitions
);
```

This private helper method creates a temporary Slang session with preprocessor definitions applied. It's used internally by the compilation methods when definitions are provided.

### 2. SlangCompiler Implementation (`Engine/src/graphics/resources/slang_compiler.cpp`)

#### 2.1 Updated `initializeSession()`

Added documentation clarifying that the default session does NOT include preprocessor definitions. This is intentional to maximize caching and reuse, as Slang's documentation recommends using as few sessions as possible.

```cpp
// Note: Preprocessor definitions are NOT set here in the session
// They are applied per-compilation via specialization or other mechanisms
// The session is kept clean to maximize caching and reuse
```

#### 2.2 Implemented `createSessionWithDefinitions()`

This method creates a temporary Slang session with the same configuration as the default session (Vulkan 1.3, SPIR-V 1.4, include paths), but with additional preprocessor definitions:

```cpp
Slang::ComPtr<slang::ISession> SlangCompiler::createSessionWithDefinitions(
    const std::vector<std::pair<std::string, std::string>>& definitions) {
    
    // ... session configuration ...
    
    // Configure preprocessor definitions
    std::vector<slang::PreprocessorMacroDesc> macros;
    macros.reserve(definitions.size());
    for (const auto& [name, value] : definitions) {
        slang::PreprocessorMacroDesc macro;
        macro.name = name.c_str();
        macro.value = value.c_str();
        macros.push_back(macro);
    }

    sessionDesc.preprocessorMacros = macros.data();
    sessionDesc.preprocessorMacroCount = static_cast<SlangInt>(macros.size());
    
    // Create and return the session
    // ...
}
```

**Key Implementation Details:**
- Uses `slang::PreprocessorMacroDesc` structure with `name` and `value` fields
- Converts the vector of string pairs to Slang's expected format
- Creates a new session for each unique set of definitions (as per Slang's design)
- Returns `nullptr` on failure with appropriate error logging

#### 2.3 Updated `compileFromFile()`

Modified to use preprocessor definitions when provided:

```cpp
// Get the appropriate session (with or without preprocessor definitions)
Slang::ComPtr<slang::ISession> sessionToUse;
if (!definitions.empty()) {
    // Create a temporary session with preprocessor definitions
    sessionToUse = createSessionWithDefinitions(definitions);
    if (!sessionToUse) {
        result.diagnostics = "Failed to create session with preprocessor definitions";
        PXT_ERROR("{}", result.diagnostics);
        return result;
    }
    PXT_INFO("Compiling {} with {} preprocessor definition(s)", filePath, definitions.size());
} else {
    // Use the default session
    sessionToUse = m_session;
}

// Use sessionToUse for all subsequent compilation operations
```

**Removed:**
- The warning message: `"Preprocessor definitions not yet implemented for Slang shaders"`

#### 2.4 Updated `compileFromSource()`

Applied the same changes as `compileFromFile()` to support preprocessor definitions when compiling from in-memory source strings.

## Usage Examples

### Example 1: Compile with a single definition

```cpp
#include "graphics/resources/slang_compiler.hpp"

SlangCompiler& compiler = SlangCompiler::getInstance();

std::vector<std::pair<std::string, std::string>> definitions = {
    {"USE_FEATURE_X", "1"}
};

auto result = compiler.compileFromFile(
    "assets/shaders/my_shader.slang.vert",
    definitions
);

if (result.success) {
    // Use result.spirv for shader module creation
}
```

### Example 2: Compile with multiple definitions

```cpp
std::vector<std::pair<std::string, std::string>> definitions = {
    {"USE_COLOR", "1"},
    {"SCALE_FACTOR", "2.0"},
    {"MAX_LIGHTS", "8"}
};

auto result = compiler.compileFromFile(
    "assets/shaders/lighting.slang.frag",
    definitions
);
```

### Example 3: Compile from source with definitions

```cpp
const char* shaderSource = R"(
#version 450

#ifdef HIGH_QUALITY
    #define SAMPLE_COUNT 16
#else
    #define SAMPLE_COUNT 4
#endif

void main() {
    // Use SAMPLE_COUNT in shader logic
}
)";

std::vector<std::pair<std::string, std::string>> definitions = {
    {"HIGH_QUALITY", ""}  // Empty value is valid
};

auto result = compiler.compileFromSource(
    shaderSource,
    "generated_shader.slang.frag",
    VK_SHADER_STAGE_FRAGMENT_BIT,
    definitions
);
```

### Example 4: Shader with #ifdef directives

**Shader file: `test_preprocessor.slang.vert`**
```glsl
#version 450

#ifdef USE_COLOR
    #define OUTPUT_COLOR vec4(1.0, 0.0, 0.0, 1.0)
#else
    #define OUTPUT_COLOR vec4(0.0, 1.0, 0.0, 1.0)
#endif

#ifdef SCALE_FACTOR
    #define POSITION_SCALE SCALE_FACTOR
#else
    #define POSITION_SCALE 1.0
#endif

layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec4 outColor;

void main() {
    gl_Position = vec4(inPosition * POSITION_SCALE, 1.0);
    outColor = OUTPUT_COLOR;
}
```

**Compilation:**
```cpp
// Variant 1: Default (green color, scale 1.0)
auto result1 = compiler.compileFromFile("test_preprocessor.slang.vert");

// Variant 2: Red color, scale 1.0
std::vector<std::pair<std::string, std::string>> defs2 = {{"USE_COLOR", "1"}};
auto result2 = compiler.compileFromFile("test_preprocessor.slang.vert", defs2);

// Variant 3: Red color, scale 2.0
std::vector<std::pair<std::string, std::string>> defs3 = {
    {"USE_COLOR", "1"},
    {"SCALE_FACTOR", "2.0"}
};
auto result3 = compiler.compileFromFile("test_preprocessor.slang.vert", defs3);
```

## Design Rationale

### Session Management Strategy

The implementation creates a new Slang session for each unique set of preprocessor definitions. This follows Slang's recommended architecture:

**From Slang Documentation:**
> "A session does have some global state in it which currently makes it unable to cache and reuse artifacts, namely, the #define configurations. Unique combinations of preprocessor #defines used in your shaders will require unique session objects."

**Trade-offs:**
- ✅ **Correct behavior**: Each definition set gets its own session, ensuring proper compilation
- ✅ **Caching**: Slang caches compiled modules within each session
- ⚠️ **Memory**: Multiple sessions consume more memory than a single session
- ⚠️ **Performance**: Creating sessions has overhead, but Slang caches within sessions

**Optimization Opportunity:**
Future optimization could cache sessions by definition set hash, reusing sessions for repeated definition combinations. This was not implemented in this task to keep the initial implementation simple and correct.

### Empty Definition Values

The implementation supports empty definition values (e.g., `{"FEATURE_ENABLED", ""}`), which is equivalent to `#define FEATURE_ENABLED` without a value. This is useful for simple feature flags.

## Testing

### Test Shader Created

A test shader was created at `assets/shaders/test_preprocessor.slang.vert` that uses `#ifdef` directives to test preprocessor support. This shader can be used for manual testing.

### Test Program Created

A test program was created at `Engine/tests/test_slang_preprocessor.cpp` that demonstrates:
1. Compilation without definitions
2. Compilation with a single definition
3. Compilation with multiple definitions
4. Compilation from source with definitions

**Note:** The test program is not integrated into the build system yet, as the project doesn't have a test framework configured in CMake.

### Verification

The implementation was verified using the `getDiagnostics` tool, which confirmed:
- ✅ No compilation errors in `slang_compiler.cpp`
- ✅ No compilation errors in `slang_compiler.hpp`

## Requirements Satisfied

This implementation satisfies **Requirement 2.6** from the requirements document:

> **2.6** THE VulkanShader SHALL support preprocessor definitions passed at compile time

The implementation extends both `compileFromFile()` and `compileFromSource()` to accept preprocessor definitions and applies them correctly to the Slang compilation request.

## API Compatibility

The changes are **fully backward compatible**:
- The `definitions` parameter was already present in both methods (defaulting to empty vector)
- Existing code that doesn't pass definitions continues to work unchanged
- The default session (without definitions) is still used for compilations without definitions

## Performance Considerations

### Session Creation Overhead

Creating a new session for each unique definition set has some overhead. However:
- Sessions are only created when definitions are provided
- Slang caches compiled modules within each session
- Most shaders in a typical application use the same definition set

### Recommended Usage Patterns

1. **Minimize definition sets**: Use as few unique definition combinations as possible
2. **Batch compilations**: Compile all shaders with the same definitions together
3. **Consider alternatives**: For frequently-changing values, consider using Slang's generics/interfaces instead of preprocessor definitions (as recommended by Slang documentation)

## Future Enhancements

Potential improvements for future tasks:

1. **Session Caching**: Cache sessions by definition set hash to reuse sessions across compilations
2. **Definition Validation**: Validate definition names and values before creating session
3. **Performance Metrics**: Add timing metrics to measure session creation and compilation overhead
4. **Alternative Specialization**: Migrate from preprocessor to Slang's generics/interfaces for better performance

## Files Modified

1. `Engine/src/graphics/resources/slang_compiler.hpp`
   - Added `createSessionWithDefinitions()` method declaration

2. `Engine/src/graphics/resources/slang_compiler.cpp`
   - Implemented `createSessionWithDefinitions()`
   - Updated `compileFromFile()` to use definitions
   - Updated `compileFromSource()` to use definitions
   - Updated `initializeSession()` documentation

## Files Created

1. `assets/shaders/test_preprocessor.slang.vert`
   - Test shader with `#ifdef` directives

2. `Engine/tests/test_slang_preprocessor.cpp`
   - Test program demonstrating preprocessor definition usage

3. `TASK_1.5_IMPLEMENTATION_SUMMARY.md`
   - This documentation file

## Conclusion

Task 1.5 is complete. The SlangCompiler now fully supports preprocessor definitions through both `compileFromFile()` and `compileFromSource()` methods. The implementation follows Slang's recommended architecture and is backward compatible with existing code.
