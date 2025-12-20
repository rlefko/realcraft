// RealCraft Rendering System
// mesh_manager.cpp - Threaded mesh management

#include <realcraft/core/logger.hpp>
#include <realcraft/rendering/mesh_manager.hpp>

namespace realcraft::rendering {

MeshManager::MeshManager() = default;

MeshManager::~MeshManager() {
    shutdown();
}

bool MeshManager::initialize(graphics::GraphicsDevice* device, world::WorldManager* world,
                             const MeshManagerConfig& config) {
    if (device_ != nullptr) {
        REALCRAFT_LOG_WARN(core::log_category::GRAPHICS, "MeshManager already initialized");
        return true;
    }

    device_ = device;
    world_ = world;
    config_ = config;

    // Register as world observer
    world_->add_observer(this);

    // Start worker threads
    shutdown_requested_.store(false);
    for (uint32_t i = 0; i < config_.worker_threads; i++) {
        workers_.emplace_back(&MeshManager::worker_thread, this);
    }

    REALCRAFT_LOG_INFO(core::log_category::GRAPHICS, "MeshManager initialized with {} worker threads",
                       config_.worker_threads);
    return true;
}

void MeshManager::shutdown() {
    if (device_ == nullptr) {
        return;
    }

    // Unregister from world
    if (world_) {
        world_->remove_observer(this);
    }

    // Signal workers to stop
    shutdown_requested_.store(true);
    request_cv_.notify_all();

    // Wait for workers
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();

    // Clear queues
    {
        std::lock_guard lock(request_mutex_);
        while (!request_queue_.empty()) {
            request_queue_.pop();
        }
    }
    {
        std::lock_guard lock(completed_mutex_);
        while (!completed_queue_.empty()) {
            completed_queue_.pop();
        }
    }

    // Clear meshes
    {
        std::lock_guard lock(meshes_mutex_);
        meshes_.clear();
    }

    device_ = nullptr;
    world_ = nullptr;

    REALCRAFT_LOG_INFO(core::log_category::GRAPHICS, "MeshManager shutdown");
}

void MeshManager::update() {
    upload_completed_meshes();
}

ChunkMesh* MeshManager::get_mesh(const world::ChunkPos& pos) {
    std::lock_guard lock(meshes_mutex_);
    auto it = meshes_.find(pos);
    if (it != meshes_.end()) {
        return it->second.get();
    }
    return nullptr;
}

const ChunkMesh* MeshManager::get_mesh(const world::ChunkPos& pos) const {
    std::lock_guard lock(meshes_mutex_);
    auto it = meshes_.find(pos);
    if (it != meshes_.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool MeshManager::has_mesh(const world::ChunkPos& pos) const {
    std::lock_guard lock(meshes_mutex_);
    return meshes_.find(pos) != meshes_.end();
}

void MeshManager::request_mesh(const world::ChunkPos& pos, MeshPriority priority) {
    std::lock_guard lock(request_mutex_);

    // Check queue size limit
    if (request_queue_.size() >= config_.max_pending_requests) {
        return;
    }

    request_queue_.push({pos, priority});
    request_cv_.notify_one();
}

void MeshManager::invalidate_mesh(const world::ChunkPos& pos) {
    {
        std::lock_guard lock(meshes_mutex_);
        meshes_.erase(pos);
    }

    request_mesh(pos, MeshPriority::High);
}

void MeshManager::on_chunk_loaded(const world::ChunkPos& pos, world::Chunk& /*chunk*/) {
    request_mesh(pos, MeshPriority::Normal);
}

void MeshManager::on_chunk_unloading(const world::ChunkPos& pos, const world::Chunk& /*chunk*/) {
    std::lock_guard lock(meshes_mutex_);
    meshes_.erase(pos);
}

void MeshManager::on_block_changed(const world::BlockChangeEvent& event) {
    world::ChunkPos chunk_pos = world::world_to_chunk(event.position);

    // Invalidate this chunk
    invalidate_mesh(chunk_pos);

    // Also invalidate neighbors if block is on border
    world::LocalBlockPos local = world::world_to_local(event.position);

    if (local.x == 0) {
        invalidate_mesh({chunk_pos.x - 1, chunk_pos.y});
    }
    if (local.x == world::CHUNK_SIZE_X - 1) {
        invalidate_mesh({chunk_pos.x + 1, chunk_pos.y});
    }
    if (local.z == 0) {
        invalidate_mesh({chunk_pos.x, chunk_pos.y - 1});
    }
    if (local.z == world::CHUNK_SIZE_Z - 1) {
        invalidate_mesh({chunk_pos.x, chunk_pos.y + 1});
    }
}

void MeshManager::on_origin_shifted(const world::WorldBlockPos& /*old_origin*/,
                                    const world::WorldBlockPos& /*new_origin*/) {
    // Meshes use chunk-local coordinates, so no update needed
}

void MeshManager::for_each_mesh(const std::function<void(const world::ChunkPos&, ChunkMesh&)>& callback) {
    std::lock_guard lock(meshes_mutex_);
    for (auto& [pos, mesh] : meshes_) {
        if (mesh && mesh->is_uploaded()) {
            callback(pos, *mesh);
        }
    }
}

void MeshManager::for_each_mesh(const std::function<void(const world::ChunkPos&, const ChunkMesh&)>& callback) const {
    std::lock_guard lock(meshes_mutex_);
    for (const auto& [pos, mesh] : meshes_) {
        if (mesh && mesh->is_uploaded()) {
            callback(pos, *mesh);
        }
    }
}

size_t MeshManager::mesh_count() const {
    std::lock_guard lock(meshes_mutex_);
    return meshes_.size();
}

size_t MeshManager::pending_count() const {
    std::lock_guard lock(request_mutex_);
    return request_queue_.size();
}

size_t MeshManager::total_vertices() const {
    std::lock_guard lock(meshes_mutex_);
    size_t count = 0;
    for (const auto& [pos, mesh] : meshes_) {
        if (mesh) {
            const auto& stats = mesh->get_stats();
            count += stats.opaque_vertex_count + stats.transparent_vertex_count;
        }
    }
    return count;
}

void MeshManager::worker_thread() {
    MeshGenerator generator(config_.generator);

    while (!shutdown_requested_.load()) {
        MeshRequest request;

        // Wait for request
        {
            std::unique_lock lock(request_mutex_);
            request_cv_.wait(lock, [this] { return shutdown_requested_.load() || !request_queue_.empty(); });

            if (shutdown_requested_.load()) {
                break;
            }

            if (request_queue_.empty()) {
                continue;
            }

            request = request_queue_.top();
            request_queue_.pop();
        }

        // Get chunk (may need to wait for lock)
        const world::Chunk* chunk = world_->get_chunk(request.pos);
        if (!chunk || !chunk->is_ready()) {
            continue;
        }

        // Get neighbors for face culling
        const world::Chunk* neg_x = world_->get_chunk({request.pos.x - 1, request.pos.y});
        const world::Chunk* pos_x = world_->get_chunk({request.pos.x + 1, request.pos.y});
        const world::Chunk* neg_z = world_->get_chunk({request.pos.x, request.pos.y - 1});
        const world::Chunk* pos_z = world_->get_chunk({request.pos.x, request.pos.y + 1});

        // Generate mesh
        ChunkMeshData data;
        ChunkMeshStats stats;

        bool has_mesh = generator.generate(*chunk, neg_x, pos_x, neg_z, pos_z, data, stats);

        if (has_mesh) {
            std::lock_guard lock(completed_mutex_);
            completed_queue_.push({request.pos, std::move(data), stats});
        }
    }
}

void MeshManager::upload_completed_meshes() {
    uint32_t uploads = 0;

    while (uploads < config_.uploads_per_frame) {
        CompletedMesh completed;

        {
            std::lock_guard lock(completed_mutex_);
            if (completed_queue_.empty()) {
                break;
            }
            completed = std::move(completed_queue_.front());
            completed_queue_.pop();
        }

        auto mesh = std::make_unique<ChunkMesh>();
        mesh->set_stats(completed.stats);

        if (mesh->upload(device_, completed.data)) {
            std::lock_guard lock(meshes_mutex_);
            meshes_[completed.pos] = std::move(mesh);
            uploads++;
        }
    }
}

}  // namespace realcraft::rendering
