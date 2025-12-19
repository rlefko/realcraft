// RealCraft World System
// chunk_data.hpp - Palette-based voxel storage with RLE compression

#pragma once

#include "types.hpp"

#include <memory>
#include <span>
#include <vector>

namespace realcraft::world {

// ============================================================================
// Palette Entry
// ============================================================================

struct PaletteEntry {
    BlockId block_id = BLOCK_AIR;
    BlockStateId state_id = 0;

    bool operator==(const PaletteEntry& other) const {
        return block_id == other.block_id && state_id == other.state_id;
    }

    bool operator!=(const PaletteEntry& other) const { return !(*this == other); }

    // Create an air entry
    static PaletteEntry air() { return {BLOCK_AIR, 0}; }

    // Create entry with block ID only
    static PaletteEntry from_block(BlockId id) { return {id, 0}; }
};

// ============================================================================
// Voxel Storage (palette-based with RLE compression for serialization)
// ============================================================================

class VoxelStorage {
public:
    VoxelStorage();
    ~VoxelStorage();

    // Non-copyable but movable
    VoxelStorage(const VoxelStorage&) = delete;
    VoxelStorage& operator=(const VoxelStorage&) = delete;
    VoxelStorage(VoxelStorage&&) noexcept;
    VoxelStorage& operator=(VoxelStorage&&) noexcept;

    // ========================================================================
    // Block Access
    // ========================================================================

    [[nodiscard]] PaletteEntry get(const LocalBlockPos& pos) const;
    [[nodiscard]] PaletteEntry get(size_t index) const;
    void set(const LocalBlockPos& pos, const PaletteEntry& entry);
    void set(size_t index, const PaletteEntry& entry);

    // Convenience accessors
    [[nodiscard]] BlockId get_block(const LocalBlockPos& pos) const;
    [[nodiscard]] BlockStateId get_state(const LocalBlockPos& pos) const;
    void set_block(const LocalBlockPos& pos, BlockId id, BlockStateId state = 0);

    // ========================================================================
    // Fill Operations
    // ========================================================================

    void fill(const PaletteEntry& entry);
    void fill_region(const LocalBlockPos& min, const LocalBlockPos& max, const PaletteEntry& entry);

    // ========================================================================
    // Palette Management
    // ========================================================================

    [[nodiscard]] size_t palette_size() const;
    [[nodiscard]] const std::vector<PaletteEntry>& get_palette() const;
    void optimize_palette();  // Remove unused entries, re-index

    // ========================================================================
    // Statistics
    // ========================================================================

    [[nodiscard]] bool is_empty() const;    // All air
    [[nodiscard]] bool is_uniform() const;  // Single block type throughout
    [[nodiscard]] size_t non_air_count() const;
    [[nodiscard]] size_t unique_block_count() const;

    // ========================================================================
    // Serialization Support
    // ========================================================================

    // Serialize to RLE-compressed format
    [[nodiscard]] std::vector<uint8_t> serialize_rle() const;

    // Deserialize from RLE-compressed format
    bool deserialize_rle(std::span<const uint8_t> data);

    // Raw palette + indices (for debugging/testing)
    [[nodiscard]] std::vector<uint8_t> serialize_raw() const;
    bool deserialize_raw(std::span<const uint8_t> data);

    // ========================================================================
    // Memory
    // ========================================================================

    [[nodiscard]] size_t memory_usage() const;

    // Get bits per voxel (4, 8, or 16 based on palette size)
    [[nodiscard]] uint8_t bits_per_voxel() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Chunk Section (16x16x16 sub-chunk for LOD and partial updates)
// ============================================================================

class ChunkSection {
public:
    static constexpr int32_t SIZE = SUBCHUNK_SIZE;
    static constexpr int32_t VOLUME = SIZE * SIZE * SIZE;

    ChunkSection();
    ~ChunkSection();

    // Non-copyable but movable
    ChunkSection(const ChunkSection&) = delete;
    ChunkSection& operator=(const ChunkSection&) = delete;
    ChunkSection(ChunkSection&&) noexcept;
    ChunkSection& operator=(ChunkSection&&) noexcept;

    // Block access (coordinates relative to section, [0, SIZE))
    [[nodiscard]] PaletteEntry get(int32_t x, int32_t y, int32_t z) const;
    void set(int32_t x, int32_t y, int32_t z, const PaletteEntry& entry);

    [[nodiscard]] bool is_empty() const;
    [[nodiscard]] size_t non_air_count() const;

    // Serialization
    [[nodiscard]] std::vector<uint8_t> serialize() const;
    bool deserialize(std::span<const uint8_t> data);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::world
