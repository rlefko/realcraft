// RealCraft World System
// biome.hpp - Biome type definitions and configuration

#pragma once

#include "types.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace realcraft::world {

// ============================================================================
// Biome Type Enumeration
// ============================================================================

enum class BiomeType : uint8_t {
    Ocean = 0,
    Beach,
    Desert,
    Savanna,
    Plains,
    Forest,
    Swamp,
    Taiga,
    Tundra,
    Mountains,
    SnowyMountains,
    River,
    Count  // Must be last
};

inline constexpr size_t BIOME_COUNT = static_cast<size_t>(BiomeType::Count);

/// Convert biome type to string name
[[nodiscard]] const char* biome_type_to_string(BiomeType type);

/// Convert string name to biome type
[[nodiscard]] BiomeType biome_type_from_string(std::string_view name);

// ============================================================================
// Biome Block Palette
// ============================================================================

struct BiomeBlockPalette {
    BlockId surface_block = BLOCK_INVALID;     // Top surface (grass, sand, etc.)
    BlockId subsurface_block = BLOCK_INVALID;  // Below surface (dirt)
    BlockId underwater_block = BLOCK_INVALID;  // Underwater surface (sand, gravel)
    BlockId stone_block = BLOCK_INVALID;       // Deep underground (stone)

    // Optional variety blocks (for future feature variety)
    BlockId surface_alt = BLOCK_INVALID;  // Alternate surface (e.g., coarse dirt)
    float surface_alt_chance = 0.0f;      // Probability of alternate (0-1)
};

// ============================================================================
// Biome Height Modifiers
// ============================================================================

struct BiomeHeightModifiers {
    float height_scale = 1.0f;        // Multiplier for height variation
    float height_offset = 0.0f;       // Base height adjustment (blocks)
    float detail_scale = 1.0f;        // Modifier for fine detail noise
    float continental_weight = 1.0f;  // Weight for large-scale terrain
    float mountain_weight = 1.0f;     // Weight for ridge/mountain features
};

// ============================================================================
// Biome Climate Range
// ============================================================================

struct BiomeClimateRange {
    float temperature_min = 0.0f;  // Minimum temperature (0-1)
    float temperature_max = 1.0f;  // Maximum temperature (0-1)
    float humidity_min = 0.0f;     // Minimum humidity (0-1)
    float humidity_max = 1.0f;     // Maximum humidity (0-1)
};

// ============================================================================
// Biome Configuration
// ============================================================================

struct BiomeConfig {
    BiomeType type = BiomeType::Plains;
    std::string name;
    std::string display_name;

    BiomeBlockPalette blocks;
    BiomeHeightModifiers height;
    BiomeClimateRange climate;

    // Special flags
    bool is_ocean = false;        // Uses water level logic
    bool is_cold = false;         // Ice forms on water
    bool is_mountainous = false;  // Ignores climate for height-based selection
};

// ============================================================================
// Biome Registry (read-only after initialization)
// ============================================================================

class BiomeRegistry {
public:
    static BiomeRegistry& instance();

    // Non-copyable, non-movable
    BiomeRegistry(const BiomeRegistry&) = delete;
    BiomeRegistry& operator=(const BiomeRegistry&) = delete;
    BiomeRegistry(BiomeRegistry&&) = delete;
    BiomeRegistry& operator=(BiomeRegistry&&) = delete;

    /// Register default biome configurations (must be called after BlockRegistry::register_defaults())
    void register_defaults();

    /// Check if defaults have been registered
    [[nodiscard]] bool is_initialized() const;

    /// Get biome configuration by type
    [[nodiscard]] const BiomeConfig& get(BiomeType type) const;

    /// Get block palette for a biome
    [[nodiscard]] const BiomeBlockPalette& get_palette(BiomeType type) const;

    /// Get height modifiers for a biome
    [[nodiscard]] const BiomeHeightModifiers& get_height_modifiers(BiomeType type) const;

    /// Find biome from climate values (temperature, humidity, height)
    /// Height is used to determine mountain biomes
    [[nodiscard]] BiomeType find_biome(float temperature, float humidity, float height) const;

    /// Get all biome configurations
    [[nodiscard]] const std::array<BiomeConfig, BIOME_COUNT>& get_all() const;

private:
    BiomeRegistry();
    ~BiomeRegistry();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Utility Functions
// ============================================================================

/// Blend two biome height modifiers based on blend factor (0 = a, 1 = b)
[[nodiscard]] BiomeHeightModifiers blend_height_modifiers(const BiomeHeightModifiers& a,
                                                          const BiomeHeightModifiers& b,
                                                          float blend_factor);

}  // namespace realcraft::world
