// RealCraft World System
// vegetation_generator.cpp - Ground cover generation (grass, flowers, bushes)

#include <cmath>
#include <realcraft/world/block.hpp>
#include <realcraft/world/vegetation_generator.hpp>

namespace realcraft::world {

// ============================================================================
// Implementation Details
// ============================================================================

struct VegetationGenerator::Impl {
    VegetationConfig config;

    // Block IDs (cached for performance)
    BlockId tall_grass_id = BLOCK_AIR;
    BlockId fern_id = BLOCK_AIR;
    BlockId dead_bush_id = BLOCK_AIR;
    BlockId cactus_id = BLOCK_AIR;
    BlockId bush_id = BLOCK_AIR;
    BlockId poppy_id = BLOCK_AIR;
    BlockId dandelion_id = BLOCK_AIR;
    BlockId blue_orchid_id = BLOCK_AIR;
    BlockId azure_bluet_id = BLOCK_AIR;
    BlockId tulip_red_id = BLOCK_AIR;
    BlockId cornflower_id = BLOCK_AIR;
    BlockId lily_pad_id = BLOCK_AIR;
    BlockId sunflower_id = BLOCK_AIR;

    void cache_block_ids() {
        const auto& registry = BlockRegistry::instance();

        if (auto id = registry.find_id("realcraft:tall_grass"))
            tall_grass_id = *id;
        if (auto id = registry.find_id("realcraft:fern"))
            fern_id = *id;
        if (auto id = registry.find_id("realcraft:dead_bush"))
            dead_bush_id = *id;
        if (auto id = registry.find_id("realcraft:cactus"))
            cactus_id = *id;
        if (auto id = registry.find_id("realcraft:bush"))
            bush_id = *id;
        if (auto id = registry.find_id("realcraft:poppy"))
            poppy_id = *id;
        if (auto id = registry.find_id("realcraft:dandelion"))
            dandelion_id = *id;
        if (auto id = registry.find_id("realcraft:blue_orchid"))
            blue_orchid_id = *id;
        if (auto id = registry.find_id("realcraft:azure_bluet"))
            azure_bluet_id = *id;
        if (auto id = registry.find_id("realcraft:tulip_red"))
            tulip_red_id = *id;
        if (auto id = registry.find_id("realcraft:cornflower"))
            cornflower_id = *id;
        if (auto id = registry.find_id("realcraft:lily_pad"))
            lily_pad_id = *id;
        if (auto id = registry.find_id("realcraft:sunflower"))
            sunflower_id = *id;
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

    // Convert hash to probability [0, 1)
    [[nodiscard]] static float hash_to_float(uint32_t hash) { return static_cast<float>(hash % 10000) / 10000.0f; }

    // Check if biome supports grass vegetation
    [[nodiscard]] static bool biome_has_grass(BiomeType biome) {
        switch (biome) {
            case BiomeType::Plains:
            case BiomeType::Forest:
            case BiomeType::Swamp:
            case BiomeType::Taiga:
            case BiomeType::Savanna:
            case BiomeType::Mountains:
            case BiomeType::River:
                return true;
            case BiomeType::Tundra:
            case BiomeType::SnowyMountains:
                return false;  // Snow cover, sparse grass
            case BiomeType::Desert:
            case BiomeType::Beach:
            case BiomeType::Ocean:
            default:
                return false;
        }
    }

    // Check if biome supports flowers
    [[nodiscard]] static bool biome_has_flowers(BiomeType biome) {
        switch (biome) {
            case BiomeType::Plains:
            case BiomeType::Forest:
            case BiomeType::Swamp:
                return true;
            default:
                return false;
        }
    }

    // Check if biome is desert
    [[nodiscard]] static bool is_desert(BiomeType biome) { return biome == BiomeType::Desert; }

    // Check if biome is savanna or desert (dead bush biomes)
    [[nodiscard]] static bool has_dead_bushes(BiomeType biome) {
        return biome == BiomeType::Desert || biome == BiomeType::Savanna;
    }

    // Check if biome is swamp
    [[nodiscard]] static bool is_swamp(BiomeType biome) { return biome == BiomeType::Swamp; }

    // Get density multiplier for biome
    [[nodiscard]] static float get_grass_density_multiplier(BiomeType biome) {
        switch (biome) {
            case BiomeType::Plains:
                return 1.2f;
            case BiomeType::Forest:
                return 0.8f;  // Trees shade out grass
            case BiomeType::Savanna:
                return 1.5f;  // Tall grassland
            case BiomeType::Taiga:
                return 0.5f;  // Sparse understory
            case BiomeType::Swamp:
                return 0.6f;
            case BiomeType::Mountains:
                return 0.4f;  // Sparse at altitude
            default:
                return 1.0f;
        }
    }
};

// ============================================================================
// VegetationGenerator Implementation
// ============================================================================

VegetationGenerator::VegetationGenerator(const VegetationConfig& config) : impl_(std::make_unique<Impl>()) {
    impl_->config = config;
    impl_->cache_block_ids();
}

VegetationGenerator::~VegetationGenerator() = default;

VegetationGenerator::VegetationGenerator(VegetationGenerator&&) noexcept = default;
VegetationGenerator& VegetationGenerator::operator=(VegetationGenerator&&) noexcept = default;

bool VegetationGenerator::should_place_grass(int64_t world_x, int64_t world_z, BiomeType biome) const {
    if (!impl_->config.enabled || !impl_->config.grass.enabled) {
        return false;
    }

    if (!Impl::biome_has_grass(biome)) {
        return false;
    }

    uint32_t hash = Impl::hash_coords(world_x, 0, world_z, impl_->config.seed + 1000);
    float roll = Impl::hash_to_float(hash);
    float density = impl_->config.grass.density * Impl::get_grass_density_multiplier(biome);

    return roll < density;
}

BlockId VegetationGenerator::get_grass_block(int64_t world_x, int64_t world_z, BiomeType biome) const {
    uint32_t hash = Impl::hash_coords(world_x, 1, world_z, impl_->config.seed + 1001);
    float roll = Impl::hash_to_float(hash);

    // Taiga and Forest prefer ferns
    if (biome == BiomeType::Taiga || biome == BiomeType::Forest) {
        if (roll < 0.4f) {
            return impl_->fern_id;
        }
    }

    // Default to tall grass
    if (roll < impl_->config.grass.tall_grass_ratio) {
        return impl_->tall_grass_id;
    }

    return impl_->fern_id;
}

BlockId VegetationGenerator::get_flower_block(int64_t world_x, int64_t world_z, BiomeType biome) const {
    if (!impl_->config.enabled || !impl_->config.flowers.enabled) {
        return BLOCK_AIR;
    }

    if (!Impl::biome_has_flowers(biome)) {
        return BLOCK_AIR;
    }

    // Check cluster influence - flowers cluster together
    uint32_t cluster_hash = Impl::hash_coords(world_x / 3, 0, world_z / 3, impl_->config.seed + 2000);
    float cluster_roll = Impl::hash_to_float(cluster_hash);
    bool in_cluster = cluster_roll < impl_->config.flowers.cluster_chance;

    float density = impl_->config.flowers.density;
    if (in_cluster) {
        density *= 5.0f;  // Higher density in clusters
    }

    uint32_t hash = Impl::hash_coords(world_x, 2, world_z, impl_->config.seed + 2001);
    float roll = Impl::hash_to_float(hash);

    if (roll >= density) {
        return BLOCK_AIR;
    }

    // Select flower type based on biome
    uint32_t type_hash = Impl::hash_coords(world_x, 3, world_z, impl_->config.seed + 2002);
    float type_roll = Impl::hash_to_float(type_hash);

    if (Impl::is_swamp(biome)) {
        // Swamp only gets blue orchids
        return impl_->blue_orchid_id;
    }

    // General flower distribution
    if (type_roll < 0.25f) {
        return impl_->poppy_id;
    } else if (type_roll < 0.50f) {
        return impl_->dandelion_id;
    } else if (type_roll < 0.65f) {
        return impl_->azure_bluet_id;
    } else if (type_roll < 0.80f) {
        return impl_->tulip_red_id;
    } else if (type_roll < 0.95f) {
        return impl_->cornflower_id;
    } else {
        return impl_->sunflower_id;
    }
}

bool VegetationGenerator::should_place_bush(int64_t world_x, int64_t world_z, BiomeType biome) const {
    if (!impl_->config.enabled || !impl_->config.bushes.enabled) {
        return false;
    }

    // Bushes in forested biomes
    if (biome != BiomeType::Forest && biome != BiomeType::Taiga && biome != BiomeType::Swamp &&
        biome != BiomeType::Plains) {
        return false;
    }

    uint32_t hash = Impl::hash_coords(world_x, 4, world_z, impl_->config.seed + 3000);
    float roll = Impl::hash_to_float(hash);

    return roll < impl_->config.bushes.density;
}

bool VegetationGenerator::should_place_dead_bush(int64_t world_x, int64_t world_z, BiomeType biome) const {
    if (!impl_->config.enabled || !impl_->config.dead_bush.enabled) {
        return false;
    }

    if (!Impl::has_dead_bushes(biome)) {
        return false;
    }

    uint32_t hash = Impl::hash_coords(world_x, 5, world_z, impl_->config.seed + 4000);
    float roll = Impl::hash_to_float(hash);

    return roll < impl_->config.dead_bush.density;
}

bool VegetationGenerator::should_place_cactus(int64_t world_x, int64_t world_z, BiomeType biome) const {
    if (!impl_->config.enabled || !impl_->config.cactus.enabled) {
        return false;
    }

    if (!Impl::is_desert(biome)) {
        return false;
    }

    uint32_t hash = Impl::hash_coords(world_x, 6, world_z, impl_->config.seed + 5000);
    float roll = Impl::hash_to_float(hash);

    return roll < impl_->config.cactus.density;
}

int32_t VegetationGenerator::get_cactus_height(int64_t world_x, int64_t world_z) const {
    uint32_t hash = Impl::hash_coords(world_x, 7, world_z, impl_->config.seed + 5001);
    int32_t height = 1 + static_cast<int32_t>(hash % static_cast<uint32_t>(impl_->config.cactus.max_height));
    return height;
}

bool VegetationGenerator::should_place_lily_pad(int64_t world_x, int64_t world_z, BiomeType biome) const {
    if (!impl_->config.enabled) {
        return false;
    }

    if (!Impl::is_swamp(biome)) {
        return false;
    }

    uint32_t hash = Impl::hash_coords(world_x, 8, world_z, impl_->config.seed + 6000);
    float roll = Impl::hash_to_float(hash);

    return roll < 0.15f;  // 15% of swamp water surfaces
}

const VegetationConfig& VegetationGenerator::get_config() const {
    return impl_->config;
}

void VegetationGenerator::set_config(const VegetationConfig& config) {
    impl_->config = config;
}

void VegetationGenerator::rebuild() {
    impl_->cache_block_ids();
}

}  // namespace realcraft::world
