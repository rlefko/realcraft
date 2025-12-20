// RealCraft World System
// erosion_heightmap.hpp - Heightmap data structure for erosion simulation

#pragma once

#include "types.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

// Forward declaration
namespace realcraft::world {
struct ErosionBorderData;
}

namespace realcraft::world {

// Forward declaration
class TerrainGenerator;

/// Heightmap with border region for erosion simulation
/// Stores terrain heights, accumulated sediment, and flow accumulation
class ErosionHeightmap {
public:
    /// Create heightmap for a chunk with border region
    /// @param chunk_width Width of the core chunk area (typically CHUNK_SIZE_X)
    /// @param chunk_height Height of the core chunk area (typically CHUNK_SIZE_Z)
    /// @param border Extra cells around chunk for cross-chunk context
    ErosionHeightmap(int32_t chunk_width, int32_t chunk_height, int32_t border);

    // ========================================================================
    // Population
    // ========================================================================

    /// Populate heightmap from terrain generator
    /// @param generator The terrain generator to sample heights from
    /// @param chunk_pos The chunk position to generate for
    void populate_from_generator(const TerrainGenerator& generator, const ChunkPos& chunk_pos);

    /// Apply eroded heights back to a heights array (chunk core only)
    /// @param heights Output array of size chunk_width * chunk_height
    void apply_to_heights(std::array<int32_t, CHUNK_SIZE_X * CHUNK_SIZE_Z>& heights) const;

    // ========================================================================
    // Height Access
    // ========================================================================

    /// Get height at integer coordinates (includes border)
    /// @param x X coordinate in local heightmap space (0 = left edge of border)
    /// @param z Z coordinate in local heightmap space (0 = top edge of border)
    [[nodiscard]] float get(int32_t x, int32_t z) const;

    /// Set height at integer coordinates
    void set(int32_t x, int32_t z, float height);

    /// Sample height using bilinear interpolation
    /// @param x X coordinate (can be fractional)
    /// @param z Z coordinate (can be fractional)
    [[nodiscard]] float sample_bilinear(float x, float z) const;

    /// Calculate terrain gradient at a point
    /// @param x X coordinate (can be fractional)
    /// @param z Z coordinate (can be fractional)
    /// @return Gradient vector (x, y, z) where y is the height at that point
    [[nodiscard]] glm::vec3 calculate_gradient(float x, float z) const;

    /// Add to height at integer coordinates (atomic-safe for single thread)
    void add_height(int32_t x, int32_t z, float delta);

    // ========================================================================
    // Sediment Tracking
    // ========================================================================

    /// Get accumulated sediment at coordinates
    [[nodiscard]] float get_sediment(int32_t x, int32_t z) const;

    /// Add sediment at coordinates
    void add_sediment(int32_t x, int32_t z, float amount);

    /// Get sediment with bilinear weights
    void deposit_sediment_bilinear(float x, float z, float amount);

    // ========================================================================
    // Flow Accumulation (for rivers)
    // ========================================================================

    /// Get flow accumulation at coordinates
    [[nodiscard]] float get_flow(int32_t x, int32_t z) const;

    /// Set flow accumulation at coordinates
    void set_flow(int32_t x, int32_t z, float flow);

    /// Compute flow accumulation using D8 algorithm
    /// Call this after erosion simulation, before river carving
    void compute_flow_accumulation();

    // ========================================================================
    // Cross-Chunk Border Exchange
    // ========================================================================

    /// Import height changes from a neighbor chunk's border
    /// @param from_dir Direction the data is coming from (which neighbor)
    /// @param deltas Height deltas (current - original) from neighbor's border
    void import_border_heights(HorizontalDirection from_dir, const std::vector<float>& deltas);

    /// Import sediment from a neighbor chunk's border
    /// @param from_dir Direction the data is coming from
    /// @param sediment_data Sediment values from neighbor's border
    void import_border_sediment(HorizontalDirection from_dir, const std::vector<float>& sediment_data);

    /// Import flow accumulation from a neighbor chunk's border
    /// @param from_dir Direction the data is coming from
    /// @param flow_data Flow values from neighbor's border
    void import_border_flow(HorizontalDirection from_dir, const std::vector<float>& flow_data);

    /// Export this chunk's border data for a given direction
    /// @param direction Which border to export (NegX, PosX, NegZ, PosZ)
    /// @return Border data containing height deltas, sediment, and flow
    [[nodiscard]] struct ErosionBorderData export_border_data(HorizontalDirection direction) const;

    /// Store original heights after population (for computing deltas)
    void store_original_heights();

    // ========================================================================
    // Dimensions
    // ========================================================================

    /// Total width including border (chunk_width + 2 * border)
    [[nodiscard]] int32_t total_width() const { return total_width_; }

    /// Total height including border (chunk_height + 2 * border)
    [[nodiscard]] int32_t total_height() const { return total_height_; }

    /// Core chunk width (without border)
    [[nodiscard]] int32_t chunk_width() const { return chunk_width_; }

    /// Core chunk height (without border)
    [[nodiscard]] int32_t chunk_height() const { return chunk_height_; }

    /// Border size
    [[nodiscard]] int32_t border() const { return border_; }

    /// Check if coordinates are valid (within total bounds)
    [[nodiscard]] bool is_valid(int32_t x, int32_t z) const;

    /// Check if coordinates are in the core chunk area (not border)
    [[nodiscard]] bool is_in_core(int32_t x, int32_t z) const;

    // ========================================================================
    // Buffer Access (for GPU transfer)
    // ========================================================================

    /// Get raw height data as float buffer
    [[nodiscard]] const std::vector<float>& heights() const { return heights_; }

    /// Get mutable height data
    [[nodiscard]] std::vector<float>& heights() { return heights_; }

    /// Get raw sediment data
    [[nodiscard]] const std::vector<float>& sediment() const { return sediment_; }

    /// Get mutable sediment data
    [[nodiscard]] std::vector<float>& sediment() { return sediment_; }

private:
    int32_t chunk_width_;
    int32_t chunk_height_;
    int32_t border_;
    int32_t total_width_;
    int32_t total_height_;

    std::vector<float> heights_;
    std::vector<float> sediment_;
    std::vector<float> flow_;
    std::vector<float> original_heights_;  // Heights before erosion, for computing deltas

    /// Convert 2D coordinates to linear index
    [[nodiscard]] size_t index(int32_t x, int32_t z) const;

    /// Compute border region coordinates for a given direction
    /// Returns start_x, start_z, width, height of the border strip
    void compute_border_region(HorizontalDirection dir, int32_t& start_x, int32_t& start_z, int32_t& width,
                               int32_t& height) const;
};

}  // namespace realcraft::world
