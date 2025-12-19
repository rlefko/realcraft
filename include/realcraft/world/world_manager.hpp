// RealCraft World System
// world_manager.hpp - World lifecycle management and chunk orchestration

#pragma once

#include "chunk.hpp"
#include "origin_shifter.hpp"
#include "serialization.hpp"
#include "types.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace realcraft::world {

// ============================================================================
// World Configuration
// ============================================================================

struct WorldConfig {
    std::string name = "world";
    uint32_t seed = 0;                     // 0 = random seed
    int32_t view_distance = 8;             // Chunks to load around player
    int32_t generation_threads = 2;        // Background generation workers
    bool enable_saving = true;             // Persist to disk
    std::filesystem::path save_directory;  // Empty = default saves dir

    // Memory limits
    size_t max_loaded_chunks = 1024;  // Force unload if exceeded
    size_t chunk_cache_size = 256;    // Keep recently used in memory

    // Origin shifting
    OriginShiftConfig origin_shift;

    // Auto-save interval (seconds, 0 = disabled)
    double auto_save_interval = 300.0;
};

// ============================================================================
// Block Change Event
// ============================================================================

struct BlockChangeEvent {
    WorldBlockPos position;
    PaletteEntry old_entry;
    PaletteEntry new_entry;
    bool from_generation = false;  // True if from world gen, not player
};

// ============================================================================
// World Observer Interface
// ============================================================================

class IWorldObserver {
public:
    virtual ~IWorldObserver() = default;

    virtual void on_chunk_loaded(const ChunkPos& pos, Chunk& chunk) {
        (void)pos;
        (void)chunk;
    }
    virtual void on_chunk_unloading(const ChunkPos& pos, const Chunk& chunk) {
        (void)pos;
        (void)chunk;
    }
    virtual void on_block_changed(const BlockChangeEvent& event) { (void)event; }
    virtual void on_origin_shifted(const WorldBlockPos& old_origin, const WorldBlockPos& new_origin) {
        (void)old_origin;
        (void)new_origin;
    }
};

// ============================================================================
// Chunk Load Priority
// ============================================================================

enum class ChunkLoadPriority : uint8_t {
    Low = 0,       // Background loading
    Normal = 1,    // Standard player-driven loading
    High = 2,      // Near player, needed soon
    Immediate = 3  // Blocking load required
};

// ============================================================================
// World Manager
// ============================================================================

class WorldManager {
public:
    WorldManager();
    ~WorldManager();

    // Non-copyable, non-movable
    WorldManager(const WorldManager&) = delete;
    WorldManager& operator=(const WorldManager&) = delete;
    WorldManager(WorldManager&&) = delete;
    WorldManager& operator=(WorldManager&&) = delete;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    bool initialize(const WorldConfig& config);
    void shutdown();
    [[nodiscard]] bool is_initialized() const;

    // Called each frame to process async operations
    void update(double delta_time);

    // ========================================================================
    // Chunk Access
    // ========================================================================

    // Get chunk (nullptr if not loaded)
    [[nodiscard]] Chunk* get_chunk(const ChunkPos& pos);
    [[nodiscard]] const Chunk* get_chunk(const ChunkPos& pos) const;

    // Request chunk load (async, returns immediately)
    void request_chunk(const ChunkPos& pos, ChunkLoadPriority priority = ChunkLoadPriority::Normal);

    // Force immediate load (blocks until loaded)
    [[nodiscard]] Chunk* load_chunk_sync(const ChunkPos& pos);

    // Unload chunk (saves if dirty)
    void unload_chunk(const ChunkPos& pos);

    // Check if chunk exists on disk
    [[nodiscard]] bool chunk_exists_on_disk(const ChunkPos& pos) const;

    // ========================================================================
    // Block Access (convenience wrappers)
    // ========================================================================

    [[nodiscard]] BlockId get_block(const WorldBlockPos& pos) const;
    [[nodiscard]] BlockStateId get_block_state(const WorldBlockPos& pos) const;
    [[nodiscard]] PaletteEntry get_entry(const WorldBlockPos& pos) const;
    void set_block(const WorldBlockPos& pos, BlockId id, BlockStateId state = 0);

    // ========================================================================
    // Player Position (for chunk loading)
    // ========================================================================

    void set_player_position(const WorldPos& position);
    [[nodiscard]] WorldPos get_player_position() const;

    // Update loaded chunks based on player position
    void update_loaded_chunks();

    // ========================================================================
    // Origin Shifting
    // ========================================================================

    [[nodiscard]] WorldBlockPos get_origin_offset() const;
    [[nodiscard]] OriginShifter& get_origin_shifter();
    [[nodiscard]] const OriginShifter& get_origin_shifter() const;

    // Convert between world and relative coordinates
    [[nodiscard]] glm::dvec3 world_to_relative(const WorldPos& world) const;
    [[nodiscard]] WorldPos relative_to_world(const glm::dvec3& relative) const;
    [[nodiscard]] glm::vec3 world_to_render(const WorldPos& world) const;

    // ========================================================================
    // Observers
    // ========================================================================

    void add_observer(IWorldObserver* observer);
    void remove_observer(IWorldObserver* observer);

    // ========================================================================
    // Persistence
    // ========================================================================

    void save_all();
    void save_chunk(const ChunkPos& pos);
    void save_dirty_chunks();

    // ========================================================================
    // Iteration
    // ========================================================================

    // Iterate over all loaded chunks
    void for_each_chunk(const std::function<void(const ChunkPos&, Chunk&)>& callback);
    void for_each_chunk(const std::function<void(const ChunkPos&, const Chunk&)>& callback) const;

    // Get list of all loaded chunk positions
    [[nodiscard]] std::vector<ChunkPos> get_loaded_chunk_positions() const;

    // ========================================================================
    // Statistics
    // ========================================================================

    [[nodiscard]] size_t loaded_chunk_count() const;
    [[nodiscard]] size_t pending_generation_count() const;
    [[nodiscard]] size_t pending_load_count() const;
    [[nodiscard]] size_t dirty_chunk_count() const;
    [[nodiscard]] size_t total_memory_usage() const;

    // ========================================================================
    // Configuration
    // ========================================================================

    [[nodiscard]] const WorldConfig& get_config() const;
    void set_view_distance(int32_t distance);
    [[nodiscard]] uint32_t get_seed() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::world
