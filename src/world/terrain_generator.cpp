// RealCraft World System
// terrain_generator.cpp - Procedural terrain generation using FastNoise2

#include <FastNoise/FastNoise.h>
#include <algorithm>
#include <cmath>
#include <realcraft/world/biome.hpp>
#include <realcraft/world/block.hpp>
#include <realcraft/world/cave_generator.hpp>
#include <realcraft/world/climate.hpp>
#include <realcraft/world/erosion.hpp>
#include <realcraft/world/erosion_context.hpp>
#include <realcraft/world/erosion_heightmap.hpp>
#include <realcraft/world/ore_generator.hpp>
#include <realcraft/world/structure_generator.hpp>
#include <realcraft/world/terrain_generator.hpp>
#include <realcraft/world/tree_generator.hpp>
#include <realcraft/world/vegetation_generator.hpp>
#include <utility>

namespace realcraft::world {

// ============================================================================
// Implementation Details
// ============================================================================

struct TerrainGenerator::Impl {
    TerrainConfig config;

    // FastNoise2 nodes - created once, reused across threads
    // SmartNode is thread-safe for GenSingle calls
    FastNoise::SmartNode<FastNoise::Generator> continental_node;
    FastNoise::SmartNode<FastNoise::Generator> mountain_node;
    FastNoise::SmartNode<FastNoise::Generator> detail_node;
    FastNoise::SmartNode<FastNoise::Generator> domain_warp_node;
    FastNoise::SmartNode<FastNoise::Generator> density_node;

    // Climate map for biome determination (thread-safe)
    std::unique_ptr<ClimateMap> climate_map;

    // Erosion simulator (CPU-only for now, GPU requires device)
    std::unique_ptr<ErosionSimulator> erosion_simulator;

    // Erosion context for cross-chunk border exchange (not owned)
    ErosionContext* erosion_context = nullptr;

    // Cave generator (thread-safe noise-based cave carving)
    std::unique_ptr<CaveGenerator> cave_generator;

    // Feature generators (Milestone 4.5)
    std::unique_ptr<OreGenerator> ore_generator;
    std::unique_ptr<VegetationGenerator> vegetation_generator;
    std::unique_ptr<TreeGenerator> tree_generator;
    std::unique_ptr<StructureGenerator> structure_generator;

    void build_nodes() {
        // Continental terrain (large scale shapes using FBm)
        auto continental_simplex = FastNoise::New<FastNoise::Simplex>();
        auto continental_fractal = FastNoise::New<FastNoise::FractalFBm>();
        continental_fractal->SetSource(continental_simplex);
        continental_fractal->SetOctaveCount(config.continental.octaves);
        continental_fractal->SetGain(config.continental.gain);
        continental_fractal->SetLacunarity(config.continental.lacunarity);
        continental_node = continental_fractal;

        // Mountain/ridge terrain (ridged for sharp peaks)
        auto mountain_simplex = FastNoise::New<FastNoise::Simplex>();
        auto mountain_fractal = FastNoise::New<FastNoise::FractalRidged>();
        mountain_fractal->SetSource(mountain_simplex);
        mountain_fractal->SetOctaveCount(config.mountain.octaves);
        mountain_fractal->SetGain(config.mountain.gain);
        mountain_fractal->SetLacunarity(config.mountain.lacunarity);
        mountain_node = mountain_fractal;

        // Detail terrain (small scale variation with FBm)
        auto detail_simplex = FastNoise::New<FastNoise::Simplex>();
        auto detail_fractal = FastNoise::New<FastNoise::FractalFBm>();
        detail_fractal->SetSource(detail_simplex);
        detail_fractal->SetOctaveCount(config.detail.octaves);
        detail_fractal->SetGain(config.detail.gain);
        detail_fractal->SetLacunarity(config.detail.lacunarity);
        detail_node = detail_fractal;

        // Domain warping (makes terrain more organic)
        if (config.domain_warp.enabled) {
            auto warp_simplex = FastNoise::New<FastNoise::Simplex>();
            auto warp_fractal = FastNoise::New<FastNoise::FractalFBm>();
            warp_fractal->SetSource(warp_simplex);
            warp_fractal->SetOctaveCount(3);
            warp_fractal->SetGain(0.5f);
            warp_fractal->SetLacunarity(2.0f);
            domain_warp_node = warp_fractal;
        }

        // 3D density for caves
        if (config.density.enabled) {
            auto density_simplex = FastNoise::New<FastNoise::Simplex>();
            auto density_fractal = FastNoise::New<FastNoise::FractalFBm>();
            density_fractal->SetSource(density_simplex);
            density_fractal->SetOctaveCount(config.density.octaves);
            density_fractal->SetGain(config.density.gain);
            density_fractal->SetLacunarity(config.density.lacunarity);
            density_node = density_fractal;
        }

        // Climate map for biome determination
        if (config.biome_system.enabled) {
            ClimateConfig climate_cfg = config.biome_system.climate;
            climate_cfg.seed = config.seed;  // Use same seed as terrain
            climate_map = std::make_unique<ClimateMap>(climate_cfg);
        } else {
            climate_map.reset();
        }

        // Erosion simulator (CPU-only, no graphics device)
        if (config.erosion.enabled) {
            erosion_simulator = std::make_unique<ErosionSimulator>(nullptr);
        } else {
            erosion_simulator.reset();
        }

        // Cave generator
        if (config.caves.enabled) {
            CaveConfig cave_cfg = config.caves;
            cave_cfg.seed = config.seed;  // Use same seed as terrain
            cave_generator = std::make_unique<CaveGenerator>(cave_cfg);
        } else {
            cave_generator.reset();
        }

        // Feature generators (Milestone 4.5)
        // Ore generator
        if (config.ores.enabled) {
            OreConfig ore_cfg = config.ores;
            ore_cfg.seed = config.seed;
            ore_generator = std::make_unique<OreGenerator>(ore_cfg);
        } else {
            ore_generator.reset();
        }

        // Vegetation generator
        if (config.vegetation.enabled) {
            VegetationConfig veg_cfg = config.vegetation;
            veg_cfg.seed = config.seed;
            vegetation_generator = std::make_unique<VegetationGenerator>(veg_cfg);
        } else {
            vegetation_generator.reset();
        }

        // Tree generator
        if (config.trees.enabled) {
            TreeConfig tree_cfg = config.trees;
            tree_cfg.seed = config.seed;
            tree_generator = std::make_unique<TreeGenerator>(tree_cfg);
        } else {
            tree_generator.reset();
        }

        // Structure generator
        if (config.structures.enabled) {
            StructureConfig struct_cfg = config.structures;
            struct_cfg.seed = config.seed;
            structure_generator = std::make_unique<StructureGenerator>(struct_cfg);
        } else {
            structure_generator.reset();
        }
    }

    // Get warped coordinates for sampling (domain warping for organic look)
    [[nodiscard]] std::pair<float, float> warp_coords(float x, float z) const {
        if (!config.domain_warp.enabled || !domain_warp_node) {
            return {x, z};
        }

        // Sample warp offset using noise
        float wx = x * config.domain_warp.scale;
        float wz = z * config.domain_warp.scale;

        // Use two different sample points for X and Z offsets
        float offset_x =
            domain_warp_node->GenSingle2D(wx, wz, static_cast<int>(config.seed + 10000)) * config.domain_warp.amplitude;
        float offset_z =
            domain_warp_node->GenSingle2D(wx + 31337.0f, wz + 31337.0f, static_cast<int>(config.seed + 10000)) *
            config.domain_warp.amplitude;

        return {x + offset_x, z + offset_z};
    }

    [[nodiscard]] int32_t compute_height(int64_t world_x, int64_t world_z) const {
        // Apply domain warping for organic terrain
        auto [wx, wz] = warp_coords(static_cast<float>(world_x), static_cast<float>(world_z));

        // Sample continental noise (large features)
        float continental = continental_node->GenSingle2D(wx * config.continental.scale, wz * config.continental.scale,
                                                          static_cast<int>(config.seed));

        // Sample mountain noise (ridged for peaks)
        float mountain = mountain_node->GenSingle2D(wx * config.mountain.scale, wz * config.mountain.scale,
                                                    static_cast<int>(config.seed + 1));

        // Sample detail noise (fine variation)
        float detail = detail_node->GenSingle2D(wx * config.detail.scale, wz * config.detail.scale,
                                                static_cast<int>(config.seed + 2));

        // Combine noise layers with configured weights
        float combined =
            continental * config.continental.weight + mountain * config.mountain.weight + detail * config.detail.weight;

        // Map to height range (noise output is approximately [-1, 1])
        int32_t height =
            config.base_height + static_cast<int32_t>(combined * static_cast<float>(config.height_variation));

        // Clamp to valid range
        return std::clamp(height, config.min_height, config.max_height);
    }

    [[nodiscard]] float compute_raw_density(int64_t world_x, int64_t world_y, int64_t world_z) const {
        if (!config.density.enabled || !density_node) {
            return 0.0f;
        }

        // Sample 3D density noise
        return density_node->GenSingle3D(
            static_cast<float>(world_x) * config.density.scale, static_cast<float>(world_y) * config.density.scale,
            static_cast<float>(world_z) * config.density.scale, static_cast<int>(config.seed + 100));
    }

    [[nodiscard]] float compute_density(int64_t world_x, int64_t world_y, int64_t world_z,
                                        int32_t surface_height) const {
        // Above surface is always air
        if (world_y > surface_height) {
            return -1.0f;
        }

        if (!config.density.enabled || !density_node) {
            // No 3D density, everything at or below surface is solid
            return 1.0f;
        }

        // Sample 3D density noise
        float raw_density = compute_raw_density(world_x, world_y, world_z);

        // Distance from surface (positive = deeper underground)
        float depth = static_cast<float>(surface_height - world_y);

        // Blend factor: 0 at surface, 1 when deep underground
        // Surface is always solid, caves only form when depth > 0
        float blend = std::clamp(depth / config.density.surface_blend, 0.0f, 1.0f);

        // At surface (depth=0): density = 1.0 (solid)
        // Deep underground (depth >= surface_blend): density = raw_density (can be negative for caves)
        float density = (1.0f - blend) * 1.0f + blend * raw_density;

        return density - config.density.threshold;
    }

    /// Helper to select between two blocks based on blend factor with deterministic noise
    [[nodiscard]] BlockId select_blended_block(BlockId primary, BlockId secondary, float blend_factor, int64_t world_x,
                                               int64_t world_z) const {
        if (blend_factor < 0.01f || primary == secondary) {
            return primary;
        }

        // Use deterministic hash for consistent results across calls
        auto hash = static_cast<uint32_t>(world_x * 73856093) ^ static_cast<uint32_t>(world_z * 19349663) ^ config.seed;
        hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
        hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
        float noise = static_cast<float>(hash & 0xFFFF) / 65535.0f;

        return (noise < blend_factor) ? secondary : primary;
    }
};

// ============================================================================
// TerrainGenerator Implementation
// ============================================================================

TerrainGenerator::TerrainGenerator(const TerrainConfig& config) : impl_(std::make_unique<Impl>()) {
    impl_->config = config;
    impl_->build_nodes();
}

TerrainGenerator::~TerrainGenerator() = default;

TerrainGenerator::TerrainGenerator(TerrainGenerator&&) noexcept = default;
TerrainGenerator& TerrainGenerator::operator=(TerrainGenerator&&) noexcept = default;

void TerrainGenerator::generate(Chunk& chunk) const {
    auto lock = chunk.write_lock();

    const auto& block_registry = BlockRegistry::instance();
    const auto& biome_registry = BiomeRegistry::instance();

    // Fallback block IDs (used when biome system is disabled)
    const BlockId fallback_stone_id = block_registry.stone_id();
    const BlockId fallback_dirt_id = block_registry.dirt_id();
    const BlockId fallback_grass_id = block_registry.grass_id();
    const BlockId fallback_sand_id = block_registry.sand_id();
    const BlockId water_id = block_registry.water_id();

    // Sediment block IDs (for erosion deposits)
    const auto silt_id_opt = block_registry.find_id("realcraft:silt");
    const auto alluvium_id_opt = block_registry.find_id("realcraft:alluvium");
    const auto river_gravel_id_opt = block_registry.find_id("realcraft:river_gravel");
    const BlockId silt_id = silt_id_opt.value_or(fallback_sand_id);
    const BlockId alluvium_id = alluvium_id_opt.value_or(fallback_dirt_id);
    const BlockId river_gravel_id = river_gravel_id_opt.value_or(fallback_sand_id);

    // Cave decoration block IDs
    const auto stalactite_id_opt = block_registry.find_id("realcraft:stalactite");
    const auto stalagmite_id_opt = block_registry.find_id("realcraft:stalagmite");
    const auto crystal_id_opt = block_registry.find_id("realcraft:crystal");
    const auto cave_moss_id_opt = block_registry.find_id("realcraft:cave_moss");
    const auto glowing_mushroom_id_opt = block_registry.find_id("realcraft:glowing_mushroom");
    const BlockId stalactite_id = stalactite_id_opt.value_or(BLOCK_AIR);
    const BlockId stalagmite_id = stalagmite_id_opt.value_or(BLOCK_AIR);
    const BlockId crystal_id = crystal_id_opt.value_or(BLOCK_AIR);
    const BlockId cave_moss_id = cave_moss_id_opt.value_or(BLOCK_AIR);
    const BlockId glowing_mushroom_id = glowing_mushroom_id_opt.value_or(BLOCK_AIR);

    const ChunkPos chunk_pos = chunk.get_position();
    const int64_t base_x = static_cast<int64_t>(chunk_pos.x) * CHUNK_SIZE_X;
    const int64_t base_z = static_cast<int64_t>(chunk_pos.y) * CHUNK_SIZE_Z;

    // Pre-compute heightmap for this chunk for efficiency
    std::array<int32_t, CHUNK_SIZE_X * CHUNK_SIZE_Z> heights{};
    for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
        for (int x = 0; x < CHUNK_SIZE_X; ++x) {
            heights[static_cast<size_t>(z * CHUNK_SIZE_X + x)] = impl_->compute_height(base_x + x, base_z + z);
        }
    }

    // Apply erosion if enabled
    // Track sediment for block type selection
    std::array<float, CHUNK_SIZE_X * CHUNK_SIZE_Z> sediment_levels{};
    if (impl_->erosion_simulator && impl_->config.erosion.enabled) {
        // Create erosion heightmap with border for cross-chunk context
        ErosionHeightmap erosion_map(CHUNK_SIZE_X, CHUNK_SIZE_Z, impl_->config.erosion.border_size);
        erosion_map.populate_from_generator(*this, chunk_pos);

        // Store original heights for computing deltas after erosion
        erosion_map.store_original_heights();

        // Import border data from already-generated neighbor chunks
        if (impl_->erosion_context) {
            for (int dir = 0; dir < static_cast<int>(HorizontalDirection::Count); ++dir) {
                auto hdir = static_cast<HorizontalDirection>(dir);
                auto neighbor_data = impl_->erosion_context->get_neighbor_border(chunk_pos, hdir);
                if (neighbor_data) {
                    erosion_map.import_border_heights(hdir, neighbor_data->height_deltas);
                    erosion_map.import_border_sediment(hdir, neighbor_data->sediment_values);
                    erosion_map.import_border_flow(hdir, neighbor_data->flow_values);
                }
            }
        }

        // Run erosion simulation
        impl_->erosion_simulator->simulate(erosion_map, impl_->config.erosion, impl_->config.seed);

        // Export border data for future neighbor chunks
        if (impl_->erosion_context) {
            for (int dir = 0; dir < static_cast<int>(HorizontalDirection::Count); ++dir) {
                auto hdir = static_cast<HorizontalDirection>(dir);
                auto border_data = erosion_map.export_border_data(hdir);
                border_data.source_chunk = chunk_pos;
                impl_->erosion_context->submit_border_data(border_data);
            }
        }

        // Apply eroded heights back to the heights array
        erosion_map.apply_to_heights(heights);

        // Extract sediment levels for block type selection
        const int32_t border = erosion_map.border();
        for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
            for (int x = 0; x < CHUNK_SIZE_X; ++x) {
                sediment_levels[static_cast<size_t>(z * CHUNK_SIZE_X + x)] =
                    erosion_map.get_sediment(x + border, z + border);
            }
        }
    }

    // Track biome at center of chunk for metadata
    BiomeType chunk_biome = BiomeType::Plains;
    const int center_x = CHUNK_SIZE_X / 2;
    const int center_z = CHUNK_SIZE_Z / 2;

    // Generate blocks column by column
    for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
        for (int x = 0; x < CHUNK_SIZE_X; ++x) {
            const int32_t surface_height = heights[static_cast<size_t>(z * CHUNK_SIZE_X + x)];
            const int64_t world_x = base_x + x;
            const int64_t world_z = base_z + z;

            // Get sediment level for this column
            const float sediment = sediment_levels[static_cast<size_t>(z * CHUNK_SIZE_X + x)];

            // Get biome with blend info for smooth transitions
            BiomeBlockPalette primary_palette;
            BiomeBlockPalette secondary_palette;
            BiomeType biome = BiomeType::Plains;
            float blend_factor = 0.0f;

            if (impl_->climate_map) {
                // Use blended sampling for smooth biome transitions
                BiomeBlend blend =
                    impl_->climate_map->sample_blended(world_x, world_z, static_cast<float>(surface_height));
                biome = blend.primary_biome;
                blend_factor = blend.blend_factor;

                primary_palette = biome_registry.get_palette(blend.primary_biome);
                secondary_palette = biome_registry.get_palette(blend.secondary_biome);

                // Handle invalid palette entries by using fallback blocks
                if (primary_palette.surface_block == BLOCK_INVALID)
                    primary_palette.surface_block = fallback_grass_id;
                if (primary_palette.subsurface_block == BLOCK_INVALID)
                    primary_palette.subsurface_block = fallback_dirt_id;
                if (primary_palette.underwater_block == BLOCK_INVALID)
                    primary_palette.underwater_block = fallback_sand_id;
                if (primary_palette.stone_block == BLOCK_INVALID)
                    primary_palette.stone_block = fallback_stone_id;

                if (secondary_palette.surface_block == BLOCK_INVALID)
                    secondary_palette.surface_block = fallback_grass_id;
                if (secondary_palette.subsurface_block == BLOCK_INVALID)
                    secondary_palette.subsurface_block = fallback_dirt_id;
                if (secondary_palette.underwater_block == BLOCK_INVALID)
                    secondary_palette.underwater_block = fallback_sand_id;
                if (secondary_palette.stone_block == BLOCK_INVALID)
                    secondary_palette.stone_block = fallback_stone_id;

                // Record center biome for chunk metadata
                if (x == center_x && z == center_z) {
                    chunk_biome = biome;
                }
            } else {
                // Biome system disabled - use fallback palette
                primary_palette.surface_block = fallback_grass_id;
                primary_palette.subsurface_block = fallback_dirt_id;
                primary_palette.underwater_block = fallback_sand_id;
                primary_palette.stone_block = fallback_stone_id;
                secondary_palette = primary_palette;
            }

            // Pre-compute blended block selections for this column
            BlockId blended_surface = impl_->select_blended_block(
                primary_palette.surface_block, secondary_palette.surface_block, blend_factor, world_x, world_z);
            BlockId blended_subsurface =
                impl_->select_blended_block(primary_palette.subsurface_block, secondary_palette.subsurface_block,
                                            blend_factor, world_x, world_z + 10000);  // Different hash
            BlockId blended_underwater =
                impl_->select_blended_block(primary_palette.underwater_block, secondary_palette.underwater_block,
                                            blend_factor, world_x, world_z + 20000);  // Different hash
            BlockId blended_stone =
                impl_->select_blended_block(primary_palette.stone_block, secondary_palette.stone_block, blend_factor,
                                            world_x, world_z + 30000);  // Different hash

            // Override surface blocks with sediment blocks if there's significant deposit
            BlockId sediment_surface = blended_surface;
            BlockId sediment_subsurface = blended_subsurface;
            if (sediment >= impl_->config.erosion.sediment.gravel_threshold) {
                sediment_surface = river_gravel_id;
                sediment_subsurface = river_gravel_id;
            } else if (sediment >= impl_->config.erosion.sediment.alluvium_threshold) {
                sediment_surface = alluvium_id;
                sediment_subsurface = alluvium_id;
            } else if (sediment >= impl_->config.erosion.sediment.silt_threshold) {
                sediment_surface = silt_id;
                sediment_subsurface = silt_id;
            }

            for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
                BlockId block_id = BLOCK_AIR;

                // Check density for this voxel (for caves/overhangs)
                float density = impl_->compute_density(world_x, y, world_z, surface_height);

                // Check for cave carving (Perlin worms and chambers)
                bool carved_by_cave = false;
                if (impl_->cave_generator && density > 0 && y > 0) {
                    carved_by_cave = impl_->cave_generator->should_carve(world_x, y, world_z, surface_height);
                }

                if (density > 0 && !carved_by_cave) {
                    // Solid block - determine type based on depth
                    if (y == 0) {
                        // Bottom layer - could be bedrock in future
                        block_id = blended_stone;
                    } else if (y < surface_height - impl_->config.dirt_depth) {
                        // Deep underground - stone
                        block_id = blended_stone;
                    } else if (y < surface_height) {
                        // Near surface - subsurface layer (use sediment if applicable)
                        block_id = sediment_subsurface;
                    } else if (y == surface_height) {
                        // Surface block - depends on height and sediment
                        if (surface_height <= impl_->config.sea_level) {
                            // At or below sea level - use sediment or underwater block
                            block_id = (sediment > 0) ? sediment_surface : blended_underwater;
                        } else {
                            // Above sea level - use sediment or surface block
                            block_id = (sediment > 0) ? sediment_surface : blended_surface;
                        }
                    }
                } else if (carved_by_cave) {
                    // Cave carved - check if flooded
                    if (impl_->cave_generator->is_flooded(world_x, y, world_z)) {
                        block_id = water_id;
                    }
                    // else: air (default)
                } else if (y <= impl_->config.sea_level && y > surface_height) {
                    // Air pocket but below sea level - fill with water
                    block_id = water_id;
                }
                // else: air (default)

                if (block_id != BLOCK_AIR) {
                    lock.set_block(LocalBlockPos(x, y, z), block_id);
                }
            }
        }
    }

    // Cave decoration pass (place stalactites, stalagmites, crystals, moss, mushrooms)
    if (impl_->cave_generator && impl_->config.caves.decorations.enabled) {
        for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
            for (int x = 0; x < CHUNK_SIZE_X; ++x) {
                const int64_t world_x = base_x + x;
                const int64_t world_z = base_z + z;

                for (int y = impl_->config.caves.min_y; y < impl_->config.caves.max_y && y < CHUNK_SIZE_Y - 1; ++y) {
                    BlockId current = lock.get_block(LocalBlockPos(x, y, z));

                    // Skip non-air blocks
                    if (current != BLOCK_AIR) {
                        continue;
                    }

                    // Check for cave surfaces
                    BlockId above = (y + 1 < CHUNK_SIZE_Y) ? lock.get_block(LocalBlockPos(x, y + 1, z)) : BLOCK_AIR;
                    BlockId below = (y > 0) ? lock.get_block(LocalBlockPos(x, y - 1, z)) : BLOCK_AIR;

                    bool has_ceiling = (above != BLOCK_AIR && above != water_id);
                    bool has_floor = (below != BLOCK_AIR && below != water_id);

                    // Stalactites hang from ceiling (stone above, air at current position)
                    if (has_ceiling && stalactite_id != BLOCK_AIR) {
                        if (impl_->cave_generator->should_place_stalactite(world_x, y, world_z)) {
                            lock.set_block(LocalBlockPos(x, y, z), stalactite_id);
                            continue;
                        }
                    }

                    // Stalagmites rise from floor (stone below, air at current position)
                    if (has_floor && stalagmite_id != BLOCK_AIR) {
                        if (impl_->cave_generator->should_place_stalagmite(world_x, y, world_z)) {
                            lock.set_block(LocalBlockPos(x, y, z), stalagmite_id);
                            continue;
                        }
                    }

                    // Crystals in deep caves on floor
                    if (has_floor && y < 40 && crystal_id != BLOCK_AIR) {
                        if (impl_->cave_generator->should_place_crystal(world_x, y, world_z)) {
                            lock.set_block(LocalBlockPos(x, y, z), crystal_id);
                            continue;
                        }
                    }

                    // Check if in damp area for moss/mushrooms
                    bool is_damp = impl_->cave_generator->is_damp_area(world_x, y, world_z);

                    // Cave moss on walls/floor in damp areas
                    if (is_damp && has_floor && cave_moss_id != BLOCK_AIR) {
                        if (impl_->cave_generator->should_place_moss(world_x, y, world_z)) {
                            lock.set_block(LocalBlockPos(x, y, z), cave_moss_id);
                            continue;
                        }
                    }

                    // Glowing mushrooms in damp areas
                    if (is_damp && has_floor && glowing_mushroom_id != BLOCK_AIR) {
                        if (impl_->cave_generator->should_place_mushroom(world_x, y, world_z)) {
                            lock.set_block(LocalBlockPos(x, y, z), glowing_mushroom_id);
                            continue;
                        }
                    }
                }
            }
        }
    }

    // ========================================================================
    // Feature Generation (Milestone 4.5)
    // ========================================================================

    // Ore generation pass - replace stone with ores
    if (impl_->ore_generator) {
        auto ore_positions = impl_->ore_generator->generate_ores(chunk_pos, chunk_biome);
        for (const auto& ore : ore_positions) {
            // Only replace stone blocks with ores
            BlockId existing = lock.get_block(ore.pos);
            if (existing == fallback_stone_id) {
                lock.set_block(ore.pos, ore.block);
            }
        }
    }

    // Structure generation pass (rock spires, boulders)
    if (impl_->structure_generator) {
        for (size_t t = 0; t < static_cast<size_t>(StructureType::Count); ++t) {
            StructureType type = static_cast<StructureType>(t);
            if (impl_->structure_generator->should_place_structure(chunk_pos, type, chunk_biome)) {
                auto pos_opt = impl_->structure_generator->get_structure_position(chunk_pos, type);
                if (pos_opt) {
                    int32_t surface_y = heights[static_cast<size_t>(pos_opt->z * CHUNK_SIZE_X + pos_opt->x)];
                    auto structure_blocks = impl_->structure_generator->generate_structure(chunk_pos, type, surface_y);
                    for (const auto& sb : structure_blocks) {
                        LocalBlockPos world_pos(pos_opt->x + sb.offset.x, surface_y + sb.offset.y,
                                                pos_opt->z + sb.offset.z);
                        // Bounds check
                        if (world_pos.x >= 0 && world_pos.x < CHUNK_SIZE_X && world_pos.z >= 0 &&
                            world_pos.z < CHUNK_SIZE_Z && world_pos.y > 0 && world_pos.y < CHUNK_SIZE_Y) {
                            lock.set_block(world_pos, sb.block);
                        }
                    }
                }
            }
        }
    }

    // Tree generation pass
    if (impl_->tree_generator) {
        for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
            for (int x = 0; x < CHUNK_SIZE_X; ++x) {
                const int64_t world_x = base_x + x;
                const int64_t world_z = base_z + z;
                const int32_t surface_y = heights[static_cast<size_t>(z * CHUNK_SIZE_X + x)];

                // Skip if below sea level
                if (surface_y <= impl_->config.sea_level) {
                    continue;
                }

                // Get biome at this position
                BiomeType local_biome = chunk_biome;
                if (impl_->climate_map) {
                    local_biome = impl_->climate_map->get_biome(world_x, world_z, static_cast<float>(surface_y));
                }

                if (impl_->tree_generator->should_place_tree(world_x, world_z, local_biome)) {
                    auto tree_blocks = impl_->tree_generator->generate_tree(world_x, world_z, local_biome, surface_y);
                    for (const auto& tb : tree_blocks) {
                        LocalBlockPos world_pos(x + tb.offset.x, surface_y + tb.offset.y, z + tb.offset.z);
                        // Bounds check - trees can extend beyond chunk
                        if (world_pos.x >= 0 && world_pos.x < CHUNK_SIZE_X && world_pos.z >= 0 &&
                            world_pos.z < CHUNK_SIZE_Z && world_pos.y > 0 && world_pos.y < CHUNK_SIZE_Y) {
                            // Only replace air or replaceable blocks
                            BlockId existing = lock.get_block(world_pos);
                            const BlockType* block_type = block_registry.get(existing);
                            if (existing == BLOCK_AIR || (block_type && block_type->is_replaceable())) {
                                lock.set_block(world_pos, tb.block);
                            }
                        }
                    }
                }
            }
        }
    }

    // Vegetation generation pass (grass, flowers, bushes, cacti)
    if (impl_->vegetation_generator) {
        const auto& registry = BlockRegistry::instance();
        const auto tall_grass_id = registry.find_id("realcraft:tall_grass").value_or(BLOCK_AIR);
        const auto fern_id = registry.find_id("realcraft:fern").value_or(BLOCK_AIR);
        const auto dead_bush_id = registry.find_id("realcraft:dead_bush").value_or(BLOCK_AIR);
        const auto cactus_id = registry.find_id("realcraft:cactus").value_or(BLOCK_AIR);
        const auto bush_id = registry.find_id("realcraft:bush").value_or(BLOCK_AIR);
        const auto lily_pad_id = registry.find_id("realcraft:lily_pad").value_or(BLOCK_AIR);

        for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
            for (int x = 0; x < CHUNK_SIZE_X; ++x) {
                const int64_t world_x = base_x + x;
                const int64_t world_z = base_z + z;
                const int32_t surface_y = heights[static_cast<size_t>(z * CHUNK_SIZE_X + x)];

                // Get biome at this position
                BiomeType local_biome = chunk_biome;
                if (impl_->climate_map) {
                    local_biome = impl_->climate_map->get_biome(world_x, world_z, static_cast<float>(surface_y));
                }

                // Check if surface is above sea level
                if (surface_y > impl_->config.sea_level) {
                    // Get block at surface and above
                    if (surface_y + 1 >= CHUNK_SIZE_Y)
                        continue;
                    BlockId surface_block = lock.get_block(LocalBlockPos(x, surface_y, z));
                    BlockId above_block = lock.get_block(LocalBlockPos(x, surface_y + 1, z));

                    // Only place on grass blocks with air above
                    if (above_block != BLOCK_AIR)
                        continue;

                    // Cactus (desert, on sand)
                    if (local_biome == BiomeType::Desert && surface_block == fallback_sand_id) {
                        if (impl_->vegetation_generator->should_place_cactus(world_x, world_z, local_biome)) {
                            int32_t cactus_height = impl_->vegetation_generator->get_cactus_height(world_x, world_z);
                            for (int32_t cy = 1; cy <= cactus_height && surface_y + cy < CHUNK_SIZE_Y; ++cy) {
                                lock.set_block(LocalBlockPos(x, surface_y + cy, z), cactus_id);
                            }
                            continue;
                        }
                    }

                    // Dead bush (desert/savanna)
                    if (impl_->vegetation_generator->should_place_dead_bush(world_x, world_z, local_biome)) {
                        lock.set_block(LocalBlockPos(x, surface_y + 1, z), dead_bush_id);
                        continue;
                    }

                    // Bush (forested biomes, on grass)
                    if (surface_block == fallback_grass_id) {
                        if (impl_->vegetation_generator->should_place_bush(world_x, world_z, local_biome)) {
                            lock.set_block(LocalBlockPos(x, surface_y + 1, z), bush_id);
                            continue;
                        }

                        // Flowers (before grass, lower density)
                        BlockId flower = impl_->vegetation_generator->get_flower_block(world_x, world_z, local_biome);
                        if (flower != BLOCK_AIR) {
                            lock.set_block(LocalBlockPos(x, surface_y + 1, z), flower);
                            continue;
                        }

                        // Tall grass / fern
                        if (impl_->vegetation_generator->should_place_grass(world_x, world_z, local_biome)) {
                            BlockId grass_block =
                                impl_->vegetation_generator->get_grass_block(world_x, world_z, local_biome);
                            lock.set_block(LocalBlockPos(x, surface_y + 1, z), grass_block);
                        }
                    }
                } else if (surface_y == impl_->config.sea_level) {
                    // Lily pads on water surface (swamp)
                    if (local_biome == BiomeType::Swamp) {
                        BlockId at_water_level = lock.get_block(LocalBlockPos(x, surface_y, z));
                        if (at_water_level == water_id) {
                            if (impl_->vegetation_generator->should_place_lily_pad(world_x, world_z, local_biome)) {
                                lock.set_block(LocalBlockPos(x, surface_y + 1, z), lily_pad_id);
                            }
                        }
                    }
                }
            }
        }
    }

    // Store biome in chunk metadata
    chunk.get_metadata_mut().biome_id = static_cast<uint8_t>(chunk_biome);
    chunk.get_metadata_mut().has_been_generated = true;
}

std::array<int32_t, CHUNK_SIZE_X * CHUNK_SIZE_Z> TerrainGenerator::generate_heightmap(const ChunkPos& pos) const {
    std::array<int32_t, CHUNK_SIZE_X * CHUNK_SIZE_Z> heights{};
    const int64_t base_x = static_cast<int64_t>(pos.x) * CHUNK_SIZE_X;
    const int64_t base_z = static_cast<int64_t>(pos.y) * CHUNK_SIZE_Z;

    for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
        for (int x = 0; x < CHUNK_SIZE_X; ++x) {
            heights[static_cast<size_t>(z * CHUNK_SIZE_X + x)] = impl_->compute_height(base_x + x, base_z + z);
        }
    }
    return heights;
}

int32_t TerrainGenerator::get_height(int64_t world_x, int64_t world_z) const {
    return impl_->compute_height(world_x, world_z);
}

float TerrainGenerator::get_density(int64_t world_x, int64_t world_y, int64_t world_z) const {
    int32_t surface = impl_->compute_height(world_x, world_z);
    return impl_->compute_density(world_x, world_y, world_z, surface);
}

float TerrainGenerator::get_raw_density(int64_t world_x, int64_t world_y, int64_t world_z) const {
    return impl_->compute_raw_density(world_x, world_y, world_z);
}

const TerrainConfig& TerrainGenerator::get_config() const {
    return impl_->config;
}

void TerrainGenerator::set_config(const TerrainConfig& config) {
    impl_->config = config;
}

void TerrainGenerator::rebuild_nodes() {
    impl_->build_nodes();
}

BiomeType TerrainGenerator::get_biome(int64_t world_x, int64_t world_z) const {
    if (!impl_->climate_map) {
        return BiomeType::Plains;
    }
    int32_t height = impl_->compute_height(world_x, world_z);
    return impl_->climate_map->get_biome(world_x, world_z, static_cast<float>(height));
}

ClimateSample TerrainGenerator::get_climate(int64_t world_x, int64_t world_z) const {
    if (!impl_->climate_map) {
        ClimateSample sample;
        sample.temperature = 0.5f;
        sample.humidity = 0.5f;
        sample.biome = BiomeType::Plains;
        return sample;
    }
    int32_t height = impl_->compute_height(world_x, world_z);
    return impl_->climate_map->sample(world_x, world_z, static_cast<float>(height));
}

BiomeBlend TerrainGenerator::get_biome_blend(int64_t world_x, int64_t world_z) const {
    if (!impl_->climate_map) {
        BiomeBlend blend;
        blend.primary_biome = BiomeType::Plains;
        blend.secondary_biome = BiomeType::Plains;
        blend.blend_factor = 0.0f;
        blend.blended_height = BiomeHeightModifiers{};
        return blend;
    }
    int32_t height = impl_->compute_height(world_x, world_z);
    return impl_->climate_map->sample_blended(world_x, world_z, static_cast<float>(height));
}

void TerrainGenerator::set_erosion_context(ErosionContext* context) {
    impl_->erosion_context = context;
}

ErosionContext* TerrainGenerator::get_erosion_context() const {
    return impl_->erosion_context;
}

}  // namespace realcraft::world
