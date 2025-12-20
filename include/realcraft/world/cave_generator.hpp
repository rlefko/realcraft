// RealCraft World System
// cave_generator.hpp - Procedural cave generation using Perlin worms and cellular noise

#pragma once

#include "types.hpp"

#include <cstdint>
#include <memory>

namespace realcraft::world {

// ============================================================================
// Cave Configuration
// ============================================================================

struct CaveConfig {
    // World seed for deterministic generation
    uint32_t seed = 0;

    // Master enable/disable
    bool enabled = true;

    // Perlin worm configuration (natural tunnel networks)
    struct WormNoise {
        float scale = 0.02f;            // Cave size scale (smaller = larger caves)
        int octaves = 2;                // Detail level
        float threshold = 0.55f;        // Carving threshold (higher = more caves)
        float vertical_squeeze = 0.5f;  // Squash caves vertically (0.5 = half height)
    } worm;

    // Chamber configuration (large open caverns using cellular noise)
    struct ChamberNoise {
        bool enabled = true;
        float scale = 0.015f;     // Chamber size scale
        float threshold = 0.25f;  // Distance threshold from cell center
        int32_t min_y = 10;       // Minimum height for chambers
        int32_t max_y = 50;       // Maximum height for chambers
    } chamber;

    // Cave entrance configuration
    struct EntranceConfig {
        bool enabled = true;
        float chance_per_chunk = 0.15f;  // Probability of visible entrances
        int32_t min_width = 3;
        int32_t max_width = 8;
    } entrance;

    // Underground water configuration
    struct WaterTable {
        bool enabled = true;
        int32_t level = 30;         // Y level for water table
        float flood_chance = 0.3f;  // Chance caves below water table are flooded
    } water;

    // Cave decoration configuration
    struct Decorations {
        bool enabled = true;
        float stalactite_chance = 0.08f;  // Chance on ceiling surfaces
        float stalagmite_chance = 0.06f;  // Chance on floor surfaces
        float crystal_chance = 0.02f;     // Chance for crystal formations
        float moss_chance = 0.15f;        // Chance for moss in damp areas
        float mushroom_chance = 0.05f;    // Chance for glowing mushrooms
        int32_t damp_radius = 8;          // Blocks from water considered "damp"
    } decorations;

    // Vertical distribution
    int32_t min_y = 5;           // Caves don't generate below this (bedrock)
    int32_t max_y = 128;         // Caves don't generate above this
    int32_t surface_margin = 3;  // Don't carve within N blocks of surface
};

// ============================================================================
// Cave Generator
// ============================================================================

class CaveGenerator {
public:
    explicit CaveGenerator(const CaveConfig& config = {});
    ~CaveGenerator();

    // Non-copyable but movable
    CaveGenerator(const CaveGenerator&) = delete;
    CaveGenerator& operator=(const CaveGenerator&) = delete;
    CaveGenerator(CaveGenerator&&) noexcept;
    CaveGenerator& operator=(CaveGenerator&&) noexcept;

    // ========================================================================
    // Main Generation Interface
    // ========================================================================

    /// Check if a block at world coordinates should be carved (become air/water)
    /// Thread-safe, reentrant
    [[nodiscard]] bool should_carve(int64_t world_x, int64_t world_y, int64_t world_z, int32_t surface_height) const;

    /// Check if a position is inside a large chamber
    [[nodiscard]] bool is_chamber(int64_t world_x, int64_t world_y, int64_t world_z) const;

    /// Check if a carved position should be flooded with water
    [[nodiscard]] bool is_flooded(int64_t world_x, int64_t world_y, int64_t world_z) const;

    // ========================================================================
    // Decoration Queries
    // ========================================================================

    /// Check if a position should have a stalactite (ceiling decoration)
    [[nodiscard]] bool should_place_stalactite(int64_t world_x, int64_t world_y, int64_t world_z) const;

    /// Check if a position should have a stalagmite (floor decoration)
    [[nodiscard]] bool should_place_stalagmite(int64_t world_x, int64_t world_y, int64_t world_z) const;

    /// Check if a position should have a crystal
    [[nodiscard]] bool should_place_crystal(int64_t world_x, int64_t world_y, int64_t world_z) const;

    /// Check if a position should have cave moss
    [[nodiscard]] bool should_place_moss(int64_t world_x, int64_t world_y, int64_t world_z) const;

    /// Check if a position should have a glowing mushroom
    [[nodiscard]] bool should_place_mushroom(int64_t world_x, int64_t world_y, int64_t world_z) const;

    /// Check if a position is in a damp area (near water)
    [[nodiscard]] bool is_damp_area(int64_t world_x, int64_t world_y, int64_t world_z) const;

    // ========================================================================
    // Configuration
    // ========================================================================

    [[nodiscard]] const CaveConfig& get_config() const;
    void set_config(const CaveConfig& config);

    /// Reinitialize noise nodes with current config
    void rebuild_nodes();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::world
