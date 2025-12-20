// RealCraft Rendering System
// mesh_generator.cpp - Voxel mesh generation with greedy meshing

#include <chrono>
#include <cstring>
#include <realcraft/core/logger.hpp>
#include <realcraft/rendering/mesh_generator.hpp>
#include <realcraft/world/types.hpp>

namespace realcraft::rendering {

struct MeshGenerator::Impl {
    MeshGeneratorConfig config;

    // Neighbor lookup for each face direction
    static constexpr glm::ivec3 FACE_OFFSETS[6] = {
        {-1, 0, 0},  // NegX
        {1, 0, 0},   // PosX
        {0, -1, 0},  // NegY
        {0, 1, 0},   // PosY
        {0, 0, -1},  // NegZ
        {0, 0, 1}    // PosZ
    };

    // Vertex positions for each face (4 vertices per face, CCW winding)
    // Positions are relative to block origin (0,0,0) to (1,1,1)
    static constexpr float FACE_VERTICES[6][4][3] = {
        // NegX (-X face)
        {{0, 0, 1}, {0, 1, 1}, {0, 1, 0}, {0, 0, 0}},
        // PosX (+X face)
        {{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}},
        // NegY (-Y face, bottom)
        {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}},
        // PosY (+Y face, top)
        {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}},
        // NegZ (-Z face)
        {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}},
        // PosZ (+Z face)
        {{1, 0, 1}, {1, 1, 1}, {0, 1, 1}, {0, 0, 1}},
    };

    // UV coordinates for each vertex (standard texture mapping)
    static constexpr float FACE_UVS[4][2] = {
        {0, 0},  // Bottom-left
        {0, 1},  // Top-left
        {1, 1},  // Top-right
        {1, 0},  // Bottom-right
    };

    // Check if a face should be rendered
    bool should_render_face(const world::Chunk& chunk, const world::Chunk* neighbors[4],
                            const world::LocalBlockPos& pos, FaceDirection face) const {
        const auto& registry = world::BlockRegistry::instance();

        // Get current block
        auto current_entry = chunk.get_entry(pos);
        const auto* current_type = registry.get(current_entry.block_id);
        if (!current_type || current_type->is_air()) {
            return false;
        }

        // Get neighbor position
        glm::ivec3 neighbor_pos = pos + FACE_OFFSETS[static_cast<int>(face)];

        // Check if neighbor is in a different chunk
        world::PaletteEntry neighbor_entry;
        if (neighbor_pos.x < 0) {
            if (neighbors[0]) {
                neighbor_entry =
                    neighbors[0]->get_entry({neighbor_pos.x + world::CHUNK_SIZE_X, neighbor_pos.y, neighbor_pos.z});
            } else {
                return true;  // No neighbor chunk, render face
            }
        } else if (neighbor_pos.x >= world::CHUNK_SIZE_X) {
            if (neighbors[1]) {
                neighbor_entry =
                    neighbors[1]->get_entry({neighbor_pos.x - world::CHUNK_SIZE_X, neighbor_pos.y, neighbor_pos.z});
            } else {
                return true;
            }
        } else if (neighbor_pos.z < 0) {
            if (neighbors[2]) {
                neighbor_entry =
                    neighbors[2]->get_entry({neighbor_pos.x, neighbor_pos.y, neighbor_pos.z + world::CHUNK_SIZE_Z});
            } else {
                return true;
            }
        } else if (neighbor_pos.z >= world::CHUNK_SIZE_Z) {
            if (neighbors[3]) {
                neighbor_entry =
                    neighbors[3]->get_entry({neighbor_pos.x, neighbor_pos.y, neighbor_pos.z - world::CHUNK_SIZE_Z});
            } else {
                return true;
            }
        } else if (neighbor_pos.y < 0 || neighbor_pos.y >= world::CHUNK_SIZE_Y) {
            return true;  // Top/bottom of world
        } else {
            neighbor_entry = chunk.get_entry(neighbor_pos);
        }

        const auto* neighbor_type = registry.get(neighbor_entry.block_id);

        // If neighbor is air, render face
        if (!neighbor_type || neighbor_type->is_air()) {
            return true;
        }

        // If current is opaque and neighbor is opaque, skip face
        if (current_type->is_solid() && !current_type->is_transparent() && neighbor_type->is_solid() &&
            !neighbor_type->is_transparent()) {
            return false;
        }

        // If current is transparent and neighbor is same type, skip face
        if (current_type->is_transparent() && current_entry.block_id == neighbor_entry.block_id) {
            return false;
        }

        // If current is opaque and neighbor is transparent, render face
        if (!current_type->is_transparent() && neighbor_type->is_transparent()) {
            return true;
        }

        // If both transparent but different types, render face
        if (current_type->is_transparent() && neighbor_type->is_transparent()) {
            return true;
        }

        return false;
    }

    // Calculate ambient occlusion for a vertex
    uint8_t calculate_ao(bool side1, bool side2, bool corner) const {
        if (!config.enable_ambient_occlusion) {
            return 255;
        }

        // Classic voxel AO formula
        int ao_value;
        if (side1 && side2) {
            ao_value = 0;
        } else {
            ao_value = 3 - (side1 ? 1 : 0) - (side2 ? 1 : 0) - (corner ? 1 : 0);
        }

        // Convert to 0-255 range
        return static_cast<uint8_t>(255 - (3 - ao_value) * config.ao_strength);
    }

    // Add a single face quad
    void add_face(std::vector<VoxelVertex>& vertices, std::vector<uint32_t>& indices, const world::LocalBlockPos& pos,
                  FaceDirection face, uint16_t texture_index, const uint8_t ao[4]) {
        uint32_t base_index = static_cast<uint32_t>(vertices.size());

        // Get face data
        int face_idx = static_cast<int>(face);
        float nx, ny, nz;
        get_face_normal(face, nx, ny, nz);

        int8_t packed_normal[3];
        VoxelVertex::pack_normal(nx, ny, nz, packed_normal);

        // Add 4 vertices
        for (int i = 0; i < 4; i++) {
            VoxelVertex v{};
            v.position[0] = static_cast<float>(pos.x) + FACE_VERTICES[face_idx][i][0];
            v.position[1] = static_cast<float>(pos.y) + FACE_VERTICES[face_idx][i][1];
            v.position[2] = static_cast<float>(pos.z) + FACE_VERTICES[face_idx][i][2];
            v.position[3] = 1.0f;  // W component for vec4

            v.normal[0] = packed_normal[0];
            v.normal[1] = packed_normal[1];
            v.normal[2] = packed_normal[2];
            v.ao = ao[i];

            v.uv[0] = FACE_UVS[i][0];
            v.uv[1] = FACE_UVS[i][1];

            v.texture_index = texture_index;
            v.light_sky = 15;  // Max light for now
            v.light_block = 0;

            v.color[0] = 255;
            v.color[1] = 255;
            v.color[2] = 255;
            v.color[3] = 255;

            vertices.push_back(v);
        }

        // Add 6 indices (2 triangles)
        // Use different winding based on AO to fix diagonal lighting artifacts
        if (ao[0] + ao[2] > ao[1] + ao[3]) {
            indices.push_back(base_index + 0);
            indices.push_back(base_index + 1);
            indices.push_back(base_index + 2);
            indices.push_back(base_index + 0);
            indices.push_back(base_index + 2);
            indices.push_back(base_index + 3);
        } else {
            indices.push_back(base_index + 1);
            indices.push_back(base_index + 2);
            indices.push_back(base_index + 3);
            indices.push_back(base_index + 1);
            indices.push_back(base_index + 3);
            indices.push_back(base_index + 0);
        }
    }
};

MeshGenerator::MeshGenerator(const MeshGeneratorConfig& config) : impl_(std::make_unique<Impl>()) {
    impl_->config = config;
}

MeshGenerator::~MeshGenerator() = default;

void MeshGenerator::set_config(const MeshGeneratorConfig& config) {
    impl_->config = config;
}

const MeshGeneratorConfig& MeshGenerator::get_config() const {
    return impl_->config;
}

bool MeshGenerator::generate(const world::Chunk& chunk, const world::Chunk* neighbor_neg_x,
                             const world::Chunk* neighbor_pos_x, const world::Chunk* neighbor_neg_z,
                             const world::Chunk* neighbor_pos_z, ChunkMeshData& out_data, ChunkMeshStats& out_stats) {
    auto start_time = std::chrono::high_resolution_clock::now();

    out_data.clear();
    out_stats = ChunkMeshStats{};

    const world::Chunk* neighbors[4] = {neighbor_neg_x, neighbor_pos_x, neighbor_neg_z, neighbor_pos_z};
    const auto& registry = world::BlockRegistry::instance();

    // Iterate all blocks in chunk
    for (int32_t y = 0; y < world::CHUNK_SIZE_Y; y++) {
        for (int32_t z = 0; z < world::CHUNK_SIZE_Z; z++) {
            for (int32_t x = 0; x < world::CHUNK_SIZE_X; x++) {
                world::LocalBlockPos pos{x, y, z};
                auto entry = chunk.get_entry(pos);

                const auto* block_type = registry.get(entry.block_id);
                if (!block_type || block_type->is_air()) {
                    continue;
                }

                bool is_transparent = block_type->is_transparent();
                auto& vertices = is_transparent ? out_data.transparent_vertices : out_data.opaque_vertices;
                auto& indices = is_transparent ? out_data.transparent_indices : out_data.opaque_indices;

                // Check each face
                for (int f = 0; f < 6; f++) {
                    auto face = static_cast<FaceDirection>(f);
                    if (!impl_->should_render_face(chunk, neighbors, pos, face)) {
                        continue;
                    }

                    // Get texture for this face using the BlockType's texture mapping
                    auto world_dir = static_cast<world::Direction>(f);
                    uint16_t texture_index = block_type->get_texture_index(world_dir);

                    // Calculate AO for each vertex (simplified - full AO would sample neighbors)
                    uint8_t ao[4] = {255, 255, 255, 255};

                    impl_->add_face(vertices, indices, pos, face, texture_index, ao);
                }
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    out_stats.opaque_vertex_count = static_cast<uint32_t>(out_data.opaque_vertices.size());
    out_stats.opaque_index_count = static_cast<uint32_t>(out_data.opaque_indices.size());
    out_stats.transparent_vertex_count = static_cast<uint32_t>(out_data.transparent_vertices.size());
    out_stats.transparent_index_count = static_cast<uint32_t>(out_data.transparent_indices.size());
    out_stats.generation_time_ms = static_cast<double>(duration.count()) / 1000.0;

    return !out_data.is_empty();
}

}  // namespace realcraft::rendering
