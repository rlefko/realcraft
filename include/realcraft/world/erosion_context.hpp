// RealCraft World System
// erosion_context.hpp - Cross-chunk erosion context for seamless terrain

#pragma once

#include "types.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace realcraft::world {

// ============================================================================
// Erosion Border Data
// ============================================================================

/// Data exchanged between adjacent chunks for seamless erosion
struct ErosionBorderData {
    ChunkPos source_chunk;
    HorizontalDirection direction;

    /// Height changes from erosion (current - original)
    std::vector<float> height_deltas;

    /// Accumulated sediment values
    std::vector<float> sediment_values;

    /// Flow accumulation for river continuity
    std::vector<float> flow_values;

    /// Dimensions of the border strip
    int32_t width = 0;  // Along the border edge (CHUNK_SIZE)
    int32_t depth = 0;  // Into the chunk (border_size)
};

// ============================================================================
// Erosion Context
// ============================================================================

/// Thread-safe context manager for coordinating erosion across chunk boundaries
///
/// When a chunk is generated, its erosion simulation produces height/sediment
/// changes in the border region. This context stores those changes so that
/// adjacent chunks can import them for seamless terrain.
class ErosionContext {
public:
    ErosionContext() = default;
    ~ErosionContext() = default;

    // Non-copyable, non-movable (due to shared_mutex member)
    ErosionContext(const ErosionContext&) = delete;
    ErosionContext& operator=(const ErosionContext&) = delete;
    ErosionContext(ErosionContext&&) = delete;
    ErosionContext& operator=(ErosionContext&&) = delete;

    // ========================================================================
    // Border Data Management
    // ========================================================================

    /// Submit erosion results from a completed chunk's border region
    /// @param data Border data including height deltas, sediment, and flow
    void submit_border_data(const ErosionBorderData& data);

    /// Retrieve border data from an adjacent chunk (if available)
    /// @param chunk The chunk requesting neighbor data
    /// @param direction Which direction to look for neighbor data
    /// @return Border data if the neighbor has been processed, nullopt otherwise
    [[nodiscard]] std::optional<ErosionBorderData> get_neighbor_border(const ChunkPos& chunk,
                                                                       HorizontalDirection direction) const;

    /// Check if neighbor erosion data is available
    /// @param chunk The chunk requesting neighbor data
    /// @param direction Which direction to check
    /// @return true if border data exists for the neighbor in that direction
    [[nodiscard]] bool has_neighbor_data(const ChunkPos& chunk, HorizontalDirection direction) const;

    // ========================================================================
    // Memory Management
    // ========================================================================

    /// Clear border data for chunks that are no longer needed
    /// @param chunk The chunk position to clear data for
    void clear_chunk_data(const ChunkPos& chunk);

    /// Clear all stored border data
    void clear_all();

    /// Get the number of chunks with stored border data
    [[nodiscard]] size_t stored_chunk_count() const;

private:
    /// Get the opposite direction (for looking up neighbor's export)
    [[nodiscard]] static HorizontalDirection opposite_direction(HorizontalDirection dir);

    /// Get the neighbor chunk position in the given direction
    [[nodiscard]] static ChunkPos neighbor_chunk(const ChunkPos& chunk, HorizontalDirection dir);

    mutable std::shared_mutex mutex_;

    /// Map from chunk position to its 4 exported border data arrays
    /// borders_[chunk][dir] = the border data exported FROM chunk IN direction dir
    std::unordered_map<ChunkPos, std::array<std::optional<ErosionBorderData>, 4>> borders_;
};

}  // namespace realcraft::world
