// RealCraft World System
// biome.cpp - Biome type definitions and registry implementation

#include <algorithm>
#include <array>
#include <realcraft/world/biome.hpp>
#include <realcraft/world/block.hpp>
#include <stdexcept>

namespace realcraft::world {

// ============================================================================
// BiomeType String Conversions
// ============================================================================

namespace {
constexpr std::array<const char*, BIOME_COUNT> BIOME_NAMES = {"ocean",  "beach",     "desert",          "savanna",
                                                              "plains", "forest",    "swamp",           "taiga",
                                                              "tundra", "mountains", "snowy_mountains", "river"};
}  // namespace

const char* biome_type_to_string(BiomeType type) {
    auto index = static_cast<size_t>(type);
    if (index >= BIOME_COUNT) {
        return "unknown";
    }
    return BIOME_NAMES[index];
}

BiomeType biome_type_from_string(std::string_view name) {
    for (size_t i = 0; i < BIOME_COUNT; ++i) {
        if (name == BIOME_NAMES[i]) {
            return static_cast<BiomeType>(i);
        }
    }
    return BiomeType::Plains;  // Default fallback
}

// ============================================================================
// BiomeHeightModifiers Blending
// ============================================================================

BiomeHeightModifiers blend_height_modifiers(const BiomeHeightModifiers& a, const BiomeHeightModifiers& b,
                                            float blend_factor) {
    float t = std::clamp(blend_factor, 0.0f, 1.0f);
    float inv_t = 1.0f - t;

    BiomeHeightModifiers result;
    result.height_scale = a.height_scale * inv_t + b.height_scale * t;
    result.height_offset = a.height_offset * inv_t + b.height_offset * t;
    result.detail_scale = a.detail_scale * inv_t + b.detail_scale * t;
    result.continental_weight = a.continental_weight * inv_t + b.continental_weight * t;
    result.mountain_weight = a.mountain_weight * inv_t + b.mountain_weight * t;
    return result;
}

// ============================================================================
// BiomeRegistry Implementation
// ============================================================================

struct BiomeRegistry::Impl {
    std::array<BiomeConfig, BIOME_COUNT> biomes;
    bool initialized = false;

    // Default biome for uninitialized access
    BiomeConfig default_biome;

    Impl() {
        // Initialize default biome with safe values
        default_biome.type = BiomeType::Plains;
        default_biome.name = "realcraft:plains";
        default_biome.display_name = "Plains";
    }
};

BiomeRegistry::BiomeRegistry() : impl_(std::make_unique<Impl>()) {}

BiomeRegistry::~BiomeRegistry() = default;

BiomeRegistry& BiomeRegistry::instance() {
    static BiomeRegistry registry;
    return registry;
}

bool BiomeRegistry::is_initialized() const {
    return impl_->initialized;
}

void BiomeRegistry::register_defaults() {
    if (impl_->initialized) {
        return;  // Already initialized
    }

    const auto& block_registry = BlockRegistry::instance();

    // Get common block IDs
    BlockId stone_id = block_registry.stone_id();
    BlockId dirt_id = block_registry.dirt_id();
    BlockId grass_id = block_registry.grass_id();
    BlockId sand_id = block_registry.sand_id();

    // Look up gravel (not cached in BlockRegistry)
    BlockId gravel_id = BLOCK_INVALID;
    if (auto id = block_registry.find_id("realcraft:gravel")) {
        gravel_id = *id;
    } else {
        gravel_id = stone_id;  // Fallback if not registered
    }

    // ========================================================================
    // Ocean
    // ========================================================================
    {
        BiomeConfig& biome = impl_->biomes[static_cast<size_t>(BiomeType::Ocean)];
        biome.type = BiomeType::Ocean;
        biome.name = "realcraft:ocean";
        biome.display_name = "Ocean";
        biome.is_ocean = true;

        biome.blocks.surface_block = sand_id;
        biome.blocks.subsurface_block = sand_id;
        biome.blocks.underwater_block = sand_id;
        biome.blocks.stone_block = stone_id;

        biome.height.height_scale = 0.3f;  // Flat ocean floor
        biome.height.height_offset = -20.0f;

        // Ocean covers all climate values when below sea level
        biome.climate = {0.0f, 1.0f, 0.0f, 1.0f};
    }

    // ========================================================================
    // Beach
    // ========================================================================
    {
        BiomeConfig& biome = impl_->biomes[static_cast<size_t>(BiomeType::Beach)];
        biome.type = BiomeType::Beach;
        biome.name = "realcraft:beach";
        biome.display_name = "Beach";

        biome.blocks.surface_block = sand_id;
        biome.blocks.subsurface_block = sand_id;
        biome.blocks.underwater_block = sand_id;
        biome.blocks.stone_block = stone_id;

        biome.height.height_scale = 0.5f;

        biome.climate = {0.0f, 1.0f, 0.0f, 1.0f};
    }

    // ========================================================================
    // Desert
    // ========================================================================
    {
        BiomeConfig& biome = impl_->biomes[static_cast<size_t>(BiomeType::Desert)];
        biome.type = BiomeType::Desert;
        biome.name = "realcraft:desert";
        biome.display_name = "Desert";

        biome.blocks.surface_block = sand_id;
        biome.blocks.subsurface_block = sand_id;
        biome.blocks.underwater_block = sand_id;
        biome.blocks.stone_block = stone_id;

        biome.height.height_scale = 0.5f;
        biome.height.detail_scale = 0.5f;  // Smoother terrain

        // Hot and dry
        biome.climate = {0.7f, 1.0f, 0.0f, 0.2f};
    }

    // ========================================================================
    // Savanna
    // ========================================================================
    {
        BiomeConfig& biome = impl_->biomes[static_cast<size_t>(BiomeType::Savanna)];
        biome.type = BiomeType::Savanna;
        biome.name = "realcraft:savanna";
        biome.display_name = "Savanna";

        biome.blocks.surface_block = grass_id;
        biome.blocks.subsurface_block = dirt_id;
        biome.blocks.underwater_block = sand_id;
        biome.blocks.stone_block = stone_id;

        biome.height.height_scale = 0.8f;

        // Warm, low humidity
        biome.climate = {0.6f, 0.9f, 0.2f, 0.4f};
    }

    // ========================================================================
    // Plains
    // ========================================================================
    {
        BiomeConfig& biome = impl_->biomes[static_cast<size_t>(BiomeType::Plains)];
        biome.type = BiomeType::Plains;
        biome.name = "realcraft:plains";
        biome.display_name = "Plains";

        biome.blocks.surface_block = grass_id;
        biome.blocks.subsurface_block = dirt_id;
        biome.blocks.underwater_block = sand_id;
        biome.blocks.stone_block = stone_id;

        biome.height.height_scale = 1.0f;

        // Moderate temperature and humidity
        biome.climate = {0.3f, 0.7f, 0.3f, 0.6f};
    }

    // ========================================================================
    // Forest
    // ========================================================================
    {
        BiomeConfig& biome = impl_->biomes[static_cast<size_t>(BiomeType::Forest)];
        biome.type = BiomeType::Forest;
        biome.name = "realcraft:forest";
        biome.display_name = "Forest";

        biome.blocks.surface_block = grass_id;
        biome.blocks.subsurface_block = dirt_id;
        biome.blocks.underwater_block = gravel_id;
        biome.blocks.stone_block = stone_id;

        biome.height.height_scale = 1.0f;

        // Moderate temperature, higher humidity
        biome.climate = {0.3f, 0.7f, 0.5f, 0.8f};
    }

    // ========================================================================
    // Swamp
    // ========================================================================
    {
        BiomeConfig& biome = impl_->biomes[static_cast<size_t>(BiomeType::Swamp)];
        biome.type = BiomeType::Swamp;
        biome.name = "realcraft:swamp";
        biome.display_name = "Swamp";

        biome.blocks.surface_block = grass_id;
        biome.blocks.subsurface_block = dirt_id;
        biome.blocks.underwater_block = dirt_id;
        biome.blocks.stone_block = stone_id;

        biome.height.height_scale = 0.6f;
        biome.height.height_offset = -5.0f;  // Lower terrain

        // Warm, very wet
        biome.climate = {0.4f, 0.7f, 0.7f, 1.0f};
    }

    // ========================================================================
    // Taiga
    // ========================================================================
    {
        BiomeConfig& biome = impl_->biomes[static_cast<size_t>(BiomeType::Taiga)];
        biome.type = BiomeType::Taiga;
        biome.name = "realcraft:taiga";
        biome.display_name = "Taiga";
        biome.is_cold = true;

        biome.blocks.surface_block = grass_id;
        biome.blocks.subsurface_block = dirt_id;
        biome.blocks.underwater_block = gravel_id;
        biome.blocks.stone_block = stone_id;

        biome.height.height_scale = 1.0f;

        // Cold, moderate humidity
        biome.climate = {0.1f, 0.3f, 0.4f, 0.7f};
    }

    // ========================================================================
    // Tundra
    // ========================================================================
    {
        BiomeConfig& biome = impl_->biomes[static_cast<size_t>(BiomeType::Tundra)];
        biome.type = BiomeType::Tundra;
        biome.name = "realcraft:tundra";
        biome.display_name = "Tundra";
        biome.is_cold = true;

        biome.blocks.surface_block = grass_id;
        biome.blocks.subsurface_block = dirt_id;
        biome.blocks.underwater_block = gravel_id;
        biome.blocks.stone_block = stone_id;

        biome.height.height_scale = 0.7f;

        // Very cold
        biome.climate = {0.0f, 0.2f, 0.2f, 0.5f};
    }

    // ========================================================================
    // Mountains
    // ========================================================================
    {
        BiomeConfig& biome = impl_->biomes[static_cast<size_t>(BiomeType::Mountains)];
        biome.type = BiomeType::Mountains;
        biome.name = "realcraft:mountains";
        biome.display_name = "Mountains";
        biome.is_mountainous = true;

        biome.blocks.surface_block = stone_id;
        biome.blocks.subsurface_block = stone_id;
        biome.blocks.underwater_block = gravel_id;
        biome.blocks.stone_block = stone_id;

        biome.height.height_scale = 1.5f;
        biome.height.mountain_weight = 1.5f;

        // Ignores climate (height-based selection)
        biome.climate = {0.0f, 1.0f, 0.0f, 1.0f};
    }

    // ========================================================================
    // SnowyMountains
    // ========================================================================
    {
        BiomeConfig& biome = impl_->biomes[static_cast<size_t>(BiomeType::SnowyMountains)];
        biome.type = BiomeType::SnowyMountains;
        biome.name = "realcraft:snowy_mountains";
        biome.display_name = "Snowy Mountains";
        biome.is_mountainous = true;
        biome.is_cold = true;

        biome.blocks.surface_block = grass_id;  // Would be snow in future
        biome.blocks.subsurface_block = dirt_id;
        biome.blocks.underwater_block = gravel_id;
        biome.blocks.stone_block = stone_id;

        biome.height.height_scale = 1.5f;
        biome.height.mountain_weight = 1.5f;

        biome.climate = {0.0f, 0.3f, 0.0f, 1.0f};
    }

    // ========================================================================
    // River
    // ========================================================================
    {
        BiomeConfig& biome = impl_->biomes[static_cast<size_t>(BiomeType::River)];
        biome.type = BiomeType::River;
        biome.name = "realcraft:river";
        biome.display_name = "River";

        biome.blocks.surface_block = grass_id;
        biome.blocks.subsurface_block = dirt_id;
        biome.blocks.underwater_block = gravel_id;
        biome.blocks.stone_block = stone_id;

        biome.height.height_scale = 0.5f;
        biome.height.height_offset = -3.0f;

        biome.climate = {0.0f, 1.0f, 0.0f, 1.0f};
    }

    impl_->initialized = true;
}

const BiomeConfig& BiomeRegistry::get(BiomeType type) const {
    auto index = static_cast<size_t>(type);
    if (index >= BIOME_COUNT || !impl_->initialized) {
        return impl_->default_biome;
    }
    return impl_->biomes[index];
}

const BiomeBlockPalette& BiomeRegistry::get_palette(BiomeType type) const {
    return get(type).blocks;
}

const BiomeHeightModifiers& BiomeRegistry::get_height_modifiers(BiomeType type) const {
    return get(type).height;
}

const std::array<BiomeConfig, BIOME_COUNT>& BiomeRegistry::get_all() const {
    return impl_->biomes;
}

BiomeType BiomeRegistry::find_biome(float temperature, float humidity, float height) const {
    // Clamp inputs
    temperature = std::clamp(temperature, 0.0f, 1.0f);
    humidity = std::clamp(humidity, 0.0f, 1.0f);

    // Height-based biome selection (mountains override climate)
    constexpr float MOUNTAIN_THRESHOLD = 100.0f;
    if (height > MOUNTAIN_THRESHOLD) {
        if (temperature < 0.3f) {
            return BiomeType::SnowyMountains;
        }
        return BiomeType::Mountains;
    }

    // Ocean/beach for terrain below/at sea level
    constexpr float SEA_LEVEL = 64.0f;
    constexpr float BEACH_HEIGHT = 68.0f;
    if (height < SEA_LEVEL) {
        return BiomeType::Ocean;
    }
    if (height <= BEACH_HEIGHT) {
        return BiomeType::Beach;
    }

    // Climate-based selection for normal terrain
    // This uses a priority-ordered approach checking each biome's range
    // Order matters: more specific biomes (narrower ranges) should be checked first

    // Hot and dry -> Desert
    if (temperature >= 0.7f && humidity < 0.2f) {
        return BiomeType::Desert;
    }

    // Hot and somewhat dry -> Savanna
    if (temperature >= 0.6f && humidity >= 0.2f && humidity < 0.4f) {
        return BiomeType::Savanna;
    }

    // Warm and very wet -> Swamp
    if (temperature >= 0.4f && temperature < 0.7f && humidity >= 0.7f) {
        return BiomeType::Swamp;
    }

    // Cold and moderate humidity -> Taiga
    if (temperature < 0.3f && humidity >= 0.4f && humidity < 0.7f) {
        return BiomeType::Taiga;
    }

    // Very cold -> Tundra
    if (temperature < 0.2f) {
        return BiomeType::Tundra;
    }

    // Moderate temperature, higher humidity -> Forest
    if (temperature >= 0.3f && temperature < 0.7f && humidity >= 0.5f) {
        return BiomeType::Forest;
    }

    // Default: Plains (moderate everything)
    return BiomeType::Plains;
}

}  // namespace realcraft::world
