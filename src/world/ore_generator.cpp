// RealCraft World System
// ore_generator.cpp - Procedural ore vein generation with depth-based distribution

#include <algorithm>
#include <cmath>
#include <realcraft/world/block.hpp>
#include <realcraft/world/ore_generator.hpp>
#include <unordered_set>

namespace realcraft::world {

// ============================================================================
// Default Ore Configurations
// ============================================================================

std::vector<OreVeinConfig> OreConfig::get_default_ores() {
    const auto& registry = BlockRegistry::instance();

    std::vector<OreVeinConfig> ores;

    // Coal - Most common, spawns at all depths
    if (auto id = registry.find_id("realcraft:coal_ore")) {
        ores.push_back({.name = "coal",
                        .ore_block = *id,
                        .min_y = 0,
                        .max_y = 128,
                        .min_vein_size = 8,
                        .max_vein_size = 17,
                        .veins_per_chunk = 20,
                        .mountains_only = false});
    }

    // Iron - Common, mid to deep
    if (auto id = registry.find_id("realcraft:iron_ore")) {
        ores.push_back({.name = "iron",
                        .ore_block = *id,
                        .min_y = 0,
                        .max_y = 64,
                        .min_vein_size = 4,
                        .max_vein_size = 9,
                        .veins_per_chunk = 20,
                        .mountains_only = false});
    }

    // Copper - Mid-tier, wide range
    if (auto id = registry.find_id("realcraft:copper_ore")) {
        ores.push_back({.name = "copper",
                        .ore_block = *id,
                        .min_y = 0,
                        .max_y = 96,
                        .min_vein_size = 6,
                        .max_vein_size = 10,
                        .veins_per_chunk = 10,
                        .mountains_only = false});
    }

    // Gold - Rare, deep only
    if (auto id = registry.find_id("realcraft:gold_ore")) {
        ores.push_back({.name = "gold",
                        .ore_block = *id,
                        .min_y = 0,
                        .max_y = 32,
                        .min_vein_size = 4,
                        .max_vein_size = 9,
                        .veins_per_chunk = 2,
                        .mountains_only = false});
    }

    // Redstone - Deep, moderately common
    if (auto id = registry.find_id("realcraft:redstone_ore")) {
        ores.push_back({.name = "redstone",
                        .ore_block = *id,
                        .min_y = 0,
                        .max_y = 16,
                        .min_vein_size = 4,
                        .max_vein_size = 8,
                        .veins_per_chunk = 8,
                        .mountains_only = false});
    }

    // Lapis - Rare, mid-depth
    if (auto id = registry.find_id("realcraft:lapis_ore")) {
        ores.push_back({.name = "lapis",
                        .ore_block = *id,
                        .min_y = 0,
                        .max_y = 30,
                        .min_vein_size = 4,
                        .max_vein_size = 8,
                        .veins_per_chunk = 1,
                        .mountains_only = false});
    }

    // Emerald - Very rare, mountains only
    if (auto id = registry.find_id("realcraft:emerald_ore")) {
        ores.push_back({.name = "emerald",
                        .ore_block = *id,
                        .min_y = 0,
                        .max_y = 32,
                        .min_vein_size = 1,
                        .max_vein_size = 1,
                        .veins_per_chunk = 1,
                        .mountains_only = true});
    }

    // Diamond - Very rare, very deep
    if (auto id = registry.find_id("realcraft:diamond_ore")) {
        ores.push_back({.name = "diamond",
                        .ore_block = *id,
                        .min_y = 0,
                        .max_y = 16,
                        .min_vein_size = 4,
                        .max_vein_size = 8,
                        .veins_per_chunk = 1,
                        .mountains_only = false});
    }

    return ores;
}

// ============================================================================
// Implementation Details
// ============================================================================

struct OreGenerator::Impl {
    OreConfig config;
    std::vector<OreVeinConfig> ore_configs;

    // Hash function for deterministic pseudo-random decisions
    [[nodiscard]] static uint32_t hash_coords(int64_t x, int64_t y, int64_t z, uint32_t seed) {
        uint32_t h = seed;
        h ^= static_cast<uint32_t>(x) * 374761393U;
        h ^= static_cast<uint32_t>(y) * 668265263U;
        h ^= static_cast<uint32_t>(z) * 2654435761U;
        h = (h ^ (h >> 13)) * 1274126177U;
        return h ^ (h >> 16);
    }

    // Hash for chunk-level decisions
    [[nodiscard]] static uint32_t hash_chunk(int32_t cx, int32_t cz, uint32_t seed) {
        return hash_coords(cx, 0, cz, seed);
    }

    // Direction offsets for random walk
    static constexpr std::array<LocalBlockPos, 6> DIRECTIONS = {
        {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}};

    // Generate a single ore vein using random walk
    [[nodiscard]] std::vector<LocalBlockPos> generate_vein(int32_t start_x, int32_t start_y, int32_t start_z,
                                                           const OreVeinConfig& ore_cfg, uint32_t vein_seed) const {

        std::vector<LocalBlockPos> positions;

        // Determine vein size
        int32_t size_range = ore_cfg.max_vein_size - ore_cfg.min_vein_size + 1;
        int32_t vein_size = ore_cfg.min_vein_size;
        if (size_range > 0) {
            vein_size += static_cast<int32_t>(vein_seed % static_cast<uint32_t>(size_range));
        }

        // Use a set to avoid duplicates (simple hash)
        std::unordered_set<size_t> visited;
        auto pos_hash = [](int32_t x, int32_t y, int32_t z) -> size_t {
            return static_cast<size_t>(x) * 73856093ULL ^ static_cast<size_t>(y) * 19349663ULL ^
                   static_cast<size_t>(z) * 83492791ULL;
        };

        // Random walk from start position
        int32_t cx = start_x;
        int32_t cy = start_y;
        int32_t cz = start_z;

        for (int32_t i = 0; i < vein_size; ++i) {
            // Add current position if not visited
            size_t h = pos_hash(cx, cy, cz);
            if (visited.find(h) == visited.end()) {
                visited.insert(h);
                positions.push_back(LocalBlockPos(cx, cy, cz));
            }

            // Choose random direction for next step
            uint32_t dir_hash = hash_coords(cx, cy + i, cz, vein_seed + static_cast<uint32_t>(i));
            int dir_idx = static_cast<int>(dir_hash % 6);
            const auto& dir = DIRECTIONS[static_cast<size_t>(dir_idx)];

            cx += dir.x;
            cy += dir.y;
            cz += dir.z;
        }

        return positions;
    }

    // Check if biome is a mountain biome
    [[nodiscard]] static bool is_mountain_biome(BiomeType biome) {
        return biome == BiomeType::Mountains || biome == BiomeType::SnowyMountains;
    }
};

// ============================================================================
// OreGenerator Implementation
// ============================================================================

OreGenerator::OreGenerator(const OreConfig& config) : impl_(std::make_unique<Impl>()) {
    impl_->config = config;
    impl_->ore_configs = OreConfig::get_default_ores();
}

OreGenerator::~OreGenerator() = default;

OreGenerator::OreGenerator(OreGenerator&&) noexcept = default;
OreGenerator& OreGenerator::operator=(OreGenerator&&) noexcept = default;

std::vector<OrePosition> OreGenerator::generate_ores(const ChunkPos& chunk_pos, BiomeType biome) const {
    std::vector<OrePosition> ores;

    if (!impl_->config.enabled) {
        return ores;
    }

    // Pre-calculate chunk base coordinates
    const int64_t base_x = static_cast<int64_t>(chunk_pos.x) * CHUNK_SIZE_X;
    const int64_t base_z = static_cast<int64_t>(chunk_pos.y) * CHUNK_SIZE_Z;

    // Generate chunk hash for consistent randomness
    uint32_t chunk_hash = Impl::hash_chunk(chunk_pos.x, chunk_pos.y, impl_->config.seed);

    // Process each ore type
    for (size_t ore_idx = 0; ore_idx < impl_->ore_configs.size(); ++ore_idx) {
        const auto& ore_cfg = impl_->ore_configs[ore_idx];

        // Check biome restrictions
        if (ore_cfg.mountains_only && !Impl::is_mountain_biome(biome)) {
            continue;
        }

        // Generate veins for this ore type
        for (int32_t v = 0; v < ore_cfg.veins_per_chunk; ++v) {
            // Generate vein seed
            uint32_t vein_seed =
                Impl::hash_coords(static_cast<int64_t>(ore_idx), static_cast<int64_t>(v), 0, chunk_hash);

            // Random position within chunk and Y range
            int32_t y_range = ore_cfg.max_y - ore_cfg.min_y + 1;
            if (y_range <= 0)
                continue;

            int32_t local_x = static_cast<int32_t>(vein_seed % static_cast<uint32_t>(CHUNK_SIZE_X));
            int32_t local_z = static_cast<int32_t>((vein_seed >> 5) % static_cast<uint32_t>(CHUNK_SIZE_Z));
            int32_t local_y = ore_cfg.min_y + static_cast<int32_t>((vein_seed >> 10) % static_cast<uint32_t>(y_range));

            // Generate vein
            auto vein = impl_->generate_vein(local_x, local_y, local_z, ore_cfg, vein_seed);

            // Add valid positions to output
            for (const auto& offset : vein) {
                // Check bounds
                if (offset.x >= 0 && offset.x < CHUNK_SIZE_X && offset.z >= 0 && offset.z < CHUNK_SIZE_Z &&
                    offset.y >= ore_cfg.min_y && offset.y <= ore_cfg.max_y && offset.y >= 0 &&
                    offset.y < CHUNK_SIZE_Y) {
                    ores.push_back({offset, ore_cfg.ore_block});
                }
            }
        }
    }

    return ores;
}

BlockId OreGenerator::get_ore_at(int64_t world_x, int64_t world_y, int64_t world_z, BiomeType biome) const {
    if (!impl_->config.enabled) {
        return BLOCK_AIR;
    }

    // This is a slower per-block query - generate_ores is preferred for chunks
    // Check each ore type
    for (const auto& ore_cfg : impl_->ore_configs) {
        // Check Y range
        if (world_y < ore_cfg.min_y || world_y > ore_cfg.max_y) {
            continue;
        }

        // Check biome restriction
        if (ore_cfg.mountains_only && !Impl::is_mountain_biome(biome)) {
            continue;
        }

        // Use hash to determine if this position is an ore
        // This is probabilistic and won't match generate_ores exactly
        // For precise matching, use generate_ores instead
        uint32_t hash =
            Impl::hash_coords(world_x, world_y, world_z, impl_->config.seed + static_cast<uint32_t>(ore_cfg.ore_block));

        // Very low probability to match per-block check
        // The actual generation uses generate_ores which places veins
        float probability = static_cast<float>(ore_cfg.veins_per_chunk * ore_cfg.max_vein_size) /
                            static_cast<float>(CHUNK_SIZE_X * CHUNK_SIZE_Z * (ore_cfg.max_y - ore_cfg.min_y + 1));

        float roll = static_cast<float>(hash % 10000) / 10000.0f;
        if (roll < probability) {
            return ore_cfg.ore_block;
        }
    }

    return BLOCK_AIR;
}

const OreConfig& OreGenerator::get_config() const {
    return impl_->config;
}

void OreGenerator::set_config(const OreConfig& config) {
    impl_->config = config;
}

const std::vector<OreVeinConfig>& OreGenerator::get_ore_configs() const {
    return impl_->ore_configs;
}

void OreGenerator::set_ore_configs(std::vector<OreVeinConfig> ores) {
    impl_->ore_configs = std::move(ores);
}

void OreGenerator::rebuild() {
    // Reload default ores if needed
    if (impl_->ore_configs.empty()) {
        impl_->ore_configs = OreConfig::get_default_ores();
    }
}

}  // namespace realcraft::world
