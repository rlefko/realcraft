// RealCraft World System
// chunk.hpp - Thread-safe chunk class with voxel data and metadata

#pragma once

#include "chunk_data.hpp"
#include "types.hpp"

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <vector>

namespace realcraft::world {

// Forward declarations
class WorldManager;
class Chunk;

// ============================================================================
// Chunk Metadata
// ============================================================================

struct ChunkMetadata {
    uint8_t biome_id = 0;                   // Primary biome
    uint32_t generation_seed = 0;           // Seed used for generation
    int64_t last_modified_time = 0;         // Unix timestamp
    uint32_t tick_count = 0;                // Total ticks processed
    bool has_been_generated = false;        // True if procedurally generated
    bool has_player_modifications = false;  // True if player changed blocks
};

// ============================================================================
// Chunk Descriptor (for creation)
// ============================================================================

struct ChunkDesc {
    ChunkPos position{0, 0};
    bool generate_terrain = true;  // False for loading from disk
    uint32_t seed = 0;             // World seed for generation
    std::string debug_name;
};

// ============================================================================
// Chunk Class
// ============================================================================

class Chunk {
public:
    explicit Chunk(const ChunkDesc& desc);
    ~Chunk();

    // Non-copyable, non-movable (owned by WorldManager)
    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;
    Chunk(Chunk&&) = delete;
    Chunk& operator=(Chunk&&) = delete;

    // ========================================================================
    // Position
    // ========================================================================

    [[nodiscard]] ChunkPos get_position() const { return position_; }
    [[nodiscard]] WorldBlockPos get_world_origin() const;

    // ========================================================================
    // State
    // ========================================================================

    [[nodiscard]] ChunkState get_state() const;
    [[nodiscard]] bool is_ready() const;  // State == Loaded or Modified
    [[nodiscard]] bool is_dirty() const;  // Has unsaved changes
    void mark_dirty();
    void mark_clean();

    // ========================================================================
    // Block Access (thread-safe, acquires locks automatically)
    // ========================================================================

    // Read lock acquired automatically
    [[nodiscard]] BlockId get_block(const LocalBlockPos& pos) const;
    [[nodiscard]] BlockStateId get_block_state(const LocalBlockPos& pos) const;
    [[nodiscard]] PaletteEntry get_entry(const LocalBlockPos& pos) const;

    // Write lock acquired automatically
    void set_block(const LocalBlockPos& pos, BlockId id, BlockStateId state = 0);
    void set_entry(const LocalBlockPos& pos, const PaletteEntry& entry);

    // ========================================================================
    // Batch Operations (for performance, hold lock for duration)
    // ========================================================================

    class ReadLock;
    class WriteLock;
    [[nodiscard]] ReadLock read_lock() const;
    [[nodiscard]] WriteLock write_lock();

    // ========================================================================
    // Neighbor Access
    // ========================================================================

    void set_neighbor(HorizontalDirection dir, Chunk* neighbor);
    [[nodiscard]] Chunk* get_neighbor(HorizontalDirection dir) const;
    [[nodiscard]] bool has_all_neighbors() const;

    // Cross-chunk block access (follows neighbor pointers if needed)
    [[nodiscard]] std::optional<PaletteEntry> get_entry_or_neighbor(const LocalBlockPos& pos) const;

    // ========================================================================
    // Metadata
    // ========================================================================

    [[nodiscard]] const ChunkMetadata& get_metadata() const { return metadata_; }
    [[nodiscard]] ChunkMetadata& get_metadata_mut() { return metadata_; }

    // ========================================================================
    // Serialization
    // ========================================================================

    [[nodiscard]] std::vector<uint8_t> serialize() const;
    bool deserialize(std::span<const uint8_t> data);

    // ========================================================================
    // Statistics
    // ========================================================================

    [[nodiscard]] size_t memory_usage() const;
    [[nodiscard]] size_t non_air_count() const;

    // ========================================================================
    // Direct Storage Access (use with caution, requires external locking)
    // ========================================================================

    [[nodiscard]] const VoxelStorage& get_storage() const { return storage_; }
    [[nodiscard]] VoxelStorage& get_storage_mut() { return storage_; }

private:
    friend class WorldManager;

    void set_state(ChunkState state);

    ChunkPos position_;
    std::atomic<ChunkState> state_{ChunkState::Unloaded};
    std::atomic<bool> dirty_{false};

    VoxelStorage storage_;
    ChunkMetadata metadata_;

    std::array<Chunk*, 4> neighbors_{};  // NegX, PosX, NegZ, PosZ (horizontal only)

    mutable std::shared_mutex mutex_;
};

// ============================================================================
// RAII Lock Classes
// ============================================================================

class Chunk::ReadLock {
public:
    explicit ReadLock(const Chunk& chunk);
    ~ReadLock();

    // Non-copyable, movable
    ReadLock(const ReadLock&) = delete;
    ReadLock& operator=(const ReadLock&) = delete;
    ReadLock(ReadLock&& other) noexcept;
    ReadLock& operator=(ReadLock&&) = delete;

    // Block access (no locking, already held)
    [[nodiscard]] BlockId get_block(const LocalBlockPos& pos) const;
    [[nodiscard]] BlockStateId get_block_state(const LocalBlockPos& pos) const;
    [[nodiscard]] PaletteEntry get_entry(const LocalBlockPos& pos) const;
    [[nodiscard]] PaletteEntry get_entry(size_t index) const;

    // Statistics
    [[nodiscard]] size_t non_air_count() const;
    [[nodiscard]] bool is_empty() const;

    // Direct storage access
    [[nodiscard]] const VoxelStorage& storage() const;

private:
    const Chunk* chunk_;
    std::shared_lock<std::shared_mutex> lock_;
};

class Chunk::WriteLock {
public:
    explicit WriteLock(Chunk& chunk);
    ~WriteLock();

    // Non-copyable, movable
    WriteLock(const WriteLock&) = delete;
    WriteLock& operator=(const WriteLock&) = delete;
    WriteLock(WriteLock&& other) noexcept;
    WriteLock& operator=(WriteLock&&) = delete;

    // Block access (no locking, already held)
    [[nodiscard]] BlockId get_block(const LocalBlockPos& pos) const;
    [[nodiscard]] PaletteEntry get_entry(const LocalBlockPos& pos) const;

    // Block modification
    void set_block(const LocalBlockPos& pos, BlockId id, BlockStateId state = 0);
    void set_entry(const LocalBlockPos& pos, const PaletteEntry& entry);
    void set_entry(size_t index, const PaletteEntry& entry);

    // Bulk operations
    void fill(const PaletteEntry& entry);
    void fill_region(const LocalBlockPos& min, const LocalBlockPos& max, const PaletteEntry& entry);

    // Direct storage access
    [[nodiscard]] VoxelStorage& storage();

private:
    Chunk* chunk_;
    std::unique_lock<std::shared_mutex> lock_;
};

}  // namespace realcraft::world
