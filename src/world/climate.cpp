// RealCraft World System
// climate.cpp - Climate model for biome distribution

#include <FastNoise/FastNoise.h>
#include <algorithm>
#include <cmath>
#include <realcraft/world/climate.hpp>
#include <unordered_map>
#include <utility>

namespace realcraft::world {

// ============================================================================
// Implementation Details
// ============================================================================

struct ClimateMap::Impl {
    ClimateConfig config;

    // FastNoise2 nodes - thread-safe for GenSingle calls
    FastNoise::SmartNode<FastNoise::Generator> temperature_node;
    FastNoise::SmartNode<FastNoise::Generator> humidity_node;

    void build_nodes() {
        // Temperature noise (large-scale climate zones)
        auto temp_simplex = FastNoise::New<FastNoise::Simplex>();
        auto temp_fractal = FastNoise::New<FastNoise::FractalFBm>();
        temp_fractal->SetSource(temp_simplex);
        temp_fractal->SetOctaveCount(config.temperature.octaves);
        temp_fractal->SetGain(config.temperature.gain);
        temp_fractal->SetLacunarity(config.temperature.lacunarity);
        temperature_node = temp_fractal;

        // Humidity noise (rainfall patterns)
        auto humid_simplex = FastNoise::New<FastNoise::Simplex>();
        auto humid_fractal = FastNoise::New<FastNoise::FractalFBm>();
        humid_fractal->SetSource(humid_simplex);
        humid_fractal->SetOctaveCount(config.humidity.octaves);
        humid_fractal->SetGain(config.humidity.gain);
        humid_fractal->SetLacunarity(config.humidity.lacunarity);
        humidity_node = humid_fractal;
    }

    [[nodiscard]] float sample_temperature(int64_t x, int64_t z) const {
        if (!temperature_node) {
            return 0.5f;
        }

        float raw = temperature_node->GenSingle2D(static_cast<float>(x) * config.temperature.scale,
                                                   static_cast<float>(z) * config.temperature.scale,
                                                   static_cast<int>(config.seed));

        // Map from [-1, 1] to [0, 1]
        return (raw + 1.0f) * 0.5f;
    }

    [[nodiscard]] float sample_humidity(int64_t x, int64_t z) const {
        if (!humidity_node) {
            return 0.5f;
        }

        // Use different seed offset for humidity to get uncorrelated noise
        float raw = humidity_node->GenSingle2D(static_cast<float>(x) * config.humidity.scale,
                                                static_cast<float>(z) * config.humidity.scale,
                                                static_cast<int>(config.seed + 50000));

        // Map from [-1, 1] to [0, 1]
        return (raw + 1.0f) * 0.5f;
    }

    [[nodiscard]] float apply_altitude_to_temperature(float base_temp, float height) const {
        // Temperature decreases with altitude (above sea level)
        float altitude_above_sea = std::max(0.0f, height - config.sea_level);
        float temp = base_temp - altitude_above_sea * config.altitude_temperature_factor;
        return std::clamp(temp, 0.0f, 1.0f);
    }

    [[nodiscard]] ClimateSample compute_sample(int64_t x, int64_t z, float height) const {
        ClimateSample sample;

        // Sample raw climate values
        float raw_temp = sample_temperature(x, z);
        sample.humidity = std::clamp(sample_humidity(x, z), 0.0f, 1.0f);

        // Apply altitude adjustment to temperature
        sample.temperature = apply_altitude_to_temperature(raw_temp, height);

        // Determine biome from climate
        sample.biome = BiomeRegistry::instance().find_biome(sample.temperature, sample.humidity, height);

        return sample;
    }
};

// ============================================================================
// ClimateMap Implementation
// ============================================================================

ClimateMap::ClimateMap(const ClimateConfig& config) : impl_(std::make_unique<Impl>()) {
    impl_->config = config;
    impl_->build_nodes();
}

ClimateMap::~ClimateMap() = default;

ClimateMap::ClimateMap(ClimateMap&&) noexcept = default;
ClimateMap& ClimateMap::operator=(ClimateMap&&) noexcept = default;

ClimateSample ClimateMap::sample(int64_t world_x, int64_t world_z, float height) const {
    return impl_->compute_sample(world_x, world_z, height);
}

BiomeType ClimateMap::get_biome(int64_t world_x, int64_t world_z, float height) const {
    return sample(world_x, world_z, height).biome;
}

float ClimateMap::get_raw_temperature(int64_t world_x, int64_t world_z) const {
    return impl_->sample_temperature(world_x, world_z);
}

float ClimateMap::get_raw_humidity(int64_t world_x, int64_t world_z) const {
    return impl_->sample_humidity(world_x, world_z);
}

BiomeBlend ClimateMap::sample_blended(int64_t world_x, int64_t world_z, float height) const {
    BiomeBlend blend;

    // Get the primary biome at this location
    ClimateSample center = sample(world_x, world_z, height);
    blend.primary_biome = center.biome;
    blend.secondary_biome = center.biome;
    blend.blend_factor = 0.0f;

    // Sample surrounding area to detect biome transitions
    int radius = impl_->config.blend_radius;
    if (radius <= 0) {
        // No blending, just return the primary biome
        blend.blended_height = BiomeRegistry::instance().get_height_modifiers(blend.primary_biome);
        return blend;
    }

    // Count biome occurrences in the blend radius
    std::unordered_map<uint8_t, int> biome_counts;

    // Sample in a circular pattern around the center
    for (int dz = -radius; dz <= radius; ++dz) {
        for (int dx = -radius; dx <= radius; ++dx) {
            // Skip corners for more circular sampling
            if (dx * dx + dz * dz > radius * radius) {
                continue;
            }

            ClimateSample s = sample(world_x + dx, world_z + dz, height);
            biome_counts[static_cast<uint8_t>(s.biome)]++;
        }
    }

    // Find the primary and secondary biomes
    uint8_t primary_id = static_cast<uint8_t>(center.biome);
    uint8_t secondary_id = primary_id;
    int primary_count = 0;
    int secondary_count = 0;

    for (const auto& [biome_id, count] : biome_counts) {
        if (count > primary_count) {
            // Current primary becomes secondary
            secondary_id = primary_id;
            secondary_count = primary_count;
            // New primary
            primary_id = biome_id;
            primary_count = count;
        } else if (count > secondary_count && biome_id != primary_id) {
            secondary_id = biome_id;
            secondary_count = count;
        }
    }

    blend.primary_biome = static_cast<BiomeType>(primary_id);
    blend.secondary_biome = static_cast<BiomeType>(secondary_id);

    // Calculate blend factor based on ratio of secondary to primary
    if (primary_count > 0 && secondary_count > 0 && primary_id != secondary_id) {
        // Blend factor is how much secondary biome influences this point
        blend.blend_factor = static_cast<float>(secondary_count) / static_cast<float>(primary_count + secondary_count);
    } else {
        blend.blend_factor = 0.0f;
    }

    // Compute blended height modifiers
    const auto& primary_height = BiomeRegistry::instance().get_height_modifiers(blend.primary_biome);
    const auto& secondary_height = BiomeRegistry::instance().get_height_modifiers(blend.secondary_biome);
    blend.blended_height = blend_height_modifiers(primary_height, secondary_height, blend.blend_factor);

    return blend;
}

const ClimateConfig& ClimateMap::get_config() const {
    return impl_->config;
}

void ClimateMap::set_config(const ClimateConfig& config) {
    impl_->config = config;
}

void ClimateMap::rebuild_nodes() {
    impl_->build_nodes();
}

}  // namespace realcraft::world
