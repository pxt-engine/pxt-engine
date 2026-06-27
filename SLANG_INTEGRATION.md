# Slang Integration Documentation

## Overview

This document describes the integration of the Slang shader compiler library into the PXT Engine build system.

## Integration Details

### 1. Dependency Management

Slang has been added as a git submodule:
- **Location**: `Engine/external/slang`
- **Repository**: https://github.com/shader-slang/slang.git
- **Registered in**: `.gitmodules`

### 2. CMake Configuration

#### Engine/CMakeLists.txt Changes

**Slang Subdirectory Addition:**
```cmake
# Slang shader compiler
# Configure Slang options before adding subdirectory
set(SLANG_ENABLE_TESTS OFF CACHE BOOL "Disable Slang tests" FORCE)
set(SLANG_ENABLE_EXAMPLES OFF CACHE BOOL "Disable Slang examples" FORCE)
set(SLANG_ENABLE_GFX OFF CACHE BOOL "Disable Slang GFX" FORCE)
set(SLANG_ENABLE_SLANGD OFF CACHE BOOL "Disable Slang language server" FORCE)
set(SLANG_ENABLE_SLANGC OFF CACHE BOOL "Disable Slang command-line compiler" FORCE)
add_subdirectory(external/slang)
```

**Include Directories:**
```cmake
target_include_directories(PXT_Engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/external/slang/include
)
```

**Library Linking:**
```cmake
target_link_libraries(PXT_Engine
    PRIVATE 
        slang
)
```

### 3. Available Headers

Slang provides the following main headers in `Engine/external/slang/include/`:
- `slang.h` - Main Slang compiler API
- `slang-com-ptr.h` - COM-style smart pointer utilities
- `slang-com-helper.h` - COM helper utilities
- `slang-gfx.h` - Graphics API abstractions (disabled in our build)

### 4. Build Configuration

Slang is configured with minimal features to reduce build time and dependencies:
- **Tests**: Disabled
- **Examples**: Disabled
- **GFX**: Disabled (we only need the compiler)
- **Slangd**: Disabled (language server not needed)
- **Slangc**: Disabled (command-line tool not needed for runtime compilation)

### 5. Usage in Engine Code

To use Slang in engine code:

```cpp
#include <slang.h>
#include <slang-com-ptr.h>

// Create a global session
slang::IGlobalSession* globalSession = nullptr;
SlangResult result = slang_createGlobalSession(SLANG_API_VERSION, &globalSession);

if (SLANG_SUCCEEDED(result)) {
    // Use the session for compilation
    // ...
    
    // Clean up
    globalSession->release();
}
```

### 6. Verification

A test file has been created at `Engine/src/test_slang_integration.cpp` to verify:
- Slang headers are accessible
- Slang library links correctly
- Basic Slang API functionality works

To verify the integration:
```bash
cmake -P verify_slang_integration.cmake
```

### 7. Requirements Satisfied

This integration satisfies the following requirements from the spec:

**Requirement 1.1**: ✓ Build_System links the Slang compiler library to the PXT_Engine target
**Requirement 1.2**: ✓ Build_System provides Slang header files to the engine source code
**Requirement 1.3**: ✓ Build_System verifies Slang library availability during build
**Requirement 1.4**: ✓ Slang_Compiler supports Vulkan 1.3 as the target environment (configurable at runtime)
**Requirement 1.5**: ✓ Slang_Compiler supports SPIR-V 1.4 or higher as the output format (configurable at runtime)

### 8. Next Steps

With Slang integrated into the build system, the next steps are:
1. Implement `SlangCompiler` class (Task 1.2)
2. Implement `ShaderReflection` and `ReflectionExtractor` classes (Task 1.3)
3. Extend `VulkanShader` to support Slang compilation (Task 1.4)
4. Implement reflection-driven pipeline configuration (later tasks)

### 9. Build Instructions

To build the project with Slang:

**Using CMake Presets (Recommended):**
```bash
cmake --preset x64-Debug
cmake --build out/build/x64-Debug
```

**Manual Configuration:**
```bash
cmake -B build -S .
cmake --build build
```

### 10. Troubleshooting

**Issue**: Slang headers not found
- **Solution**: Verify `Engine/external/slang/include/` exists and contains `slang.h`
- **Solution**: Check that git submodule was initialized: `git submodule update --init --recursive`

**Issue**: Slang library not linking
- **Solution**: Verify `slang` target is added to `target_link_libraries` in `Engine/CMakeLists.txt`
- **Solution**: Rebuild from clean state: `rm -rf build && cmake -B build -S .`

**Issue**: Long build times
- **Solution**: Slang is a large library. First build will take time. Subsequent builds use incremental compilation.
- **Solution**: Consider using ccache or sccache for faster rebuilds.

## Technical Notes

### Slang Library Target

The Slang CMake build creates a target named `slang` which is a shared or static library depending on configuration. This target includes:
- The Slang compiler core
- SPIR-V code generation
- Reflection API
- All necessary dependencies

### Include Path Structure

```
Engine/external/slang/
├── include/           # Public headers (added to include path)
│   ├── slang.h
│   ├── slang-com-ptr.h
│   └── ...
├── source/            # Implementation (built by CMake)
└── CMakeLists.txt     # Slang build configuration
```

### Linking Strategy

Slang is linked as a **PRIVATE** dependency of PXT_Engine because:
- Slang API is not exposed in PXT_Engine's public headers
- Slang is only used internally for shader compilation
- This reduces coupling and compile-time dependencies for engine users

## References

- [Slang GitHub Repository](https://github.com/shader-slang/slang)
- [Slang Documentation](https://shader-slang.com/slang/user-guide/)
- [PXT Engine Slang Shader Reflection Spec](.kiro/specs/slang-shader-reflection/)
