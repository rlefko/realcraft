// RealCraft World System
// terrain_generator.hpp - Procedural terrain generation using FastNoise2

#pragma once

#include "biome.hpp"
#include "cave_generator.hpp"
#include "chunk.hpp"
#include "climate.hpp"
#include "erosion.hpp"
#include "ore_generator.hpp"
#include "structure_generator.hpp"
#include "tree_generator.hpp"
#include "types.hpp"
#include "vegetation_generator.hpp"

#include <array>
#include <cstdint>
#include <memory>

namespace realcraft::world {

// ============================================================================
// Terrain Configuration
// ============================================================================

struct TerrainConfig {
    // World seed for deterministic generation
    uint32_t seed = 0;

    // Height parameters
    int32_t sea_level = 64;         // Water fills up to this height
    int32_t base_height = 64;       // Average terrain height
    int32_t height_variation = 48;  // Max deviation from base height
    int32_t min_height = 1;         // Lowest terrain (bedrock layer)
    int32_t max_height = 200;       // Highest peaks allowed

    // Continental/base terrain noise (large-scale terrain shape)
    struct ContinentalNoise {
        float scale = 0.002f;     // Very large features (0.001-0.005)
        int octaves = 4;          // Number of noise layers
        float gain = 0.5f;        // Persistence - amplitude reduction per octave
        float lacunarity = 2.0f;  // Frequency increase per octave
        float weight = 0.6f;      // Contribution to final height
    } continental;

    // Mountain/ridge terrain noise (medium scale, sharp features)
    struct MountainNoise {
        float scale = 0.01f;      // Medium features
        int octaves = 6;          // More octaves for detail
        float gain = 0.5f;        // Persistence
        float lacunarity = 2.0f;  // Lacunarity
        float weight = 0.3f;      // Contribution to final height
    } mountain;

    // Fine detail noise (small-scale variation)
    struct DetailNoise {
        float scale = 0.05f;      // Fine details
        int octaves = 4;          // Moderate detail
        float gain = 0.5f;        // Persistence
        float lacunarity = 2.0f;  // Lacunarity
        float weight = 0.1f;      // Contribution to final height
    } detail;

    // Domain warping (makes terrain more organic/natural)
    struct DomainWarpConfig {
        bool enabled = true;      // Enable/disable domain warping
        float amplitude = 20.0f;  // Warp distance in blocks
        float scale = 0.01f;      // Warp noise scale
    } domain_warp;

    // 3D density for caves and overhangs
    struct DensityNoise {
        bool enabled = true;         // Enable 3D density (caves)
        float scale = 0.03f;         // Cave size scale
        int octaves = 3;             // Cave detail level
        float gain = 0.5f;           // Persistence
        float lacunarity = 2.0f;     // Lacunarity
        float threshold = 0.0f;      // Density cutoff for air
        float surface_blend = 8.0f;  // Blend distance from surface
    } density;

    // Block layer depths
    int32_t dirt_depth = 4;   // Dirt layer thickness below surface
    int32_t grass_depth = 1;  // Grass on top (for surface above sea level)

    // Biome system configuration
    struct BiomeSystemConfig {
        bool enabled = true;                 // Enable biome-based block selection
        ClimateConfig climate;               // Climate noise parameters
        bool apply_height_modifiers = true;  // Apply per-biome height modifiers
    } biome_system;

    // Erosion system configuration
    ErosionConfig erosion;

    // Cave generation configuration
    CaveConfig caves;

    // Feature generation configuration (Milestone 4.5)
    OreConfig ores;
    VegetationConfig vegetation;
    TreeConfig trees;
    StructureConfig structures;
};

// ============================================================================
// Terrain Generator
// ============================================================================

class TerrainGenerator {
public:
    explicit TerrainGenerator(const TerrainConfig& config = {});
    ~TerrainGenerator();

    // Non-copyable but movable
    TerrainGenerator(const TerrainGenerator&) = delete;
    TerrainGenerator& operator=(const TerrainGenerator&) = delete;
    TerrainGenerator(TerrainGenerator&&) noexcept;
    TerrainGenerator& operator=(TerrainGenerator&&) noexcept;

    // ========================================================================
    // Main Generation Interface
    // ========================================================================

    /// Generate terrain for a chunk (thread-safe, reentrant)
    void generate(Chunk& chunk) const;

    /// Generate heightmap for a chunk position (for preview/debugging)
    /// Returns a CHUNK_SIZE_X * CHUNK_SIZE_Z array of heights
    [[nodiscard]] std::array<int32_t, CHUNK_SIZE_X * CHUNK_SIZE_Z> generate_heightmap(const ChunkPos& pos) const;

    // ========================================================================
    // Individual Noise Queries (for debugging/testing)
    // ========================================================================

    /// Get terrain height at world X,Z coordinates
    [[nodiscard]] int32_t get_height(int64_t world_x, int64_t world_z) const;

    /// Get 3D density at world coordinates (positive = solid, negative = air)
    [[nodiscard]] float get_density(int64_t world_x, int64_t world_y, int64_t world_z) const;

    /// Get density without heightmap blending (raw 3D noise)
    [[nodiscard]] float get_raw_density(int64_t world_x, int64_t world_y, int64_t world_z) const;

    // ========================================================================
    // Biome Queries (requires biome_system.enabled = true)
    // ========================================================================

    /// Get biome type at world coordinates
    [[nodiscard]] BiomeType get_biome(int64_t world_x, int64_t world_z) const;

    /// Get climate sample at world coordinates (temperature, humidity, biome)
    [[nodiscard]] ClimateSample get_climate(int64_t world_x, int64_t world_z) const;

    /// Get blended biome info for smooth transitions
    [[nodiscard]] BiomeBlend get_biome_blend(int64_t world_x, int64_t world_z) const;

    // ========================================================================
    // Configuration
    // ========================================================================

    [[nodiscard]] const TerrainConfig& get_config() const;
    void set_config(const TerrainConfig& config);

    /// Reinitialize noise nodes with current config (call after set_config)
    void rebuild_nodes();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::world
