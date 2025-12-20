// RealCraft World System
// tree_generator.hpp - L-system based tree generation with biome templates

#pragma once

#include "biome.hpp"
#include "chunk.hpp"
#include "types.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace realcraft::world {

// ============================================================================
// Tree Template
// ============================================================================

/// Tree species identifier
enum class TreeSpecies : uint8_t { Oak = 0, Spruce, Birch, Jungle, Acacia, Palm, Count };

/// Template defining tree characteristics for a species
struct TreeTemplate {
    TreeSpecies species;
    std::string name;

    // Block types
    BlockId log_block = BLOCK_AIR;
    BlockId leaf_block = BLOCK_AIR;

    // Size parameters
    int32_t min_height = 4;
    int32_t max_height = 7;
    int32_t trunk_height_ratio = 60;  // Percentage of height that is trunk

    // Canopy parameters
    int32_t canopy_radius = 2;
    int32_t canopy_height = 3;
    float leaf_density = 0.85f;

    // L-system parameters (simplified)
    float branch_angle = 25.0f;       // Branch angle in degrees
    float branch_probability = 0.4f;  // Chance of branch at each trunk level
};

// ============================================================================
// Tree Configuration
// ============================================================================

struct TreeConfig {
    uint32_t seed = 0;
    bool enabled = true;

    struct DensityConfig {
        float base_chance = 0.015f;       // Base tree placement probability
        float forest_multiplier = 4.0f;   // Multiplier for forest biomes
        float taiga_multiplier = 3.0f;    // Multiplier for taiga
        float savanna_multiplier = 0.5f;  // Lower density for savanna
        int32_t min_spacing = 2;          // Minimum blocks between trees
    } density;

    struct SizeConfig {
        float small_weight = 0.3f;   // Probability of small tree
        float medium_weight = 0.5f;  // Probability of medium tree
        float large_weight = 0.2f;   // Probability of large tree
    } size;
};

// ============================================================================
// Tree Generator
// ============================================================================

/// Represents a single tree block to place
struct TreeBlock {
    LocalBlockPos offset;  // Offset from tree base
    BlockId block;
};

class TreeGenerator {
public:
    explicit TreeGenerator(const TreeConfig& config = {});
    ~TreeGenerator();

    // Non-copyable but movable
    TreeGenerator(const TreeGenerator&) = delete;
    TreeGenerator& operator=(const TreeGenerator&) = delete;
    TreeGenerator(TreeGenerator&&) noexcept;
    TreeGenerator& operator=(TreeGenerator&&) noexcept;

    // ========================================================================
    // Query Interface
    // ========================================================================

    /// Check if a tree should be placed at this surface position
    [[nodiscard]] bool should_place_tree(int64_t world_x, int64_t world_z, BiomeType biome) const;

    /// Get the tree species for a biome
    [[nodiscard]] TreeSpecies get_species_for_biome(BiomeType biome, int64_t world_x, int64_t world_z) const;

    /// Get the tree template for a species
    [[nodiscard]] const TreeTemplate& get_template(TreeSpecies species) const;

    /// Generate tree blocks for a position
    /// Returns a vector of (offset, block_id) pairs relative to the base position
    [[nodiscard]] std::vector<TreeBlock> generate_tree(int64_t world_x, int64_t world_z, BiomeType biome,
                                                       int32_t surface_y) const;

    // ========================================================================
    // Configuration
    // ========================================================================

    [[nodiscard]] const TreeConfig& get_config() const;
    void set_config(const TreeConfig& config);
    void rebuild();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::world
