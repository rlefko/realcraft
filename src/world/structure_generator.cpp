// RealCraft World System
// structure_generator.cpp - Natural formation generation (rock spires, boulders)

#include <algorithm>
#include <cmath>
#include <realcraft/world/block.hpp>
#include <realcraft/world/structure_generator.hpp>

namespace realcraft::world {

// ============================================================================
// Implementation Details
// ============================================================================

struct StructureGenerator::Impl {
    StructureConfig config;

    // Block IDs
    BlockId stone_id = BLOCK_AIR;
    BlockId cobblestone_id = BLOCK_AIR;
    BlockId mossy_stone_id = BLOCK_AIR;
    BlockId pillar_stone_id = BLOCK_AIR;

    void cache_block_ids() {
        const auto& registry = BlockRegistry::instance();

        stone_id = registry.stone_id();
        if (auto id = registry.find_id("realcraft:cobblestone"))
            cobblestone_id = *id;
        if (auto id = registry.find_id("realcraft:mossy_stone"))
            mossy_stone_id = *id;
        if (auto id = registry.find_id("realcraft:pillar_stone"))
            pillar_stone_id = *id;

        // Fallbacks
        if (cobblestone_id == BLOCK_AIR)
            cobblestone_id = stone_id;
        if (mossy_stone_id == BLOCK_AIR)
            mossy_stone_id = stone_id;
        if (pillar_stone_id == BLOCK_AIR)
            pillar_stone_id = stone_id;
    }

    // Hash function for deterministic pseudo-random decisions
    [[nodiscard]] static uint32_t hash_coords(int64_t x, int64_t y, int64_t z, uint32_t seed) {
        uint32_t h = seed;
        h ^= static_cast<uint32_t>(x) * 374761393U;
        h ^= static_cast<uint32_t>(y) * 668265263U;
        h ^= static_cast<uint32_t>(z) * 2654435761U;
        h = (h ^ (h >> 13)) * 1274126177U;
        return h ^ (h >> 16);
    }

    [[nodiscard]] static uint32_t hash_chunk(int32_t cx, int32_t cz, uint32_t seed) {
        return hash_coords(cx, 0, cz, seed);
    }

    [[nodiscard]] static float hash_to_float(uint32_t hash) { return static_cast<float>(hash % 10000) / 10000.0f; }

    // Check if biome supports structures
    [[nodiscard]] static bool biome_has_structures(BiomeType biome, StructureType type) {
        switch (biome) {
            case BiomeType::Mountains:
            case BiomeType::SnowyMountains:
                return true;  // All structure types
            case BiomeType::Plains:
            case BiomeType::Savanna:
                // Only boulders and outcrops
                return type != StructureType::RockSpire;
            case BiomeType::Desert:
                // Only rock spires
                return type == StructureType::RockSpire;
            default:
                return false;
        }
    }

    // Generate rock spire
    [[nodiscard]] std::vector<StructureBlock> generate_rock_spire(int32_t surface_height,
                                                                  uint32_t structure_seed) const {
        std::vector<StructureBlock> blocks;

        // Determine height and radius
        int32_t height_range = config.rock_spire.max_height - config.rock_spire.min_height + 1;
        int32_t height = config.rock_spire.min_height;
        if (height_range > 0) {
            height += static_cast<int32_t>(structure_seed % static_cast<uint32_t>(height_range));
        }

        int32_t base_radius = config.rock_spire.min_radius;
        int32_t radius_range = config.rock_spire.max_radius - config.rock_spire.min_radius + 1;
        if (radius_range > 0) {
            base_radius += static_cast<int32_t>((structure_seed >> 8) % static_cast<uint32_t>(radius_range));
        }

        // Generate tapered spire
        for (int32_t y = 0; y <= height; ++y) {
            // Radius tapers from base to top
            float taper = 1.0f - static_cast<float>(y) / static_cast<float>(height);
            int32_t radius = std::max(1, static_cast<int32_t>(static_cast<float>(base_radius) * taper));

            for (int32_t dx = -radius; dx <= radius; ++dx) {
                for (int32_t dz = -radius; dz <= radius; ++dz) {
                    // Roughly circular cross-section
                    if (dx * dx + dz * dz <= radius * radius + 1) {
                        // Select block type based on position
                        BlockId block = pillar_stone_id;
                        uint32_t block_hash = hash_coords(dx, y, dz, structure_seed + 1000);
                        float block_roll = hash_to_float(block_hash);

                        if (y < 2 && block_roll < 0.3f) {
                            block = mossy_stone_id;  // Moss at base
                        } else if (block_roll < 0.2f) {
                            block = cobblestone_id;  // Some variation
                        }

                        blocks.push_back({LocalBlockPos(dx, y + 1, dz), block});
                    }
                }
            }
        }

        return blocks;
    }

    // Generate boulder
    [[nodiscard]] std::vector<StructureBlock> generate_boulder(int32_t surface_height, uint32_t structure_seed) const {
        std::vector<StructureBlock> blocks;

        // Determine size
        int32_t size_range = config.boulder.max_size - config.boulder.min_size + 1;
        int32_t size = config.boulder.min_size;
        if (size_range > 0) {
            size += static_cast<int32_t>(structure_seed % static_cast<uint32_t>(size_range));
        }

        int32_t radius = size / 2 + 1;
        int32_t height = size / 2;

        // Generate roughly spherical boulder
        for (int32_t y = 0; y <= height; ++y) {
            // Radius varies with height (ellipsoid)
            float y_factor = 1.0f - std::pow(static_cast<float>(y) / static_cast<float>(height) - 0.5f, 2.0f) * 4.0f;
            int32_t layer_radius = std::max(1, static_cast<int32_t>(static_cast<float>(radius) * std::sqrt(y_factor)));

            for (int32_t dx = -layer_radius; dx <= layer_radius; ++dx) {
                for (int32_t dz = -layer_radius; dz <= layer_radius; ++dz) {
                    if (dx * dx + dz * dz <= layer_radius * layer_radius) {
                        // Add some noise to surface
                        uint32_t noise_hash = hash_coords(dx, y, dz, structure_seed + 2000);
                        if (dx * dx + dz * dz < layer_radius * layer_radius - 1 || hash_to_float(noise_hash) < 0.7f) {

                            BlockId block = cobblestone_id;
                            float block_roll = hash_to_float(noise_hash);

                            if (y == 0 && block_roll < 0.4f) {
                                block = mossy_stone_id;
                            } else if (block_roll < 0.3f) {
                                block = stone_id;
                            }

                            blocks.push_back({LocalBlockPos(dx, y + 1, dz), block});
                        }
                    }
                }
            }
        }

        return blocks;
    }

    // Generate stone outcrop
    [[nodiscard]] std::vector<StructureBlock> generate_outcrop(int32_t surface_height, uint32_t structure_seed) const {
        std::vector<StructureBlock> blocks;

        // Determine size
        int32_t size_range = config.outcrop.max_size - config.outcrop.min_size + 1;
        int32_t size = config.outcrop.min_size;
        if (size_range > 0) {
            size += static_cast<int32_t>(structure_seed % static_cast<uint32_t>(size_range));
        }

        // Outcrop is wider than tall
        int32_t width = size;
        int32_t height = size / 2 + 1;

        // Generate irregular rocky shape
        for (int32_t y = 0; y < height; ++y) {
            int32_t layer_width = width - y;

            for (int32_t dx = -layer_width; dx <= layer_width; ++dx) {
                for (int32_t dz = -layer_width; dz <= layer_width; ++dz) {
                    // Irregular shape
                    uint32_t shape_hash = hash_coords(dx, y, dz, structure_seed + 3000);
                    float threshold = 0.4f + static_cast<float>(y) * 0.1f;

                    int32_t dist_sq = dx * dx + dz * dz;
                    if (dist_sq <= layer_width * layer_width &&
                        (dist_sq < layer_width * layer_width / 2 || hash_to_float(shape_hash) < threshold)) {

                        BlockId block = stone_id;
                        float block_roll = hash_to_float(shape_hash);

                        if (y < 2 && block_roll < 0.3f) {
                            block = mossy_stone_id;
                        } else if (block_roll < 0.25f) {
                            block = cobblestone_id;
                        }

                        blocks.push_back({LocalBlockPos(dx, y + 1, dz), block});
                    }
                }
            }
        }

        return blocks;
    }
};

// ============================================================================
// StructureGenerator Implementation
// ============================================================================

StructureGenerator::StructureGenerator(const StructureConfig& config) : impl_(std::make_unique<Impl>()) {
    impl_->config = config;
    impl_->cache_block_ids();
}

StructureGenerator::~StructureGenerator() = default;

StructureGenerator::StructureGenerator(StructureGenerator&&) noexcept = default;
StructureGenerator& StructureGenerator::operator=(StructureGenerator&&) noexcept = default;

bool StructureGenerator::should_place_structure(const ChunkPos& chunk_pos, StructureType type, BiomeType biome) const {
    if (!impl_->config.enabled) {
        return false;
    }

    if (!Impl::biome_has_structures(biome, type)) {
        return false;
    }

    // Get chance based on structure type
    float chance = 0.0f;
    bool type_enabled = true;

    switch (type) {
        case StructureType::RockSpire:
            chance = impl_->config.rock_spire.chance_per_chunk;
            type_enabled = impl_->config.rock_spire.enabled;
            break;
        case StructureType::Boulder:
            chance = impl_->config.boulder.chance_per_chunk;
            type_enabled = impl_->config.boulder.enabled;
            break;
        case StructureType::StoneOutcrop:
            chance = impl_->config.outcrop.chance_per_chunk;
            type_enabled = impl_->config.outcrop.enabled;
            break;
        default:
            return false;
    }

    if (!type_enabled) {
        return false;
    }

    uint32_t hash = Impl::hash_chunk(chunk_pos.x, chunk_pos.y, impl_->config.seed + static_cast<uint32_t>(type) * 1000);
    float roll = Impl::hash_to_float(hash);

    return roll < chance;
}

std::optional<LocalBlockPos> StructureGenerator::get_structure_position(const ChunkPos& chunk_pos,
                                                                        StructureType type) const {

    uint32_t hash =
        Impl::hash_chunk(chunk_pos.x, chunk_pos.y, impl_->config.seed + static_cast<uint32_t>(type) * 1000 + 500);

    // Position within chunk (leave margin from edges)
    constexpr int32_t margin = 4;
    constexpr int32_t range_x = CHUNK_SIZE_X - 2 * margin;
    constexpr int32_t range_z = CHUNK_SIZE_Z - 2 * margin;
    static_assert(range_x > 0 && range_z > 0, "Chunk size too small for structure margin");

    int32_t x = margin + static_cast<int32_t>(hash % static_cast<uint32_t>(range_x));
    int32_t z = margin + static_cast<int32_t>((hash >> 10) % static_cast<uint32_t>(range_z));

    return LocalBlockPos(x, 0, z);  // Y will be determined by surface
}

std::vector<StructureBlock> StructureGenerator::generate_structure(const ChunkPos& chunk_pos, StructureType type,
                                                                   int32_t surface_height) const {

    uint32_t structure_seed =
        Impl::hash_chunk(chunk_pos.x, chunk_pos.y, impl_->config.seed + static_cast<uint32_t>(type) * 2000);

    switch (type) {
        case StructureType::RockSpire:
            return impl_->generate_rock_spire(surface_height, structure_seed);
        case StructureType::Boulder:
            return impl_->generate_boulder(surface_height, structure_seed);
        case StructureType::StoneOutcrop:
            return impl_->generate_outcrop(surface_height, structure_seed);
        default:
            return {};
    }
}

const StructureConfig& StructureGenerator::get_config() const {
    return impl_->config;
}

void StructureGenerator::set_config(const StructureConfig& config) {
    impl_->config = config;
}

void StructureGenerator::rebuild() {
    impl_->cache_block_ids();
}

}  // namespace realcraft::world
