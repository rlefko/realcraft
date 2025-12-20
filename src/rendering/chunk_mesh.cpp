// RealCraft Rendering System
// chunk_mesh.cpp - GPU mesh implementation

#include <realcraft/core/logger.hpp>
#include <realcraft/rendering/chunk_mesh.hpp>

namespace realcraft::rendering {

bool ChunkMesh::upload(graphics::GraphicsDevice* device, const ChunkMeshData& data) {
    if (!device) {
        return false;
    }

    // Release any existing buffers
    release();

    // Upload opaque geometry
    if (!data.opaque_vertices.empty() && !data.opaque_indices.empty()) {
        graphics::BufferDesc vertex_desc;
        vertex_desc.size = data.opaque_vertices.size() * sizeof(VoxelVertex);
        vertex_desc.usage = graphics::BufferUsage::Vertex;
        vertex_desc.host_visible = true;
        vertex_desc.initial_data = data.opaque_vertices.data();
        vertex_desc.debug_name = "ChunkMesh_Opaque_VB";

        opaque_vertex_buffer_ = device->create_buffer(vertex_desc);
        if (!opaque_vertex_buffer_) {
            REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create opaque vertex buffer");
            return false;
        }

        graphics::BufferDesc index_desc;
        index_desc.size = data.opaque_indices.size() * sizeof(uint32_t);
        index_desc.usage = graphics::BufferUsage::Index;
        index_desc.host_visible = true;
        index_desc.initial_data = data.opaque_indices.data();
        index_desc.debug_name = "ChunkMesh_Opaque_IB";

        opaque_index_buffer_ = device->create_buffer(index_desc);
        if (!opaque_index_buffer_) {
            REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create opaque index buffer");
            release();
            return false;
        }

        opaque_index_count_ = static_cast<uint32_t>(data.opaque_indices.size());
    }

    // Upload transparent geometry
    if (!data.transparent_vertices.empty() && !data.transparent_indices.empty()) {
        graphics::BufferDesc vertex_desc;
        vertex_desc.size = data.transparent_vertices.size() * sizeof(VoxelVertex);
        vertex_desc.usage = graphics::BufferUsage::Vertex;
        vertex_desc.host_visible = true;
        vertex_desc.initial_data = data.transparent_vertices.data();
        vertex_desc.debug_name = "ChunkMesh_Transparent_VB";

        transparent_vertex_buffer_ = device->create_buffer(vertex_desc);
        if (!transparent_vertex_buffer_) {
            REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create transparent vertex buffer");
            release();
            return false;
        }

        graphics::BufferDesc index_desc;
        index_desc.size = data.transparent_indices.size() * sizeof(uint32_t);
        index_desc.usage = graphics::BufferUsage::Index;
        index_desc.host_visible = true;
        index_desc.initial_data = data.transparent_indices.data();
        index_desc.debug_name = "ChunkMesh_Transparent_IB";

        transparent_index_buffer_ = device->create_buffer(index_desc);
        if (!transparent_index_buffer_) {
            REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create transparent index buffer");
            release();
            return false;
        }

        transparent_index_count_ = static_cast<uint32_t>(data.transparent_indices.size());
    }

    return true;
}

void ChunkMesh::release() {
    opaque_vertex_buffer_.reset();
    opaque_index_buffer_.reset();
    transparent_vertex_buffer_.reset();
    transparent_index_buffer_.reset();
    opaque_index_count_ = 0;
    transparent_index_count_ = 0;
}

}  // namespace realcraft::rendering
