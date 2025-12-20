// RealCraft World System
// cave_generator.cpp - Procedural cave generation using Perlin worms and cellular noise

#include <FastNoise/FastNoise.h>
#include <algorithm>
#include <cmath>
#include <realcraft/world/cave_generator.hpp>

namespace realcraft::world {

// ============================================================================
// Implementation Details
// ============================================================================

struct CaveGenerator::Impl {
    CaveConfig config;

    // FastNoise2 nodes for Perlin worms (two noise channels)
    FastNoise::SmartNode<FastNoise::Generator> worm_noise_a;
    FastNoise::SmartNode<FastNoise::Generator> worm_noise_b;

    // FastNoise2 node for chamber generation (cellular noise)
    FastNoise::SmartNode<FastNoise::Generator> chamber_node;

    // Noise for decoration randomization
    FastNoise::SmartNode<FastNoise::Generator> decoration_noise;

    // Hash function for deterministic pseudo-random decisions
    [[nodiscard]] static uint32_t hash_coords(int64_t x, int64_t y, int64_t z, uint32_t seed) {
        // Simple but effective hash combining coordinates
        uint32_t h = seed;
        h ^= static_cast<uint32_t>(x) * 374761393U;
        h ^= static_cast<uint32_t>(y) * 668265263U;
        h ^= static_cast<uint32_t>(z) * 2654435761U;
        h = (h ^ (h >> 13)) * 1274126177U;
        return h ^ (h >> 16);
    }

    // Hash for larger regions (used for flood determination)
    [[nodiscard]] static uint32_t hash_region(int64_t x, int64_t y, int64_t z, uint32_t seed) {
        // Hash at region granularity (16x16x16 blocks)
        return hash_coords(x >> 4, y >> 4, z >> 4, seed);
    }

    void build_nodes() {
        // Perlin worm noise A (first coordinate in 2D worm space)
        {
            auto simplex = FastNoise::New<FastNoise::Simplex>();
            auto fractal = FastNoise::New<FastNoise::FractalFBm>();
            fractal->SetSource(simplex);
            fractal->SetOctaveCount(config.worm.octaves);
            fractal->SetGain(0.5f);
            fractal->SetLacunarity(2.0f);
            worm_noise_a = fractal;
        }

        // Perlin worm noise B (second coordinate in 2D worm space)
        {
            auto simplex = FastNoise::New<FastNoise::Simplex>();
            auto fractal = FastNoise::New<FastNoise::FractalFBm>();
            fractal->SetSource(simplex);
            fractal->SetOctaveCount(config.worm.octaves);
            fractal->SetGain(0.5f);
            fractal->SetLacunarity(2.0f);
            worm_noise_b = fractal;
        }

        // Chamber noise (cellular/Worley noise for organic caverns)
        if (config.chamber.enabled) {
            auto cellular = FastNoise::New<FastNoise::CellularDistance>();
            cellular->SetDistanceFunction(FastNoise::DistanceFunction::Euclidean);
            chamber_node = cellular;
        }

        // Decoration noise (for randomizing decoration placement)
        {
            auto simplex = FastNoise::New<FastNoise::Simplex>();
            decoration_noise = simplex;
        }
    }

    [[nodiscard]] float compute_worm_carve(int64_t world_x, int64_t world_y, int64_t world_z) const {
        // Sample the two worm noise channels
        float scale = config.worm.scale;
        float v_squeeze = config.worm.vertical_squeeze;

        float x_scaled = static_cast<float>(world_x) * scale;
        float y_scaled = static_cast<float>(world_y) * scale * v_squeeze;
        float z_scaled = static_cast<float>(world_z) * scale;

        // Sample two noise values that define the "worm position" at this point
        float noise_a = worm_noise_a->GenSingle3D(x_scaled, y_scaled, z_scaled, static_cast<int>(config.seed));
        float noise_b = worm_noise_b->GenSingle3D(x_scaled, y_scaled, z_scaled, static_cast<int>(config.seed + 1000));

        // Distance squared from worm center (0,0) in noise space
        // Lower distance = closer to worm center = should be carved
        return noise_a * noise_a + noise_b * noise_b;
    }

    [[nodiscard]] bool check_chamber(int64_t world_x, int64_t world_y, int64_t world_z) const {
        if (!config.chamber.enabled || !chamber_node) {
            return false;
        }

        // Chambers only in certain Y range
        if (world_y < config.chamber.min_y || world_y > config.chamber.max_y) {
            return false;
        }

        float scale = config.chamber.scale;

        // Sample cellular noise (returns distance to nearest cell center)
        float cell_dist =
            chamber_node->GenSingle3D(static_cast<float>(world_x) * scale,
                                      static_cast<float>(world_y) * scale * 0.7f,  // Squash vertically
                                      static_cast<float>(world_z) * scale, static_cast<int>(config.seed + 5000));

        // Normalize from [-1, 1] to [0, 1]
        float normalized = (cell_dist + 1.0f) * 0.5f;

        // Carve if close enough to cell center
        return normalized < config.chamber.threshold;
    }

    [[nodiscard]] bool check_flooded(int64_t world_x, int64_t world_y, int64_t world_z) const {
        if (!config.water.enabled) {
            return false;
        }

        // Only flood below water table
        if (world_y > config.water.level) {
            return false;
        }

        // Deterministic flood check per cave region
        uint32_t hash = hash_region(world_x, world_y, world_z, config.seed + 9999);
        float roll = static_cast<float>(hash % 10000) / 10000.0f;

        return roll < config.water.flood_chance;
    }

    [[nodiscard]] float get_decoration_noise(int64_t x, int64_t y, int64_t z) const {
        if (!decoration_noise) {
            return 0.0f;
        }

        return decoration_noise->GenSingle3D(static_cast<float>(x) * 0.5f, static_cast<float>(y) * 0.5f,
                                             static_cast<float>(z) * 0.5f, static_cast<int>(config.seed + 7777));
    }
};

// ============================================================================
// CaveGenerator Implementation
// ============================================================================

CaveGenerator::CaveGenerator(const CaveConfig& config) : impl_(std::make_unique<Impl>()) {
    impl_->config = config;
    impl_->build_nodes();
}

CaveGenerator::~CaveGenerator() = default;

CaveGenerator::CaveGenerator(CaveGenerator&&) noexcept = default;
CaveGenerator& CaveGenerator::operator=(CaveGenerator&&) noexcept = default;

bool CaveGenerator::should_carve(int64_t world_x, int64_t world_y, int64_t world_z, int32_t surface_height) const {
    if (!impl_->config.enabled) {
        return false;
    }

    // Vertical bounds check
    if (world_y < impl_->config.min_y || world_y > impl_->config.max_y) {
        return false;
    }

    // Don't carve too close to surface (prevents surface holes)
    if (world_y > surface_height - impl_->config.surface_margin) {
        return false;
    }

    // Check Perlin worm carving
    float worm_dist = impl_->compute_worm_carve(world_x, world_y, world_z);

    // Worm threshold: lower distance = closer to worm = carve
    // The threshold squared gives us the carving radius in noise space
    float worm_threshold = impl_->config.worm.threshold;
    float worm_threshold_sq = worm_threshold * worm_threshold;

    if (worm_dist < worm_threshold_sq) {
        return true;
    }

    // Check chamber carving
    if (impl_->check_chamber(world_x, world_y, world_z)) {
        return true;
    }

    return false;
}

bool CaveGenerator::is_chamber(int64_t world_x, int64_t world_y, int64_t world_z) const {
    return impl_->check_chamber(world_x, world_y, world_z);
}

bool CaveGenerator::is_flooded(int64_t world_x, int64_t world_y, int64_t world_z) const {
    return impl_->check_flooded(world_x, world_y, world_z);
}

bool CaveGenerator::should_place_stalactite(int64_t world_x, int64_t world_y, int64_t world_z) const {
    if (!impl_->config.decorations.enabled) {
        return false;
    }

    // Use hash for deterministic decision
    uint32_t hash = Impl::hash_coords(world_x, world_y, world_z, impl_->config.seed + 1111);
    float roll = static_cast<float>(hash % 10000) / 10000.0f;

    return roll < impl_->config.decorations.stalactite_chance;
}

bool CaveGenerator::should_place_stalagmite(int64_t world_x, int64_t world_y, int64_t world_z) const {
    if (!impl_->config.decorations.enabled) {
        return false;
    }

    // Use hash for deterministic decision
    uint32_t hash = Impl::hash_coords(world_x, world_y, world_z, impl_->config.seed + 2222);
    float roll = static_cast<float>(hash % 10000) / 10000.0f;

    return roll < impl_->config.decorations.stalagmite_chance;
}

bool CaveGenerator::should_place_crystal(int64_t world_x, int64_t world_y, int64_t world_z) const {
    if (!impl_->config.decorations.enabled) {
        return false;
    }

    // Crystals only in deeper caves
    if (world_y > 40) {
        return false;
    }

    // Use hash for deterministic decision
    uint32_t hash = Impl::hash_coords(world_x, world_y, world_z, impl_->config.seed + 3333);
    float roll = static_cast<float>(hash % 10000) / 10000.0f;

    return roll < impl_->config.decorations.crystal_chance;
}

bool CaveGenerator::should_place_moss(int64_t world_x, int64_t world_y, int64_t world_z) const {
    if (!impl_->config.decorations.enabled) {
        return false;
    }

    // Use hash for deterministic decision
    uint32_t hash = Impl::hash_coords(world_x, world_y, world_z, impl_->config.seed + 4444);
    float roll = static_cast<float>(hash % 10000) / 10000.0f;

    return roll < impl_->config.decorations.moss_chance;
}

bool CaveGenerator::should_place_mushroom(int64_t world_x, int64_t world_y, int64_t world_z) const {
    if (!impl_->config.decorations.enabled) {
        return false;
    }

    // Use hash for deterministic decision
    uint32_t hash = Impl::hash_coords(world_x, world_y, world_z, impl_->config.seed + 5555);
    float roll = static_cast<float>(hash % 10000) / 10000.0f;

    return roll < impl_->config.decorations.mushroom_chance;
}

bool CaveGenerator::is_damp_area(int64_t world_x, int64_t world_y, int64_t world_z) const {
    if (!impl_->config.water.enabled) {
        return false;
    }

    // Consider areas near water table as damp
    int32_t damp_radius = impl_->config.decorations.damp_radius;
    int32_t water_level = impl_->config.water.level;

    // Damp if close to water table level
    if (std::abs(static_cast<int32_t>(world_y) - water_level) <= damp_radius) {
        return true;
    }

    // Also damp in flooded areas
    if (world_y <= water_level && is_flooded(world_x, world_y + damp_radius, world_z)) {
        return true;
    }

    return false;
}

const CaveConfig& CaveGenerator::get_config() const {
    return impl_->config;
}

void CaveGenerator::set_config(const CaveConfig& config) {
    impl_->config = config;
}

void CaveGenerator::rebuild_nodes() {
    impl_->build_nodes();
}

}  // namespace realcraft::world
