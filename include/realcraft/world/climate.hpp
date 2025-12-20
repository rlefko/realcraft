// RealCraft World System
// climate.hpp - Climate model for biome distribution

#pragma once

#include "biome.hpp"
#include "types.hpp"

#include <cstdint>
#include <memory>

namespace realcraft::world {

// ============================================================================
// Climate Configuration
// ============================================================================

struct ClimateConfig {
    uint32_t seed = 0;

    // Temperature noise (large scale, varies by latitude simulation)
    struct TemperatureNoise {
        float scale = 0.001f;     // Very large features (continental scale)
        int octaves = 3;          // Moderate complexity
        float gain = 0.5f;        // Persistence
        float lacunarity = 2.0f;  // Frequency increase per octave
    } temperature;

    // Humidity/rainfall noise
    struct HumidityNoise {
        float scale = 0.002f;     // Large features
        int octaves = 3;          // Moderate complexity
        float gain = 0.5f;        // Persistence
        float lacunarity = 2.0f;  // Frequency increase per octave
    } humidity;

    // Altitude influence on temperature
    float altitude_temperature_factor = 0.015f;  // Temperature drops per block above sea level
    float sea_level = 64.0f;                     // Reference sea level

    // Blend radius for smooth biome transitions (in blocks)
    int blend_radius = 4;
};

// ============================================================================
// Climate Sample Result
// ============================================================================

struct ClimateSample {
    float temperature = 0.5f;       // 0.0 (cold) to 1.0 (hot)
    float humidity = 0.5f;          // 0.0 (dry) to 1.0 (wet)
    BiomeType biome = BiomeType::Plains;  // Determined biome at this location
};

// ============================================================================
// Biome Blend Info (for smooth transitions)
// ============================================================================

struct BiomeBlend {
    BiomeType primary_biome = BiomeType::Plains;
    BiomeType secondary_biome = BiomeType::Plains;
    float blend_factor = 0.0f;  // 0.0 = fully primary, 1.0 = fully secondary

    // Blended height modifiers (computed from both biomes)
    BiomeHeightModifiers blended_height;
};

// ============================================================================
// Climate Map (generates climate values at world coordinates)
// ============================================================================

class ClimateMap {
public:
    explicit ClimateMap(const ClimateConfig& config = {});
    ~ClimateMap();

    // Non-copyable but movable
    ClimateMap(const ClimateMap&) = delete;
    ClimateMap& operator=(const ClimateMap&) = delete;
    ClimateMap(ClimateMap&&) noexcept;
    ClimateMap& operator=(ClimateMap&&) noexcept;

    // ========================================================================
    // Sampling Interface (thread-safe)
    // ========================================================================

    /// Get raw climate values at world coordinates
    /// Height is used to adjust temperature (higher = colder)
    [[nodiscard]] ClimateSample sample(int64_t world_x, int64_t world_z, float height = 64.0f) const;

    /// Get biome with blend info for smooth transitions
    /// Uses sampling around the point to detect biome boundaries
    [[nodiscard]] BiomeBlend sample_blended(int64_t world_x, int64_t world_z, float height = 64.0f) const;

    /// Get just the biome type (convenience method)
    [[nodiscard]] BiomeType get_biome(int64_t world_x, int64_t world_z, float height = 64.0f) const;

    /// Get raw temperature at world coordinates (without altitude adjustment)
    [[nodiscard]] float get_raw_temperature(int64_t world_x, int64_t world_z) const;

    /// Get raw humidity at world coordinates
    [[nodiscard]] float get_raw_humidity(int64_t world_x, int64_t world_z) const;

    // ========================================================================
    // Configuration
    // ========================================================================

    [[nodiscard]] const ClimateConfig& get_config() const;
    void set_config(const ClimateConfig& config);

    /// Reinitialize noise nodes with current config (call after set_config)
    void rebuild_nodes();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::world
