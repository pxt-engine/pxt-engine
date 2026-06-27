# Project Structure

## Root Organization

```
PXT/
├── Engine/          # Core engine library
├── Editor/          # Editor application
├── GameExample/     # Example game project
├── assets/          # Shared assets (shaders, models, textures, fonts)
├── build/           # CMake build directory (gitignored)
├── out/             # Compiled binaries and shaders (gitignored)
└── cmake/           # Custom CMake modules
```

## Engine Architecture (`Engine/src/`)

### Core Systems (`core/`)
- **pch.hpp** - Precompiled header with all common includes
- **constants.hpp** - Engine-wide constants
- **diagnostics.hpp** - Debugging and diagnostics utilities
- **logging/** - Logging system (spdlog wrapper)
- **events/** - Event system for engine and window events
- **input/** - Input handling system
- **uid.hpp** - Unique identifier generation
- **obj_picking_id.hpp** - Object picking system IDs

### Graphics (`graphics/`)
- **context/** - Vulkan context, device, instance, surface management
- **descriptors/** - Descriptor set management and allocation
- **render_systems/** - Specialized rendering systems:
  - Material rendering (PBR)
  - Ray tracing and path tracing
  - Shadow mapping
  - Point lights
  - Object picking
  - Selection masks
  - Editor grid
  - Denoising
  - Skybox
  - UI rendering layer
- **resources/** - Graphics resources:
  - Vulkan buffers, images, meshes, shaders, samplers
  - Texture registry and material registry
  - BLAS (Bottom-Level Acceleration Structure) registry
  - Cube maps and skyboxes
- **pipeline.hpp/cpp** - Graphics pipeline management
- **renderer.hpp/cpp** - Main renderer
- **swap_chain.hpp/cpp** - Swap chain management
- **window.hpp/cpp** - Window abstraction
- **frame_info.hpp** - Per-frame rendering data

### Scene Management (`scene/`)
- **ecs/** - Entity-Component System:
  - **component.hpp** - All component definitions
  - **entity.hpp** - Entity wrapper
  - **system.hpp** - System base classes
- **camera_data.hpp** - Camera configuration
- Scene serialization and management

### Resources (`resources/`)
- Asset management system
- **types/** - Resource types (Material, Mesh, Image)
- **asset_handle.hpp** - Handle-based resource access

### Concurrency (`concurrency/`)
- Job system (WIP)
- Thread management

### UI (`ui/`)
- ImGui integration and editor UI components

### Utilities (`utils/`)
- Helper functions and utilities

## External Dependencies (`Engine/external/`)

All third-party libraries are included as submodules or vendored:
- entt, glfw, glm, imgui, imguizmo, spdlog, stb, tinyfiledialogs, tinyobjloader, tracy, yaml-cpp

## Assets Organization (`assets/`)

```
assets/
├── fonts/           # UI fonts (Roboto, Lucide icons)
├── imgs/            # Documentation and preview images
├── imgui_config/    # ImGui configuration
├── models/          # 3D models (.obj files)
├── scenes/          # Saved scene files (.pxtscene)
├── shaders/         # GLSL shaders
│   ├── common/      # Shared shader code
│   ├── lighting/    # Lighting calculations
│   ├── material/    # Material shaders (PBR)
│   ├── post/        # Post-processing
│   ├── raytracing/  # Ray tracing shaders (.rgen, .rchit, .rmiss)
│   └── ubo/         # Uniform buffer object definitions
└── textures/        # Texture assets
    └── blue_noise/  # Blue noise textures for ray tracing
```

## Naming Conventions

### Files
- **Headers**: `.hpp` extension
- **Implementation**: `.cpp` extension
- **Vulkan-specific**: `vk_` prefix (e.g., `vk_buffer.hpp`, `vk_image.cpp`)
- **Shaders**: GLSL extensions (`.vert`, `.frag`, `.comp`, `.rgen`, `.rchit`, `.rmiss`)

### Code
- **Namespace**: All engine code in `pxt` namespace
- **Classes**: PascalCase (e.g., `VulkanBuffer`, `MaterialRenderSystem`)
- **Functions/Methods**: camelCase (e.g., `createPipeline()`, `onUpdate()`)
- **Member variables**: `m_` prefix (e.g., `m_context`, `m_renderer`)
- **Static members**: `s_` prefix (e.g., `s_defaultMaterial`)
- **Components**: Suffix with `Component` or `Tag` (e.g., `TransformComponent`, `RenderableTag`)
- **Systems**: Suffix with `System` (e.g., `MaterialRenderSystem`)

### Architecture Patterns
- **ECS Components**: Defined in `scene/ecs/component.hpp`
- **Builder Pattern**: Used for complex component construction (e.g., `MaterialComponent::Builder`)
- **Registry Pattern**: Used for resource management (TextureRegistry, MaterialRegistry, BLASRegistry)
- **Layer System**: Render systems organized as layers in a stack
- **Smart Pointers**: 
  - `Unique<T>` for exclusive ownership
  - `Shared<T>` for shared ownership
  - Defined in engine (likely aliases for std::unique_ptr/std::shared_ptr)
