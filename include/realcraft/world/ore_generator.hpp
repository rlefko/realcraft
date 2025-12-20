// RealCraft World System
// ore_generator.hpp - Procedural ore vein generation with depth-based distribution

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
// Ore Vein Configuration
// ============================================================================

/// Configuration for a single ore type
struct OreVeinConfig {
    std::string name;             // Ore name for debugging
    BlockId ore_block;            // Block ID to place
    int32_t min_y;                // Minimum Y level for spawning
    int32_t max_y;                // Maximum Y level for spawning
    int32_t min_vein_size;        // Minimum blocks per vein
    int32_t max_vein_size;        // Maximum blocks per vein
    int32_t veins_per_chunk;      // Number of veins to attempt per chunk
    bool mountains_only = false;  // Only spawn in mountain biomes (e.g., emerald)
};

// ============================================================================
// Ore Generator Configuration
// ============================================================================

struct OreConfig {
    uint32_t seed = 0;
    bool enabled = true;

    /// Get default ore configurations
    static std::vector<OreVeinConfig> get_default_ores();
};

// ============================================================================
// Ore Generator
// ============================================================================

/// Represents a single ore block position within a chunk
struct OrePosition {
    LocalBlockPos pos;
    BlockId block;
};

class OreGenerator {
public:
    explicit OreGenerator(const OreConfig& config = {});
    ~OreGenerator();

    // Non-copyable but movable
    OreGenerator(const OreGenerator&) = delete;
    OreGenerator& operator=(const OreGenerator&) = delete;
    OreGenerator(OreGenerator&&) noexcept;
    OreGenerator& operator=(OreGenerator&&) noexcept;

    // ========================================================================
    // Generation Interface
    // ========================================================================

    /// Generate all ore positions for a chunk
    /// Returns a vector of (position, block_id) pairs for ore blocks
    [[nodiscard]] std::vector<OrePosition> generate_ores(const ChunkPos& chunk_pos, BiomeType biome) const;

    /// Check if a specific position should be an ore block
    /// Returns the ore block ID or BLOCK_AIR if no ore
    [[nodiscard]] BlockId get_ore_at(int64_t world_x, int64_t world_y, int64_t world_z, BiomeType biome) const;

    // ========================================================================
    // Configuration
    // ========================================================================

    [[nodiscard]] const OreConfig& get_config() const;
    void set_config(const OreConfig& config);

    /// Get the list of ore configurations
    [[nodiscard]] const std::vector<OreVeinConfig>& get_ore_configs() const;

    /// Set custom ore configurations
    void set_ore_configs(std::vector<OreVeinConfig> ores);

    /// Reinitialize with current config
    void rebuild();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::world
