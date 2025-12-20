// RealCraft World System
// terrain_generator.cpp - Procedural terrain generation using FastNoise2

#include <FastNoise/FastNoise.h>
#include <algorithm>
#include <cmath>
#include <realcraft/world/biome.hpp>
#include <realcraft/world/block.hpp>
#include <realcraft/world/climate.hpp>
#include <realcraft/world/erosion.hpp>
#include <realcraft/world/erosion_heightmap.hpp>
#include <realcraft/world/terrain_generator.hpp>
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

        // Run erosion simulation
        impl_->erosion_simulator->simulate(erosion_map, impl_->config.erosion, impl_->config.seed);

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

            // Get biome and block palette for this column
            BiomeBlockPalette palette;
            BiomeType biome = BiomeType::Plains;

            if (impl_->climate_map) {
                biome = impl_->climate_map->get_biome(world_x, world_z, static_cast<float>(surface_height));
                palette = biome_registry.get_palette(biome);

                // Handle invalid palette entries by using fallback blocks
                if (palette.surface_block == BLOCK_INVALID)
                    palette.surface_block = fallback_grass_id;
                if (palette.subsurface_block == BLOCK_INVALID)
                    palette.subsurface_block = fallback_dirt_id;
                if (palette.underwater_block == BLOCK_INVALID)
                    palette.underwater_block = fallback_sand_id;
                if (palette.stone_block == BLOCK_INVALID)
                    palette.stone_block = fallback_stone_id;

                // Record center biome for chunk metadata
                if (x == center_x && z == center_z) {
                    chunk_biome = biome;
                }
            } else {
                // Biome system disabled - use fallback palette
                palette.surface_block = fallback_grass_id;
                palette.subsurface_block = fallback_dirt_id;
                palette.underwater_block = fallback_sand_id;
                palette.stone_block = fallback_stone_id;
            }

            // Override surface blocks with sediment blocks if there's significant deposit
            BlockId sediment_surface = palette.surface_block;
            BlockId sediment_subsurface = palette.subsurface_block;
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

                if (density > 0) {
                    // Solid block - determine type based on depth
                    if (y == 0) {
                        // Bottom layer - could be bedrock in future
                        block_id = palette.stone_block;
                    } else if (y < surface_height - impl_->config.dirt_depth) {
                        // Deep underground - stone
                        block_id = palette.stone_block;
                    } else if (y < surface_height) {
                        // Near surface - subsurface layer (use sediment if applicable)
                        block_id = sediment_subsurface;
                    } else if (y == surface_height) {
                        // Surface block - depends on height and sediment
                        if (surface_height <= impl_->config.sea_level) {
                            // At or below sea level - use sediment or underwater block
                            block_id = (sediment > 0) ? sediment_surface : palette.underwater_block;
                        } else {
                            // Above sea level - use sediment or surface block
                            block_id = (sediment > 0) ? sediment_surface : palette.surface_block;
                        }
                    }
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

}  // namespace realcraft::world
