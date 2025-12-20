// RealCraft Rendering System
// mesh_generator.hpp - Voxel mesh generation with greedy meshing

#pragma once

#include "chunk_mesh.hpp"
#include "mesh_vertex.hpp"

#include <cstdint>
#include <memory>
#include <realcraft/world/block.hpp>
#include <realcraft/world/chunk.hpp>

namespace realcraft::rendering {

// Configuration for mesh generation
struct MeshGeneratorConfig {
    bool enable_greedy_meshing = true;
    bool enable_ambient_occlusion = true;
    uint8_t ao_strength = 64;  // How dark AO gets (0-255)
};

// Generates chunk meshes from voxel data
// Thread-safe: does not access GPU resources
class MeshGenerator {
public:
    explicit MeshGenerator(const MeshGeneratorConfig& config = {});
    ~MeshGenerator();

    // Non-copyable, non-movable
    MeshGenerator(const MeshGenerator&) = delete;
    MeshGenerator& operator=(const MeshGenerator&) = delete;

    // Configuration
    void set_config(const MeshGeneratorConfig& config);
    [[nodiscard]] const MeshGeneratorConfig& get_config() const;

    // Generate mesh for a chunk
    // Neighbors are optional but improve cross-chunk face culling and AO
    // Returns true if mesh was generated (false if chunk is empty)
    bool generate(const world::Chunk& chunk, const world::Chunk* neighbor_neg_x, const world::Chunk* neighbor_pos_x,
                  const world::Chunk* neighbor_neg_z, const world::Chunk* neighbor_pos_z, ChunkMeshData& out_data,
                  ChunkMeshStats& out_stats);

    // Simplified version without neighbors
    bool generate(const world::Chunk& chunk, ChunkMeshData& out_data, ChunkMeshStats& out_stats) {
        return generate(chunk, nullptr, nullptr, nullptr, nullptr, out_data, out_stats);
    }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::rendering
