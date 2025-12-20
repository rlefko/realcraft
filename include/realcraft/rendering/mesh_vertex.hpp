// RealCraft Rendering System
// mesh_vertex.hpp - Voxel vertex format definition

#pragma once

#include <cstdint>
#include <realcraft/graphics/types.hpp>
#include <vector>

namespace realcraft::rendering {

// Compact vertex format for chunk meshes (36 bytes, GPU-aligned for vec4 position)
struct VoxelVertex {
    // Position relative to chunk origin (16 bytes - padded for GPU alignment)
    float position[4];  // xyz position, w unused (set to 1.0)

    // Packed normal direction (3 bytes) + AO (1 byte)
    // Normal: -127 to 127, normalized in shader
    int8_t normal[3];
    uint8_t ao;  // Ambient occlusion 0-255

    // Texture coordinates in atlas (8 bytes)
    float uv[2];

    // Texture index in atlas (2 bytes)
    uint16_t texture_index;

    // Light levels (2 bytes)
    uint8_t light_sky;    // Sky light 0-15
    uint8_t light_block;  // Block light 0-15

    // Biome color tint (4 bytes)
    uint8_t color[4];  // RGBA

    // Pack a normal vector into int8 components
    static void pack_normal(float nx, float ny, float nz, int8_t out[3]) {
        out[0] = static_cast<int8_t>(nx * 127.0f);
        out[1] = static_cast<int8_t>(ny * 127.0f);
        out[2] = static_cast<int8_t>(nz * 127.0f);
    }

    // Create vertex attribute descriptions for pipeline creation
    static std::vector<graphics::VertexAttribute> get_attributes() {
        return {
            // location 0: position (vec4, xyz used, w=1)
            {0, 0, graphics::TextureFormat::RGBA32Float, 0},
            // location 1: normal + ao (vec4 as bytes, interpreted as signed/unsigned)
            {1, 0, graphics::TextureFormat::RGBA8Sint, 16},
            // location 2: uv (vec2)
            {2, 0, graphics::TextureFormat::RG32Float, 20},
            // location 3: texture_index + lights (packed as uint)
            {3, 0, graphics::TextureFormat::RGBA16Uint, 28},
            // location 4: color (vec4 normalized)
            {4, 0, graphics::TextureFormat::RGBA8Unorm, 32},
        };
    }

    // Create vertex binding description
    static graphics::VertexBinding get_binding() { return {0, sizeof(VoxelVertex), false}; }
};

static_assert(sizeof(VoxelVertex) == 36, "VoxelVertex must be 36 bytes for GPU alignment");

// Face direction for mesh generation
enum class FaceDirection : uint8_t {
    NegX = 0,  // West  (-X)
    PosX = 1,  // East  (+X)
    NegY = 2,  // Down  (-Y)
    PosY = 3,  // Up    (+Y)
    NegZ = 4,  // North (-Z)
    PosZ = 5,  // South (+Z)
    Count = 6
};

// Get normal vector for a face direction
inline void get_face_normal(FaceDirection dir, float& nx, float& ny, float& nz) {
    switch (dir) {
        case FaceDirection::NegX:
            nx = -1.0f;
            ny = 0.0f;
            nz = 0.0f;
            break;
        case FaceDirection::PosX:
            nx = 1.0f;
            ny = 0.0f;
            nz = 0.0f;
            break;
        case FaceDirection::NegY:
            nx = 0.0f;
            ny = -1.0f;
            nz = 0.0f;
            break;
        case FaceDirection::PosY:
            nx = 0.0f;
            ny = 1.0f;
            nz = 0.0f;
            break;
        case FaceDirection::NegZ:
            nx = 0.0f;
            ny = 0.0f;
            nz = -1.0f;
            break;
        case FaceDirection::PosZ:
            nx = 0.0f;
            ny = 0.0f;
            nz = 1.0f;
            break;
        default:
            nx = 0.0f;
            ny = 1.0f;
            nz = 0.0f;
            break;
    }
}

}  // namespace realcraft::rendering
