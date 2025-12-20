// RealCraft Rendering System
// chunk_mesh.hpp - GPU mesh data for a single chunk

#pragma once

#include "mesh_vertex.hpp"

#include <cstdint>
#include <memory>
#include <realcraft/graphics/buffer.hpp>
#include <realcraft/graphics/device.hpp>
#include <vector>

namespace realcraft::rendering {

// CPU-side mesh data before GPU upload
struct ChunkMeshData {
    std::vector<VoxelVertex> opaque_vertices;
    std::vector<uint32_t> opaque_indices;
    std::vector<VoxelVertex> transparent_vertices;
    std::vector<uint32_t> transparent_indices;

    [[nodiscard]] bool is_empty() const { return opaque_vertices.empty() && transparent_vertices.empty(); }

    void clear() {
        opaque_vertices.clear();
        opaque_indices.clear();
        transparent_vertices.clear();
        transparent_indices.clear();
    }

    void shrink_to_fit() {
        opaque_vertices.shrink_to_fit();
        opaque_indices.shrink_to_fit();
        transparent_vertices.shrink_to_fit();
        transparent_indices.shrink_to_fit();
    }
};

// Mesh generation statistics
struct ChunkMeshStats {
    uint32_t opaque_vertex_count = 0;
    uint32_t opaque_index_count = 0;
    uint32_t transparent_vertex_count = 0;
    uint32_t transparent_index_count = 0;
    uint32_t quads_merged = 0;  // From greedy meshing
    double generation_time_ms = 0.0;
};

// GPU-resident mesh for a single chunk
class ChunkMesh {
public:
    ChunkMesh() = default;
    ~ChunkMesh() = default;

    // Non-copyable, movable
    ChunkMesh(const ChunkMesh&) = delete;
    ChunkMesh& operator=(const ChunkMesh&) = delete;
    ChunkMesh(ChunkMesh&&) noexcept = default;
    ChunkMesh& operator=(ChunkMesh&&) noexcept = default;

    // Upload mesh data to GPU buffers
    // Must be called from the main thread
    bool upload(graphics::GraphicsDevice* device, const ChunkMeshData& data);

    // Release GPU resources
    void release();

    // Check mesh state
    [[nodiscard]] bool has_opaque() const { return opaque_index_count_ > 0; }
    [[nodiscard]] bool has_transparent() const { return transparent_index_count_ > 0; }
    [[nodiscard]] bool is_empty() const { return !has_opaque() && !has_transparent(); }
    [[nodiscard]] bool is_uploaded() const {
        return opaque_vertex_buffer_ != nullptr || transparent_vertex_buffer_ != nullptr;
    }

    // Get GPU buffers for rendering
    [[nodiscard]] graphics::Buffer* get_opaque_vertex_buffer() const { return opaque_vertex_buffer_.get(); }
    [[nodiscard]] graphics::Buffer* get_opaque_index_buffer() const { return opaque_index_buffer_.get(); }
    [[nodiscard]] graphics::Buffer* get_transparent_vertex_buffer() const { return transparent_vertex_buffer_.get(); }
    [[nodiscard]] graphics::Buffer* get_transparent_index_buffer() const { return transparent_index_buffer_.get(); }

    // Get counts for draw calls
    [[nodiscard]] uint32_t get_opaque_index_count() const { return opaque_index_count_; }
    [[nodiscard]] uint32_t get_transparent_index_count() const { return transparent_index_count_; }

    // Get statistics
    [[nodiscard]] const ChunkMeshStats& get_stats() const { return stats_; }
    void set_stats(const ChunkMeshStats& stats) { stats_ = stats; }

private:
    std::unique_ptr<graphics::Buffer> opaque_vertex_buffer_;
    std::unique_ptr<graphics::Buffer> opaque_index_buffer_;
    std::unique_ptr<graphics::Buffer> transparent_vertex_buffer_;
    std::unique_ptr<graphics::Buffer> transparent_index_buffer_;

    uint32_t opaque_index_count_ = 0;
    uint32_t transparent_index_count_ = 0;

    ChunkMeshStats stats_;
};

}  // namespace realcraft::rendering
