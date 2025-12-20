// RealCraft World System
// vegetation_generator.hpp - Ground cover generation (grass, flowers, bushes)

#pragma once

#include "biome.hpp"
#include "chunk.hpp"
#include "types.hpp"

#include <cstdint>
#include <memory>

namespace realcraft::world {

// ============================================================================
// Vegetation Configuration
// ============================================================================

struct VegetationConfig {
    uint32_t seed = 0;
    bool enabled = true;

    struct GrassConfig {
        bool enabled = true;
        float density = 0.35f;          // Probability on valid grass block
        float tall_grass_ratio = 0.7f;  // Ratio of tall grass vs ferns
    } grass;

    struct FlowerConfig {
        bool enabled = true;
        float density = 0.03f;         // Base flower probability
        float cluster_chance = 0.15f;  // Chance of flower cluster
        float cluster_radius = 2.5f;   // Cluster spread radius
    } flowers;

    struct BushConfig {
        bool enabled = true;
        float density = 0.015f;  // Bush probability
    } bushes;

    struct CactusConfig {
        bool enabled = true;
        float density = 0.008f;  // Cactus probability in desert
        int32_t max_height = 3;  // Maximum cactus height
    } cactus;

    struct DeadBushConfig {
        bool enabled = true;
        float density = 0.02f;  // Dead bush probability
    } dead_bush;
};

// ============================================================================
// Vegetation Generator
// ============================================================================

/// Represents vegetation to place at a position
struct VegetationPlacement {
    LocalBlockPos pos;
    BlockId block;
    int32_t height;  // For multi-block vegetation like cacti (1 for single blocks)
};

class VegetationGenerator {
public:
    explicit VegetationGenerator(const VegetationConfig& config = {});
    ~VegetationGenerator();

    // Non-copyable but movable
    VegetationGenerator(const VegetationGenerator&) = delete;
    VegetationGenerator& operator=(const VegetationGenerator&) = delete;
    VegetationGenerator(VegetationGenerator&&) noexcept;
    VegetationGenerator& operator=(VegetationGenerator&&) noexcept;

    // ========================================================================
    // Query Interface
    // ========================================================================

    /// Check if grass should be placed at this surface position
    [[nodiscard]] bool should_place_grass(int64_t world_x, int64_t world_z, BiomeType biome) const;

    /// Get the specific grass block to place (tall_grass, fern, etc.)
    [[nodiscard]] BlockId get_grass_block(int64_t world_x, int64_t world_z, BiomeType biome) const;

    /// Check if a flower should be placed and return the flower block ID
    /// Returns BLOCK_AIR if no flower
    [[nodiscard]] BlockId get_flower_block(int64_t world_x, int64_t world_z, BiomeType biome) const;

    /// Check if a bush should be placed
    [[nodiscard]] bool should_place_bush(int64_t world_x, int64_t world_z, BiomeType biome) const;

    /// Check if a dead bush should be placed
    [[nodiscard]] bool should_place_dead_bush(int64_t world_x, int64_t world_z, BiomeType biome) const;

    /// Check if a cactus should be placed (desert only)
    [[nodiscard]] bool should_place_cactus(int64_t world_x, int64_t world_z, BiomeType biome) const;

    /// Get cactus height at this position (1-3)
    [[nodiscard]] int32_t get_cactus_height(int64_t world_x, int64_t world_z) const;

    /// Check if lily pad should be placed (swamp water surface)
    [[nodiscard]] bool should_place_lily_pad(int64_t world_x, int64_t world_z, BiomeType biome) const;

    // ========================================================================
    // Configuration
    // ========================================================================

    [[nodiscard]] const VegetationConfig& get_config() const;
    void set_config(const VegetationConfig& config);
    void rebuild();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::world
