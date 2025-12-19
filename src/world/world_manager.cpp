// RealCraft World System
// world_manager.cpp - World lifecycle management implementation

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <random>
#include <realcraft/core/logger.hpp>
#include <realcraft/platform/file_io.hpp>
#include <realcraft/world/block.hpp>
#include <realcraft/world/world_manager.hpp>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace realcraft::world {

// ============================================================================
// WorldManager Implementation
// ============================================================================

struct WorldManager::Impl {
    WorldConfig config;
    bool initialized = false;

    // Chunk storage
    std::unordered_map<ChunkPos, std::unique_ptr<Chunk>> chunks;
    mutable std::shared_mutex chunks_mutex;

    // Origin shifting
    OriginShifter origin_shifter;

    // Serialization
    std::unique_ptr<ChunkSerializer> serializer;
    std::unique_ptr<RegionManager> region_manager;

    // Player tracking
    WorldPos player_position{0, 64, 0};
    std::mutex player_mutex;

    // Observers
    std::vector<IWorldObserver*> observers;
    std::mutex observers_mutex;

    // Async loading
    struct LoadRequest {
        ChunkPos pos;
        ChunkLoadPriority priority;
    };

    struct LoadRequestCompare {
        bool operator()(const LoadRequest& a, const LoadRequest& b) const {
            return static_cast<uint8_t>(a.priority) < static_cast<uint8_t>(b.priority);
        }
    };

    std::priority_queue<LoadRequest, std::vector<LoadRequest>, LoadRequestCompare> load_queue;
    std::unordered_set<ChunkPos> queued_chunks;
    std::mutex load_queue_mutex;
    std::condition_variable load_cv;

    // Worker threads
    std::vector<std::thread> workers;
    std::atomic<bool> should_stop{false};

    // Auto-save
    double auto_save_timer = 0.0;

    // Statistics
    std::atomic<size_t> pending_generation_count{0};
    std::atomic<size_t> pending_load_count{0};

    void worker_thread() {
        while (!should_stop) {
            LoadRequest request;
            {
                std::unique_lock<std::mutex> lock(load_queue_mutex);
                load_cv.wait(lock, [this] { return should_stop || !load_queue.empty(); });

                if (should_stop) {
                    break;
                }

                if (load_queue.empty()) {
                    continue;
                }

                request = load_queue.top();
                load_queue.pop();
                queued_chunks.erase(request.pos);
            }

            process_load_request(request);
        }
    }

    void process_load_request(const LoadRequest& request) {
        // Check if already loaded
        {
            std::shared_lock<std::shared_mutex> lock(chunks_mutex);
            auto it = chunks.find(request.pos);
            if (it != chunks.end() && it->second->is_ready()) {
                return;
            }
        }

        // Try loading from disk first
        bool loaded_from_disk = false;
        std::vector<uint8_t> chunk_data;

        if (region_manager) {
            pending_load_count++;
            auto data = region_manager->read_chunk(request.pos);
            pending_load_count--;

            if (data.has_value()) {
                chunk_data = std::move(*data);
                loaded_from_disk = true;
            }
        }

        // Create or update chunk
        std::unique_ptr<Chunk> new_chunk;
        {
            std::unique_lock<std::shared_mutex> lock(chunks_mutex);
            auto it = chunks.find(request.pos);

            if (it != chunks.end()) {
                // Chunk already exists, just update it
                if (loaded_from_disk) {
                    it->second->set_state(ChunkState::Loading);
                    serializer->deserialize(*it->second, chunk_data);
                    it->second->set_state(ChunkState::Loaded);
                } else {
                    it->second->set_state(ChunkState::Generating);
                    generate_chunk(*it->second);
                    it->second->set_state(ChunkState::Loaded);
                }
                notify_chunk_loaded(request.pos, *it->second);
                return;
            }

            // Create new chunk
            ChunkDesc desc;
            desc.position = request.pos;
            desc.seed = config.seed;
            desc.generate_terrain = !loaded_from_disk;

            new_chunk = std::make_unique<Chunk>(desc);

            if (loaded_from_disk) {
                new_chunk->set_state(ChunkState::Loading);
            } else {
                new_chunk->set_state(ChunkState::Generating);
                pending_generation_count++;
            }
        }

        // Process outside lock
        if (loaded_from_disk) {
            serializer->deserialize(*new_chunk, chunk_data);
            new_chunk->set_state(ChunkState::Loaded);
        } else {
            generate_chunk(*new_chunk);
            new_chunk->set_state(ChunkState::Loaded);
            pending_generation_count--;
        }

        // Insert into map
        Chunk* chunk_ptr = new_chunk.get();
        {
            std::unique_lock<std::shared_mutex> lock(chunks_mutex);
            chunks[request.pos] = std::move(new_chunk);
            update_neighbors(request.pos, chunk_ptr);
        }

        notify_chunk_loaded(request.pos, *chunk_ptr);
    }

    void generate_chunk(Chunk& chunk) {
        // Simple placeholder generation - fills with stone at bottom, air above
        auto lock = chunk.write_lock();

        // Get block IDs from registry
        const auto& registry = BlockRegistry::instance();
        BlockId stone_id = registry.stone_id();
        BlockId dirt_id = registry.dirt_id();
        BlockId grass_id = registry.grass_id();

        // Simple flat terrain for now
        int base_height = 64;

        for (int x = 0; x < CHUNK_SIZE_X; ++x) {
            for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
                // Simple height variation using position
                WorldBlockPos world_pos = local_to_world(chunk.get_position(), LocalBlockPos(x, 0, z));
                int height = base_height + static_cast<int>((std::sin(static_cast<double>(world_pos.x) * 0.05) +
                                                             std::sin(static_cast<double>(world_pos.z) * 0.07)) *
                                                            4.0);

                for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
                    BlockId block_id = BLOCK_AIR;

                    if (y < height - 4) {
                        block_id = stone_id;
                    } else if (y < height - 1) {
                        block_id = dirt_id;
                    } else if (y < height) {
                        block_id = grass_id;
                    }

                    if (block_id != BLOCK_AIR) {
                        lock.set_block(LocalBlockPos(x, y, z), block_id);
                    }
                }
            }
        }

        chunk.get_metadata_mut().has_been_generated = true;
    }

    void update_neighbors(const ChunkPos& pos, Chunk* chunk) {
        // Link to existing neighbors
        for (int i = 0; i < static_cast<int>(HorizontalDirection::Count); ++i) {
            auto dir = static_cast<HorizontalDirection>(i);
            glm::ivec2 offset = direction_offset(dir);
            ChunkPos neighbor_pos(pos.x + offset.x, pos.y + offset.y);

            auto it = chunks.find(neighbor_pos);
            if (it != chunks.end()) {
                chunk->set_neighbor(dir, it->second.get());
                it->second->set_neighbor(opposite(dir), chunk);
            }
        }
    }

    void notify_chunk_loaded(const ChunkPos& pos, Chunk& chunk) {
        std::lock_guard<std::mutex> lock(observers_mutex);
        for (auto* observer : observers) {
            observer->on_chunk_loaded(pos, chunk);
        }
    }

    void notify_chunk_unloading(const ChunkPos& pos, const Chunk& chunk) {
        std::lock_guard<std::mutex> lock(observers_mutex);
        for (auto* observer : observers) {
            observer->on_chunk_unloading(pos, chunk);
        }
    }

    void notify_block_changed(const BlockChangeEvent& event) {
        std::lock_guard<std::mutex> lock(observers_mutex);
        for (auto* observer : observers) {
            observer->on_block_changed(event);
        }
    }

    void notify_origin_shifted(const WorldBlockPos& old_origin, const WorldBlockPos& new_origin) {
        std::lock_guard<std::mutex> lock(observers_mutex);
        for (auto* observer : observers) {
            observer->on_origin_shifted(old_origin, new_origin);
        }
    }
};

WorldManager::WorldManager() : impl_(std::make_unique<Impl>()) {}

WorldManager::~WorldManager() {
    shutdown();
}

bool WorldManager::initialize(const WorldConfig& config) {
    if (impl_->initialized) {
        REALCRAFT_LOG_WARN(core::log_category::WORLD, "WorldManager already initialized");
        return true;
    }

    impl_->config = config;

    // Generate random seed if not specified
    if (impl_->config.seed == 0) {
        std::random_device rd;
        impl_->config.seed = rd();
    }

    REALCRAFT_LOG_INFO(core::log_category::WORLD, "Initializing world '{}' with seed {}", config.name,
                       impl_->config.seed);

    // Register default blocks if not already done
    BlockRegistry::instance().register_defaults();

    // Configure origin shifting
    impl_->origin_shifter.configure(config.origin_shift);
    impl_->origin_shifter.set_shift_callback([this](const WorldBlockPos& old_origin, const WorldBlockPos& new_origin) {
        impl_->notify_origin_shifted(old_origin, new_origin);
    });

    // Initialize serialization
    impl_->serializer = std::make_unique<ChunkSerializer>();

    if (config.enable_saving) {
        std::filesystem::path save_dir = config.save_directory;
        if (save_dir.empty()) {
            save_dir = platform::FileSystem::get_user_saves_directory() / config.name;
        }
        impl_->region_manager = std::make_unique<RegionManager>(save_dir);
        REALCRAFT_LOG_INFO(core::log_category::WORLD, "World save directory: {}", save_dir.string());
    }

    // Start worker threads
    impl_->should_stop = false;
    int num_workers = std::max(1, config.generation_threads);
    for (int i = 0; i < num_workers; ++i) {
        impl_->workers.emplace_back([this] { impl_->worker_thread(); });
    }

    impl_->initialized = true;
    REALCRAFT_LOG_INFO(core::log_category::WORLD, "WorldManager initialized with {} worker threads", num_workers);

    return true;
}

void WorldManager::shutdown() {
    if (!impl_->initialized) {
        return;
    }

    REALCRAFT_LOG_INFO(core::log_category::WORLD, "Shutting down WorldManager");

    // Stop workers
    impl_->should_stop = true;
    impl_->load_cv.notify_all();

    for (auto& worker : impl_->workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    impl_->workers.clear();

    // Save dirty chunks
    save_dirty_chunks();

    // Close region files
    if (impl_->region_manager) {
        impl_->region_manager->close_all();
    }

    // Clear chunks
    {
        std::unique_lock<std::shared_mutex> lock(impl_->chunks_mutex);
        impl_->chunks.clear();
    }

    impl_->initialized = false;
}

bool WorldManager::is_initialized() const {
    return impl_->initialized;
}

void WorldManager::update(double delta_time) {
    if (!impl_->initialized) {
        return;
    }

    // Update origin shifting
    {
        std::lock_guard<std::mutex> lock(impl_->player_mutex);
        impl_->origin_shifter.update(impl_->player_position);
    }

    // Auto-save
    if (impl_->config.auto_save_interval > 0) {
        impl_->auto_save_timer += delta_time;
        if (impl_->auto_save_timer >= impl_->config.auto_save_interval) {
            impl_->auto_save_timer = 0.0;
            save_dirty_chunks();
        }
    }
}

Chunk* WorldManager::get_chunk(const ChunkPos& pos) {
    std::shared_lock<std::shared_mutex> lock(impl_->chunks_mutex);
    auto it = impl_->chunks.find(pos);
    if (it != impl_->chunks.end()) {
        return it->second.get();
    }
    return nullptr;
}

const Chunk* WorldManager::get_chunk(const ChunkPos& pos) const {
    std::shared_lock<std::shared_mutex> lock(impl_->chunks_mutex);
    auto it = impl_->chunks.find(pos);
    if (it != impl_->chunks.end()) {
        return it->second.get();
    }
    return nullptr;
}

void WorldManager::request_chunk(const ChunkPos& pos, ChunkLoadPriority priority) {
    // Check if already loaded or queued
    {
        std::shared_lock<std::shared_mutex> lock(impl_->chunks_mutex);
        auto it = impl_->chunks.find(pos);
        if (it != impl_->chunks.end() && it->second->is_ready()) {
            return;
        }
    }

    {
        std::lock_guard<std::mutex> lock(impl_->load_queue_mutex);
        if (impl_->queued_chunks.count(pos) > 0) {
            return;
        }
        impl_->queued_chunks.insert(pos);
        impl_->load_queue.push({pos, priority});
    }

    impl_->load_cv.notify_one();
}

Chunk* WorldManager::load_chunk_sync(const ChunkPos& pos) {
    // Check if already loaded
    {
        std::shared_lock<std::shared_mutex> lock(impl_->chunks_mutex);
        auto it = impl_->chunks.find(pos);
        if (it != impl_->chunks.end() && it->second->is_ready()) {
            return it->second.get();
        }
    }

    // Process synchronously
    Impl::LoadRequest request{pos, ChunkLoadPriority::Immediate};
    impl_->process_load_request(request);

    return get_chunk(pos);
}

void WorldManager::unload_chunk(const ChunkPos& pos) {
    std::unique_ptr<Chunk> chunk_to_unload;

    {
        std::unique_lock<std::shared_mutex> lock(impl_->chunks_mutex);
        auto it = impl_->chunks.find(pos);
        if (it == impl_->chunks.end()) {
            return;
        }

        impl_->notify_chunk_unloading(pos, *it->second);

        // Save if dirty
        if (it->second->is_dirty() && impl_->region_manager) {
            std::vector<uint8_t> data = impl_->serializer->serialize(*it->second);
            impl_->region_manager->write_chunk(pos, data);
        }

        // Remove neighbor links
        for (int i = 0; i < static_cast<int>(HorizontalDirection::Count); ++i) {
            auto dir = static_cast<HorizontalDirection>(i);
            Chunk* neighbor = it->second->get_neighbor(dir);
            if (neighbor) {
                neighbor->set_neighbor(opposite(dir), nullptr);
            }
        }

        chunk_to_unload = std::move(it->second);
        impl_->chunks.erase(it);
    }

    // Chunk destructor called outside lock
}

bool WorldManager::chunk_exists_on_disk(const ChunkPos& pos) const {
    if (!impl_->region_manager) {
        return false;
    }
    return impl_->region_manager->has_chunk(pos);
}

BlockId WorldManager::get_block(const WorldBlockPos& pos) const {
    return get_entry(pos).block_id;
}

BlockStateId WorldManager::get_block_state(const WorldBlockPos& pos) const {
    return get_entry(pos).state_id;
}

PaletteEntry WorldManager::get_entry(const WorldBlockPos& pos) const {
    ChunkPos chunk_pos = world_to_chunk(pos);
    LocalBlockPos local_pos = world_to_local(pos);

    const Chunk* chunk = get_chunk(chunk_pos);
    if (!chunk) {
        return PaletteEntry::air();
    }

    return chunk->get_entry(local_pos);
}

void WorldManager::set_block(const WorldBlockPos& pos, BlockId id, BlockStateId state) {
    ChunkPos chunk_pos = world_to_chunk(pos);
    LocalBlockPos local_pos = world_to_local(pos);

    Chunk* chunk = get_chunk(chunk_pos);
    if (!chunk) {
        return;
    }

    PaletteEntry old_entry = chunk->get_entry(local_pos);
    chunk->set_block(local_pos, id, state);

    BlockChangeEvent event;
    event.position = pos;
    event.old_entry = old_entry;
    event.new_entry = PaletteEntry{id, state};
    event.from_generation = false;

    impl_->notify_block_changed(event);
}

void WorldManager::set_player_position(const WorldPos& position) {
    std::lock_guard<std::mutex> lock(impl_->player_mutex);
    impl_->player_position = position;
}

WorldPos WorldManager::get_player_position() const {
    std::lock_guard<std::mutex> lock(impl_->player_mutex);
    return impl_->player_position;
}

void WorldManager::update_loaded_chunks() {
    WorldPos player_pos;
    {
        std::lock_guard<std::mutex> lock(impl_->player_mutex);
        player_pos = impl_->player_position;
    }

    WorldBlockPos player_block = world_to_block(player_pos);
    ChunkPos player_chunk = world_to_chunk(player_block);

    int view_dist = impl_->config.view_distance;

    // Request chunks within view distance
    for (int x = -view_dist; x <= view_dist; ++x) {
        for (int z = -view_dist; z <= view_dist; ++z) {
            ChunkPos pos(player_chunk.x + x, player_chunk.y + z);

            // Prioritize based on distance
            int dist = std::max(std::abs(x), std::abs(z));
            ChunkLoadPriority priority = dist <= 2 ? ChunkLoadPriority::High : ChunkLoadPriority::Normal;

            request_chunk(pos, priority);
        }
    }

    // Unload chunks outside view distance + buffer
    int unload_dist = view_dist + 2;
    std::vector<ChunkPos> to_unload;

    {
        std::shared_lock<std::shared_mutex> lock(impl_->chunks_mutex);
        for (const auto& [pos, chunk] : impl_->chunks) {
            int dx = std::abs(pos.x - player_chunk.x);
            int dz = std::abs(pos.y - player_chunk.y);

            if (dx > unload_dist || dz > unload_dist) {
                to_unload.push_back(pos);
            }
        }
    }

    for (const auto& pos : to_unload) {
        unload_chunk(pos);
    }
}

WorldBlockPos WorldManager::get_origin_offset() const {
    return impl_->origin_shifter.get_origin_offset();
}

OriginShifter& WorldManager::get_origin_shifter() {
    return impl_->origin_shifter;
}

const OriginShifter& WorldManager::get_origin_shifter() const {
    return impl_->origin_shifter;
}

glm::dvec3 WorldManager::world_to_relative(const WorldPos& world) const {
    return impl_->origin_shifter.world_to_relative(world);
}

WorldPos WorldManager::relative_to_world(const glm::dvec3& relative) const {
    return impl_->origin_shifter.relative_to_world(relative);
}

glm::vec3 WorldManager::world_to_render(const WorldPos& world) const {
    return impl_->origin_shifter.world_to_render(world);
}

void WorldManager::add_observer(IWorldObserver* observer) {
    std::lock_guard<std::mutex> lock(impl_->observers_mutex);
    impl_->observers.push_back(observer);
}

void WorldManager::remove_observer(IWorldObserver* observer) {
    std::lock_guard<std::mutex> lock(impl_->observers_mutex);
    impl_->observers.erase(std::remove(impl_->observers.begin(), impl_->observers.end(), observer),
                           impl_->observers.end());
}

void WorldManager::save_all() {
    save_dirty_chunks();
    if (impl_->region_manager) {
        impl_->region_manager->flush();
    }
}

void WorldManager::save_chunk(const ChunkPos& pos) {
    if (!impl_->region_manager) {
        return;
    }

    std::shared_lock<std::shared_mutex> lock(impl_->chunks_mutex);
    auto it = impl_->chunks.find(pos);
    if (it == impl_->chunks.end()) {
        return;
    }

    std::vector<uint8_t> data = impl_->serializer->serialize(*it->second);
    impl_->region_manager->write_chunk(pos, data);
    it->second->mark_clean();
}

void WorldManager::save_dirty_chunks() {
    if (!impl_->region_manager) {
        return;
    }

    std::vector<ChunkPos> dirty_positions;

    {
        std::shared_lock<std::shared_mutex> lock(impl_->chunks_mutex);
        for (const auto& [pos, chunk] : impl_->chunks) {
            if (chunk->is_dirty()) {
                dirty_positions.push_back(pos);
            }
        }
    }

    for (const auto& pos : dirty_positions) {
        save_chunk(pos);
    }

    if (!dirty_positions.empty()) {
        REALCRAFT_LOG_DEBUG(core::log_category::WORLD, "Saved {} dirty chunks", dirty_positions.size());
    }
}

void WorldManager::for_each_chunk(const std::function<void(const ChunkPos&, Chunk&)>& callback) {
    std::shared_lock<std::shared_mutex> lock(impl_->chunks_mutex);
    for (auto& [pos, chunk] : impl_->chunks) {
        callback(pos, *chunk);
    }
}

void WorldManager::for_each_chunk(const std::function<void(const ChunkPos&, const Chunk&)>& callback) const {
    std::shared_lock<std::shared_mutex> lock(impl_->chunks_mutex);
    for (const auto& [pos, chunk] : impl_->chunks) {
        callback(pos, *chunk);
    }
}

std::vector<ChunkPos> WorldManager::get_loaded_chunk_positions() const {
    std::shared_lock<std::shared_mutex> lock(impl_->chunks_mutex);
    std::vector<ChunkPos> positions;
    positions.reserve(impl_->chunks.size());
    for (const auto& [pos, chunk] : impl_->chunks) {
        positions.push_back(pos);
    }
    return positions;
}

size_t WorldManager::loaded_chunk_count() const {
    std::shared_lock<std::shared_mutex> lock(impl_->chunks_mutex);
    return impl_->chunks.size();
}

size_t WorldManager::pending_generation_count() const {
    return impl_->pending_generation_count.load();
}

size_t WorldManager::pending_load_count() const {
    return impl_->pending_load_count.load();
}

size_t WorldManager::dirty_chunk_count() const {
    std::shared_lock<std::shared_mutex> lock(impl_->chunks_mutex);
    size_t count = 0;
    for (const auto& [pos, chunk] : impl_->chunks) {
        if (chunk->is_dirty()) {
            ++count;
        }
    }
    return count;
}

size_t WorldManager::total_memory_usage() const {
    std::shared_lock<std::shared_mutex> lock(impl_->chunks_mutex);
    size_t total = sizeof(Impl);
    for (const auto& [pos, chunk] : impl_->chunks) {
        total += chunk->memory_usage();
    }
    return total;
}

const WorldConfig& WorldManager::get_config() const {
    return impl_->config;
}

void WorldManager::set_view_distance(int32_t distance) {
    impl_->config.view_distance = distance;
}

uint32_t WorldManager::get_seed() const {
    return impl_->config.seed;
}

}  // namespace realcraft::world
