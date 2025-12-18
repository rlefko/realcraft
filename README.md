# RealCraft

A single-player voxel sandbox game combining the creative freedom of Minecraft with hyper-realistic graphics, physics, and world generation.

## Overview

RealCraft is an ambitious sandbox game built on a custom C++ engine. It features real-time path-traced lighting, physically-accurate structural simulation, and infinite procedurally-generated worlds shaped by hydraulic erosion algorithms. The game offers both Creative and Survival modes in a world that looks and behaves realistically while remaining fun and playable.

## Features

### Realistic Ray-Traced Graphics
- Real-time path tracing with global illumination
- Physically-based rendering (PBR) materials
- Volumetric fog, god rays, and atmospheric effects
- Accurate reflections, refractions, and caustics
- AI-powered denoising and upscaling (DLSS, FSR, MetalFX)

### Realistic Physics
- Structural integrity simulation - unsupported structures collapse
- Dynamic debris and destruction with physical fragments
- Fluid dynamics with natural water flow and buoyancy
- Material weight and strength properties affect gameplay

### Infinite Procedural World
- Chunk-based infinite terrain generation
- GPU-accelerated hydraulic erosion for realistic landscapes
- Diverse biomes with climate-based distribution
- Complex cave systems with underground rivers
- Realistic ore veins and vegetation placement

### Game Modes
- **Creative Mode**: Unlimited resources, flight, and building tools
- **Survival Mode**: Resource gathering, crafting, health/hunger management, and environmental challenges

## System Requirements

### Minimum
- macOS 13+ (Apple Silicon recommended) or Windows 10/Linux
- GPU with ray tracing support
- 16 GB RAM
- 8 GB VRAM

### Recommended
- macOS 14+ with M2/M3 chip, or Windows with RTX 3070+
- 32 GB RAM
- 12+ GB VRAM

## Building

*Build instructions coming soon.*

The project uses CMake and targets:
- **macOS**: Metal API (primary platform)
- **Windows/Linux**: Vulkan API

## Project Structure

```
realcraft/
├── src/           # Source files by subsystem
├── include/       # Public headers
├── shaders/       # MSL, HLSL, GLSL shaders
├── assets/        # Textures, sounds, data
├── tests/         # Unit and integration tests
├── tools/         # Build utilities
├── PRD.md         # Product Requirements Document
├── TDD.md         # Technical Design Document
└── MILESTONES.md  # Development roadmap
```

## Documentation

- [Product Requirements (PRD.md)](PRD.md) - Vision, features, and requirements
- [Technical Design (TDD.md)](TDD.md) - Architecture and implementation details
- [Milestones (MILESTONES.md)](MILESTONES.md) - Development phases and tasks

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.
