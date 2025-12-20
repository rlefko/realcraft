// RealCraft Rendering System
// mesh_manager.hpp - Threaded mesh generation and caching

#pragma once

#include "chunk_mesh.hpp"
#include "mesh_generator.hpp"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <realcraft/world/world_manager.hpp>
#include <thread>
#include <unordered_map>

namespace realcraft::rendering {

// Priority for mesh generation requests
enum class MeshPriority : uint8_t {
    Low = 0,       // Far from player
    Normal = 1,    // Visible chunks
    High = 2,      // Near player
    Immediate = 3  // Player modified block
};

// Configuration for mesh manager
struct MeshManagerConfig {
    uint32_t worker_threads = 2;
    uint32_t max_pending_requests = 256;
    uint32_t uploads_per_frame = 4;
    MeshGeneratorConfig generator;
};

// Manages mesh generation and caching for chunks
// Implements IWorldObserver for automatic updates
class MeshManager : public world::IWorldObserver {
public:
    MeshManager();
    ~MeshManager() override;

    // Non-copyable
    MeshManager(const MeshManager&) = delete;
    MeshManager& operator=(const MeshManager&) = delete;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    bool initialize(graphics::GraphicsDevice* device, world::WorldManager* world, const MeshManagerConfig& config = {});
    void shutdown();
    [[nodiscard]] bool is_initialized() const { return device_ != nullptr; }

    // ========================================================================
    // Update (call from main thread each frame)
    // ========================================================================

    void update();

    // ========================================================================
    // Mesh Access
    // ========================================================================

    [[nodiscard]] ChunkMesh* get_mesh(const world::ChunkPos& pos);
    [[nodiscard]] const ChunkMesh* get_mesh(const world::ChunkPos& pos) const;
    [[nodiscard]] bool has_mesh(const world::ChunkPos& pos) const;

    // ========================================================================
    // Mesh Requests
    // ========================================================================

    void request_mesh(const world::ChunkPos& pos, MeshPriority priority = MeshPriority::Normal);
    void invalidate_mesh(const world::ChunkPos& pos);

    // ========================================================================
    // IWorldObserver Implementation
    // ========================================================================

    void on_chunk_loaded(const world::ChunkPos& pos, world::Chunk& chunk) override;
    void on_chunk_unloading(const world::ChunkPos& pos, const world::Chunk& chunk) override;
    void on_block_changed(const world::BlockChangeEvent& event) override;
    void on_origin_shifted(const world::WorldBlockPos& old_origin, const world::WorldBlockPos& new_origin) override;

    // ========================================================================
    // Iteration
    // ========================================================================

    void for_each_mesh(const std::function<void(const world::ChunkPos&, ChunkMesh&)>& callback);
    void for_each_mesh(const std::function<void(const world::ChunkPos&, const ChunkMesh&)>& callback) const;

    // ========================================================================
    // Statistics
    // ========================================================================

    [[nodiscard]] size_t mesh_count() const;
    [[nodiscard]] size_t pending_count() const;
    [[nodiscard]] size_t total_vertices() const;

private:
    graphics::GraphicsDevice* device_ = nullptr;
    world::WorldManager* world_ = nullptr;
    MeshManagerConfig config_;

    // Worker threads
    std::vector<std::thread> workers_;
    std::atomic<bool> shutdown_requested_{false};

    // Request queue
    struct MeshRequest {
        world::ChunkPos pos;
        MeshPriority priority;
    };
    struct RequestCompare {
        bool operator()(const MeshRequest& a, const MeshRequest& b) const {
            return static_cast<uint8_t>(a.priority) < static_cast<uint8_t>(b.priority);
        }
    };
    std::priority_queue<MeshRequest, std::vector<MeshRequest>, RequestCompare> request_queue_;
    mutable std::mutex request_mutex_;
    std::condition_variable request_cv_;

    // Completed meshes waiting for GPU upload
    struct CompletedMesh {
        world::ChunkPos pos;
        ChunkMeshData data;
        ChunkMeshStats stats;
    };
    std::queue<CompletedMesh> completed_queue_;
    std::mutex completed_mutex_;

    // Active meshes
    std::unordered_map<world::ChunkPos, std::unique_ptr<ChunkMesh>> meshes_;
    mutable std::mutex meshes_mutex_;

    // Worker thread function
    void worker_thread();

    // Process GPU uploads
    void upload_completed_meshes();
};

}  // namespace realcraft::rendering
