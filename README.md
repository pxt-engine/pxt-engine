# PXT Engine

A modern C++20 game engine based on Vulkan, with a hybrid rendering pipeline. Designed with a focus on photorealistic path tracing and an intuitive ECS-based editor.

## Key Features

### Rendering Architecture
- **Hybrid Pipeline**: Integrated Vulkan-based rasterizer for real-time feedback and a Path Tracer for high-fidelity results.
- **Path Tracing**: Surface Path Tracing utilizing a Disney-like BSDF (supporting reflection, refraction and trasmittance).
- **Volumetric rendering**: Advanced volumetric path tracing using Delta Tracking and FBM (Fractional Brownian Motion) for procedural density textures. Allowing the creation of volumes such as fog or clouds or, in combination with the surface path tracer, Sub-Surface Scattering phenomena.
- **Denoiser**: Simple Temporal and Spatial denoiser + Accumulation.

### Engine & Tools
- **ECS Architecture**: Powered by EnTT for high-performance entity management.
- **Editor Suite**: Full-featured UI using ImGui and ImGuizmo, including:
   - Scene Hierarchy & Entity Inspector: Real-time component and entity manipulation (add, edit, delete, copy).
   - Editor Console: Integrated logging system for real-time debugging.
   - Live Controls: Modify the objects using Guizmos, switch camera, Play/Stop mode.
- **Native Scripting**: C++20 scripting using the API provided by the engine.
- **Event System**
- **Input System**
- **Scene Serialization**
- **Resource Management**

### Roadmap
- **Job System**: (WIP)
- **Resource & Asset managing improvements**: (WIP)
- **C# Scripting**
- **Project management**
- **SDL**
- **RHI and Rendering layer abstraction**
- **Audio System**
- **Physics Engine**
 
## Disclaimer
This engine is under active development and intended for learning, experimentation, and research purposes.
APIs and features may change frequently.
