# PXT Engine

A modern C++23 game engine built on Vulkan with a hybrid rendering pipeline combining real-time rasterization and photorealistic path tracing.

## Core Purpose

PXT Engine is designed for learning, experimentation, and research in advanced rendering techniques. It provides a full-featured editor with an ECS architecture for creating and manipulating 3D scenes with both real-time and high-fidelity rendering capabilities.

## Key Capabilities

- **Hybrid Rendering**: Vulkan-based rasterizer for real-time feedback + path tracer for photorealistic results
- **Advanced Path Tracing**: Surface path tracing with Disney-like BSDF (reflection, refraction, transmittance)
- **Volumetric Rendering**: Delta tracking and FBM for procedural fog, clouds, and sub-surface scattering
- **Denoising**: Temporal and spatial denoising with accumulation
- **ECS Architecture**: EnTT-powered entity-component system
- **Full Editor Suite**: ImGui-based editor with scene hierarchy, entity inspector, gizmos, and console
- **Native Scripting**: C++20 scripting API
- **Scene Serialization**: Save and load complete scenes

## Development Status

Active development - APIs and features may change frequently. Intended for learning and experimentation rather than production use.
