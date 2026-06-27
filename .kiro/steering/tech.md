# Technology Stack

## Language & Standards

- **C++23** - Modern C++ with latest standard features
- **CMake 3.20+** - Build system

## Core Dependencies

### Graphics & Rendering
- **Vulkan** - Low-level graphics API with ray tracing extensions
- **GLFW** - Window creation and input handling
- **GLM** - Mathematics library for graphics (with Vulkan conventions)
- **shaderc** - Runtime shader compilation

### Entity-Component System
- **EnTT** - High-performance ECS library

### UI & Editor
- **ImGui** - Immediate mode GUI framework
- **ImGuizmo** - 3D gizmo manipulation widgets

### Asset Loading
- **tinyobjloader** - OBJ mesh file loading
- **stb_image** - Image loading (header-only)
- **yaml-cpp** - Scene serialization

### Utilities
- **spdlog** - Fast logging library (header-only mode)
- **Tracy** - Profiling and performance analysis
- **tinyfiledialogs** - Native file dialogs

## Build System

### Project Structure
- **Engine/** - Core engine static library (PXT_Engine)
- **Editor/** - Editor application static library (PXT_Editor)
- **GameExample/** - Example game project
- **assets/** - Shared assets (shaders, models, textures, fonts)

### Output Directories
- **out/** - Compiled binaries and libraries
- **out/shaders/** - Compiled SPIR-V shaders

### Common Commands

```bash
# Configure (from project root)
cmake -B build -S .

# Build all targets
cmake --build build

# Build specific target
cmake --build build --target PXT_Engine
cmake --build build --target PXT_Editor

# Build in Release mode
cmake --build build --config Release
```

### MSVC-Specific
- Static analysis enabled (`/analyze`)
- Warning 4099 suppressed (PDB debug info for shaderc)

## Shader Compilation

Shaders are automatically compiled from `assets/shaders/` to `out/shaders/` as SPIR-V during build via custom CMake module (`CompileShaders`).

## Precompiled Headers

Engine uses PCH for faster compilation: `Engine/src/core/pch.hpp`

## Validation Layers

- **Debug builds**: Vulkan validation layers enabled (`ENABLE_VALIDATION_LAYERS=1`)
- **Release builds**: Validation layers disabled

## Logging Levels

Global logging level set to `SPDLOG_LEVEL_TRACE` for all builds.
