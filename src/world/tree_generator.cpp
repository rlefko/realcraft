// RealCraft World System
// tree_generator.cpp - L-system based tree generation with biome templates

#include <array>
#include <cmath>
#include <realcraft/world/block.hpp>
#include <realcraft/world/tree_generator.hpp>

namespace realcraft::world {

// ============================================================================
// Implementation Details
// ============================================================================

struct TreeGenerator::Impl {
    TreeConfig config;
    std::array<TreeTemplate, static_cast<size_t>(TreeSpecies::Count)> templates;

    void initialize_templates() {
        const auto& registry = BlockRegistry::instance();

        // Get block IDs
        BlockId oak_log = BLOCK_AIR;
        BlockId oak_leaves = BLOCK_AIR;
        BlockId spruce_log = BLOCK_AIR;
        BlockId spruce_leaves = BLOCK_AIR;
        BlockId birch_log = BLOCK_AIR;
        BlockId birch_leaves = BLOCK_AIR;
        BlockId jungle_log = BLOCK_AIR;
        BlockId jungle_leaves = BLOCK_AIR;
        BlockId acacia_log = BLOCK_AIR;
        BlockId acacia_leaves = BLOCK_AIR;
        BlockId palm_log = BLOCK_AIR;
        BlockId palm_leaves = BLOCK_AIR;

        // Use existing wood/leaves for oak, new blocks for others
        if (auto id = registry.find_id("realcraft:wood"))
            oak_log = *id;
        if (auto id = registry.find_id("realcraft:leaves"))
            oak_leaves = *id;
        if (auto id = registry.find_id("realcraft:spruce_log"))
            spruce_log = *id;
        if (auto id = registry.find_id("realcraft:spruce_leaves"))
            spruce_leaves = *id;
        if (auto id = registry.find_id("realcraft:birch_log"))
            birch_log = *id;
        if (auto id = registry.find_id("realcraft:birch_leaves"))
            birch_leaves = *id;
        if (auto id = registry.find_id("realcraft:jungle_log"))
            jungle_log = *id;
        if (auto id = registry.find_id("realcraft:jungle_leaves"))
            jungle_leaves = *id;
        if (auto id = registry.find_id("realcraft:acacia_log"))
            acacia_log = *id;
        if (auto id = registry.find_id("realcraft:acacia_leaves"))
            acacia_leaves = *id;
        if (auto id = registry.find_id("realcraft:palm_log"))
            palm_log = *id;
        if (auto id = registry.find_id("realcraft:palm_leaves"))
            palm_leaves = *id;

        // Oak - Standard tree for forest/plains
        templates[static_cast<size_t>(TreeSpecies::Oak)] = {.species = TreeSpecies::Oak,
                                                            .name = "Oak",
                                                            .log_block = oak_log,
                                                            .leaf_block = oak_leaves,
                                                            .min_height = 4,
                                                            .max_height = 7,
                                                            .trunk_height_ratio = 60,
                                                            .canopy_radius = 2,
                                                            .canopy_height = 3,
                                                            .leaf_density = 0.85f,
                                                            .branch_angle = 25.0f,
                                                            .branch_probability = 0.3f};

        // Spruce - Tall conical tree for taiga
        templates[static_cast<size_t>(TreeSpecies::Spruce)] = {.species = TreeSpecies::Spruce,
                                                               .name = "Spruce",
                                                               .log_block = spruce_log,
                                                               .leaf_block = spruce_leaves,
                                                               .min_height = 6,
                                                               .max_height = 12,
                                                               .trunk_height_ratio = 80,
                                                               .canopy_radius = 2,
                                                               .canopy_height = 6,
                                                               .leaf_density = 0.90f,
                                                               .branch_angle = 20.0f,
                                                               .branch_probability = 0.1f};

        // Birch - Thin white-barked tree
        templates[static_cast<size_t>(TreeSpecies::Birch)] = {.species = TreeSpecies::Birch,
                                                              .name = "Birch",
                                                              .log_block = birch_log,
                                                              .leaf_block = birch_leaves,
                                                              .min_height = 5,
                                                              .max_height = 8,
                                                              .trunk_height_ratio = 65,
                                                              .canopy_radius = 2,
                                                              .canopy_height = 3,
                                                              .leaf_density = 0.80f,
                                                              .branch_angle = 20.0f,
                                                              .branch_probability = 0.2f};

        // Jungle - Large dense tree
        templates[static_cast<size_t>(TreeSpecies::Jungle)] = {.species = TreeSpecies::Jungle,
                                                               .name = "Jungle",
                                                               .log_block = jungle_log,
                                                               .leaf_block = jungle_leaves,
                                                               .min_height = 8,
                                                               .max_height = 15,
                                                               .trunk_height_ratio = 70,
                                                               .canopy_radius = 3,
                                                               .canopy_height = 4,
                                                               .leaf_density = 0.95f,
                                                               .branch_angle = 30.0f,
                                                               .branch_probability = 0.4f};

        // Acacia - Flat-topped savanna tree
        templates[static_cast<size_t>(TreeSpecies::Acacia)] = {.species = TreeSpecies::Acacia,
                                                               .name = "Acacia",
                                                               .log_block = acacia_log,
                                                               .leaf_block = acacia_leaves,
                                                               .min_height = 4,
                                                               .max_height = 6,
                                                               .trunk_height_ratio = 70,
                                                               .canopy_radius = 3,
                                                               .canopy_height = 1,  // Flat canopy
                                                               .leaf_density = 0.75f,
                                                               .branch_angle = 45.0f,
                                                               .branch_probability = 0.6f};

        // Palm - Beach/oasis tree
        templates[static_cast<size_t>(TreeSpecies::Palm)] = {
            .species = TreeSpecies::Palm,
            .name = "Palm",
            .log_block = palm_log,
            .leaf_block = palm_leaves,
            .min_height = 5,
            .max_height = 8,
            .trunk_height_ratio = 90,  // Mostly trunk
            .canopy_radius = 2,
            .canopy_height = 1,
            .leaf_density = 0.70f,
            .branch_angle = 60.0f,
            .branch_probability = 0.0f  // No branches, just fronds at top
        };
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

    [[nodiscard]] static float hash_to_float(uint32_t hash) { return static_cast<float>(hash % 10000) / 10000.0f; }

    // Get tree density multiplier for biome
    [[nodiscard]] float get_density_for_biome(BiomeType biome) const {
        switch (biome) {
            case BiomeType::Forest:
                return config.density.base_chance * config.density.forest_multiplier;
            case BiomeType::Taiga:
                return config.density.base_chance * config.density.taiga_multiplier;
            case BiomeType::Savanna:
                return config.density.base_chance * config.density.savanna_multiplier;
            case BiomeType::Plains:
                return config.density.base_chance * 0.3f;
            case BiomeType::Swamp:
                return config.density.base_chance * 2.0f;
            case BiomeType::Mountains:
                return config.density.base_chance * 0.2f;
            case BiomeType::Beach:
                return config.density.base_chance * 0.05f;  // Rare palms
            default:
                return 0.0f;  // No trees in ocean, desert, tundra, etc.
        }
    }

    // Check if biome can have trees
    [[nodiscard]] static bool biome_has_trees(BiomeType biome) {
        switch (biome) {
            case BiomeType::Forest:
            case BiomeType::Taiga:
            case BiomeType::Savanna:
            case BiomeType::Plains:
            case BiomeType::Swamp:
            case BiomeType::Mountains:
            case BiomeType::Beach:
                return true;
            default:
                return false;
        }
    }

    // Generate standard round/oval canopy
    void generate_round_canopy(std::vector<TreeBlock>& blocks, const TreeTemplate& tmpl, int32_t trunk_height,
                               uint32_t tree_seed) const {
        int32_t canopy_start = trunk_height - tmpl.canopy_height + 1;
        int32_t canopy_end = trunk_height + 1;

        for (int32_t y = canopy_start; y <= canopy_end; ++y) {
            // Radius varies with height (smaller at top and bottom)
            int32_t y_from_center = std::abs(y - (canopy_start + canopy_end) / 2);
            int32_t radius = tmpl.canopy_radius;
            if (y == canopy_start || y == canopy_end) {
                radius = std::max(1, radius - 1);
            }

            for (int32_t dx = -radius; dx <= radius; ++dx) {
                for (int32_t dz = -radius; dz <= radius; ++dz) {
                    // Skip corners for round shape
                    if (dx * dx + dz * dz > radius * radius + 1) {
                        continue;
                    }

                    // Skip trunk position at lower levels
                    if (dx == 0 && dz == 0 && y < trunk_height) {
                        continue;
                    }

                    // Random leaf placement based on density
                    uint32_t leaf_hash = hash_coords(dx, y, dz, tree_seed + 100);
                    if (hash_to_float(leaf_hash) < tmpl.leaf_density) {
                        blocks.push_back({LocalBlockPos(dx, y, dz), tmpl.leaf_block});
                    }
                }
            }
        }
    }

    // Generate conical canopy (for spruce)
    void generate_conical_canopy(std::vector<TreeBlock>& blocks, const TreeTemplate& tmpl, int32_t trunk_height,
                                 uint32_t tree_seed) const {
        int32_t canopy_start = trunk_height - tmpl.canopy_height + 1;
        int32_t canopy_end = trunk_height;

        for (int32_t y = canopy_start; y <= canopy_end; ++y) {
            // Radius decreases linearly from bottom to top
            float t = static_cast<float>(y - canopy_start) / static_cast<float>(tmpl.canopy_height);
            int32_t radius = static_cast<int32_t>(static_cast<float>(tmpl.canopy_radius) * (1.0f - t * 0.8f));
            radius = std::max(1, radius);

            for (int32_t dx = -radius; dx <= radius; ++dx) {
                for (int32_t dz = -radius; dz <= radius; ++dz) {
                    if (std::abs(dx) + std::abs(dz) > radius + 1) {
                        continue;
                    }

                    if (dx == 0 && dz == 0) {
                        continue;  // Trunk position
                    }

                    uint32_t leaf_hash = hash_coords(dx, y, dz, tree_seed + 200);
                    if (hash_to_float(leaf_hash) < tmpl.leaf_density) {
                        blocks.push_back({LocalBlockPos(dx, y, dz), tmpl.leaf_block});
                    }
                }
            }
        }

        // Top leaf
        blocks.push_back({LocalBlockPos(0, trunk_height + 1, 0), tmpl.leaf_block});
    }

    // Generate flat canopy (for acacia)
    void generate_flat_canopy(std::vector<TreeBlock>& blocks, const TreeTemplate& tmpl, int32_t trunk_height,
                              uint32_t tree_seed) const {
        int32_t radius = tmpl.canopy_radius;

        for (int32_t dx = -radius; dx <= radius; ++dx) {
            for (int32_t dz = -radius; dz <= radius; ++dz) {
                if (dx * dx + dz * dz > radius * radius + 2) {
                    continue;
                }

                uint32_t leaf_hash = hash_coords(dx, trunk_height, dz, tree_seed + 300);
                if (hash_to_float(leaf_hash) < tmpl.leaf_density) {
                    blocks.push_back({LocalBlockPos(dx, trunk_height, dz), tmpl.leaf_block});
                    // Add occasional second layer
                    if (hash_to_float(leaf_hash) < tmpl.leaf_density * 0.3f) {
                        blocks.push_back({LocalBlockPos(dx, trunk_height + 1, dz), tmpl.leaf_block});
                    }
                }
            }
        }
    }

    // Generate palm fronds
    void generate_palm_fronds(std::vector<TreeBlock>& blocks, const TreeTemplate& tmpl, int32_t trunk_height,
                              uint32_t tree_seed) const {
        // Fronds extend outward from top
        static const std::array<std::pair<int32_t, int32_t>, 8> directions = {
            {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}}};

        for (size_t i = 0; i < directions.size(); ++i) {
            auto [dx, dz] = directions[i];
            int32_t length = 2 + static_cast<int32_t>((tree_seed + i) % 2);

            for (int32_t d = 1; d <= length; ++d) {
                int32_t y_offset = trunk_height - (d - 1) / 2;  // Droop slightly
                blocks.push_back({LocalBlockPos(dx * d, y_offset, dz * d), tmpl.leaf_block});
            }
        }

        // Top tuft
        blocks.push_back({LocalBlockPos(0, trunk_height + 1, 0), tmpl.leaf_block});
    }
};

// ============================================================================
// TreeGenerator Implementation
// ============================================================================

TreeGenerator::TreeGenerator(const TreeConfig& config) : impl_(std::make_unique<Impl>()) {
    impl_->config = config;
    impl_->initialize_templates();
}

TreeGenerator::~TreeGenerator() = default;

TreeGenerator::TreeGenerator(TreeGenerator&&) noexcept = default;
TreeGenerator& TreeGenerator::operator=(TreeGenerator&&) noexcept = default;

bool TreeGenerator::should_place_tree(int64_t world_x, int64_t world_z, BiomeType biome) const {
    if (!impl_->config.enabled) {
        return false;
    }

    if (!Impl::biome_has_trees(biome)) {
        return false;
    }

    // Check spacing - use grid alignment for minimum spacing
    int32_t spacing = impl_->config.density.min_spacing + 1;
    int64_t grid_x = world_x / spacing;
    int64_t grid_z = world_z / spacing;

    // Only one tree per grid cell can spawn
    uint32_t grid_hash = Impl::hash_coords(grid_x, 0, grid_z, impl_->config.seed + 7000);
    int32_t spawn_x = static_cast<int32_t>(grid_hash % static_cast<uint32_t>(spacing));
    int32_t spawn_z = static_cast<int32_t>((grid_hash >> 8) % static_cast<uint32_t>(spacing));

    int64_t cell_origin_x = grid_x * spacing;
    int64_t cell_origin_z = grid_z * spacing;

    if (world_x != cell_origin_x + spawn_x || world_z != cell_origin_z + spawn_z) {
        return false;
    }

    // Probability check
    float density = impl_->get_density_for_biome(biome);
    uint32_t hash = Impl::hash_coords(world_x, 1, world_z, impl_->config.seed + 7001);
    float roll = Impl::hash_to_float(hash);

    return roll < density;
}

TreeSpecies TreeGenerator::get_species_for_biome(BiomeType biome, int64_t world_x, int64_t world_z) const {
    uint32_t hash = Impl::hash_coords(world_x, 2, world_z, impl_->config.seed + 7002);
    float roll = Impl::hash_to_float(hash);

    switch (biome) {
        case BiomeType::Forest:
            // Mix of oak and birch
            if (roll < 0.7f) {
                return TreeSpecies::Oak;
            } else {
                return TreeSpecies::Birch;
            }

        case BiomeType::Taiga:
            return TreeSpecies::Spruce;

        case BiomeType::Savanna:
            return TreeSpecies::Acacia;

        case BiomeType::Plains:
            return TreeSpecies::Oak;

        case BiomeType::Swamp:
            return TreeSpecies::Oak;  // Could be custom swamp oak

        case BiomeType::Mountains:
            if (roll < 0.6f) {
                return TreeSpecies::Spruce;
            } else {
                return TreeSpecies::Oak;
            }

        case BiomeType::Beach:
            return TreeSpecies::Palm;

        default:
            return TreeSpecies::Oak;
    }
}

const TreeTemplate& TreeGenerator::get_template(TreeSpecies species) const {
    return impl_->templates[static_cast<size_t>(species)];
}

std::vector<TreeBlock> TreeGenerator::generate_tree(int64_t world_x, int64_t world_z, BiomeType biome,
                                                    int32_t surface_y) const {
    std::vector<TreeBlock> blocks;

    TreeSpecies species = get_species_for_biome(biome, world_x, world_z);
    const TreeTemplate& tmpl = get_template(species);

    if (tmpl.log_block == BLOCK_AIR || tmpl.leaf_block == BLOCK_AIR) {
        return blocks;  // Template not initialized
    }

    // Determine tree height
    uint32_t tree_seed = Impl::hash_coords(world_x, surface_y, world_z, impl_->config.seed + 8000);
    int32_t height_range = tmpl.max_height - tmpl.min_height + 1;
    int32_t tree_height = tmpl.min_height;
    if (height_range > 0) {
        tree_height += static_cast<int32_t>(tree_seed % static_cast<uint32_t>(height_range));
    }

    // Apply size variation
    uint32_t size_hash = Impl::hash_coords(world_x, 3, world_z, impl_->config.seed + 8001);
    float size_roll = Impl::hash_to_float(size_hash);

    if (size_roll < impl_->config.size.small_weight) {
        tree_height = std::max(tmpl.min_height, tree_height - 2);
    } else if (size_roll > 1.0f - impl_->config.size.large_weight) {
        tree_height = std::min(tmpl.max_height + 2, tree_height + 2);
    }

    // Generate trunk
    for (int32_t y = 0; y < tree_height; ++y) {
        blocks.push_back({LocalBlockPos(0, y + 1, 0), tmpl.log_block});
    }

    // Generate canopy based on species
    switch (species) {
        case TreeSpecies::Spruce:
            impl_->generate_conical_canopy(blocks, tmpl, tree_height, tree_seed);
            break;

        case TreeSpecies::Acacia:
            impl_->generate_flat_canopy(blocks, tmpl, tree_height, tree_seed);
            break;

        case TreeSpecies::Palm:
            impl_->generate_palm_fronds(blocks, tmpl, tree_height, tree_seed);
            break;

        default:
            impl_->generate_round_canopy(blocks, tmpl, tree_height, tree_seed);
            break;
    }

    return blocks;
}

const TreeConfig& TreeGenerator::get_config() const {
    return impl_->config;
}

void TreeGenerator::set_config(const TreeConfig& config) {
    impl_->config = config;
}

void TreeGenerator::rebuild() {
    impl_->initialize_templates();
}

}  // namespace realcraft::world
