// RealCraft World System Tests
// terrain_generator_test.cpp - Tests for TerrainGenerator class

#include <gtest/gtest.h>

#include <atomic>
#include <cmath>
#include <realcraft/world/biome.hpp>
#include <realcraft/world/block.hpp>
#include <realcraft/world/chunk.hpp>
#include <realcraft/world/climate.hpp>
#include <realcraft/world/terrain_generator.hpp>
#include <set>
#include <thread>
#include <vector>

namespace realcraft::world {
namespace {

class TerrainGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        BlockRegistry::instance().register_defaults();
        BiomeRegistry::instance().register_defaults();
    }
};

// Test: Same seed produces same height (deterministic)
TEST_F(TerrainGeneratorTest, DeterministicHeight) {
    TerrainConfig config;
    config.seed = 12345;
    TerrainGenerator gen1(config);
    TerrainGenerator gen2(config);

    // Same coordinates with same seed should produce identical heights
    EXPECT_EQ(gen1.get_height(100, 200), gen2.get_height(100, 200));
    EXPECT_EQ(gen1.get_height(-500, 750), gen2.get_height(-500, 750));
    EXPECT_EQ(gen1.get_height(0, 0), gen2.get_height(0, 0));
    EXPECT_EQ(gen1.get_height(999999, -999999), gen2.get_height(999999, -999999));
}

// Test: Different seeds produce different heights
TEST_F(TerrainGeneratorTest, DifferentSeedsProduceDifferentTerrain) {
    TerrainConfig config1;
    config1.seed = 12345;
    TerrainConfig config2;
    config2.seed = 54321;

    TerrainGenerator gen1(config1);
    TerrainGenerator gen2(config2);

    // Heights should differ with different seeds (statistically unlikely to be equal)
    // Test at multiple points to ensure this isn't a coincidence
    int different_count = 0;
    for (int i = 0; i < 10; ++i) {
        if (gen1.get_height(i * 100, i * 50) != gen2.get_height(i * 100, i * 50)) {
            ++different_count;
        }
    }
    EXPECT_GE(different_count, 8);  // At least 8 out of 10 should differ
}

// Test: Height is within configured bounds
TEST_F(TerrainGeneratorTest, HeightWithinBounds) {
    TerrainConfig config;
    config.seed = 42;
    config.min_height = 10;
    config.max_height = 150;
    TerrainGenerator gen(config);

    // Sample many points and verify all are within bounds
    for (int i = 0; i < 1000; ++i) {
        int64_t x = static_cast<int64_t>(i) * 7 - 500;
        int64_t z = static_cast<int64_t>(i) * 13 - 300;
        int32_t height = gen.get_height(x, z);
        EXPECT_GE(height, config.min_height) << "Height below min at (" << x << ", " << z << ")";
        EXPECT_LE(height, config.max_height) << "Height above max at (" << x << ", " << z << ")";
    }
}

// Test: Default config produces reasonable heights
TEST_F(TerrainGeneratorTest, DefaultConfigProducesReasonableHeights) {
    TerrainConfig config;
    config.seed = 123;
    TerrainGenerator gen(config);

    int32_t min_found = INT32_MAX;
    int32_t max_found = INT32_MIN;
    int64_t sum = 0;

    // Sample a larger grid of heights for better variation detection
    for (int x = -500; x < 500; x += 10) {
        for (int z = -500; z < 500; z += 10) {
            int32_t height = gen.get_height(x, z);
            min_found = std::min(min_found, height);
            max_found = std::max(max_found, height);
            sum += height;
        }
    }

    int count = 10000;  // 100 * 100 samples
    int32_t avg = static_cast<int32_t>(sum / count);

    // Should have some variation (at least 5 blocks over large area)
    EXPECT_GE(max_found - min_found, 5) << "Height variation too low";

    // Average should be roughly near base_height (within height_variation)
    EXPECT_GE(avg, config.base_height - config.height_variation);
    EXPECT_LE(avg, config.base_height + config.height_variation);
}

// Test: Chunk generation produces solid blocks
TEST_F(TerrainGeneratorTest, ChunkHasBlocks) {
    TerrainConfig config;
    config.seed = 12345;
    TerrainGenerator gen(config);

    ChunkDesc desc;
    desc.position = ChunkPos(0, 0);
    Chunk chunk(desc);

    gen.generate(chunk);

    EXPECT_GT(chunk.non_air_count(), 0u);
    EXPECT_TRUE(chunk.get_metadata().has_been_generated);
}

// Test: Generated chunk has expected block types
TEST_F(TerrainGeneratorTest, ChunkHasCorrectBlockTypes) {
    TerrainConfig config;
    config.seed = 12345;
    TerrainGenerator gen(config);

    ChunkDesc desc;
    desc.position = ChunkPos(0, 0);
    Chunk chunk(desc);

    gen.generate(chunk);

    const auto& registry = BlockRegistry::instance();
    BlockId stone_id = registry.stone_id();
    BlockId dirt_id = registry.dirt_id();
    BlockId grass_id = registry.grass_id();

    // Check that we have stone, dirt, and grass blocks
    bool has_stone = false;
    bool has_dirt = false;
    bool has_grass = false;

    for (int x = 0; x < CHUNK_SIZE_X && !(has_stone && has_dirt && has_grass); ++x) {
        for (int z = 0; z < CHUNK_SIZE_Z && !(has_stone && has_dirt && has_grass); ++z) {
            for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
                BlockId block = chunk.get_block(LocalBlockPos(x, y, z));
                if (block == stone_id)
                    has_stone = true;
                if (block == dirt_id)
                    has_dirt = true;
                if (block == grass_id)
                    has_grass = true;
            }
        }
    }

    EXPECT_TRUE(has_stone) << "No stone blocks found";
    EXPECT_TRUE(has_dirt) << "No dirt blocks found";
    EXPECT_TRUE(has_grass) << "No grass blocks found";
}

// Test: Adjacent chunks have continuous terrain (no seams)
TEST_F(TerrainGeneratorTest, ChunkBorderContinuity) {
    TerrainConfig config;
    config.seed = 42;
    TerrainGenerator gen(config);

    // Check heights at chunk border
    // If x is at CHUNK_SIZE_X - 1 in chunk 0, x+1 is at 0 in chunk 1
    for (int z = 0; z < 100; z += 10) {
        int64_t border_x = CHUNK_SIZE_X - 1;
        int32_t h1 = gen.get_height(border_x, z);
        int32_t h2 = gen.get_height(border_x + 1, z);

        // Heights should be similar at border (within reasonable tolerance)
        // Terrain can change, but not drastically between adjacent blocks
        EXPECT_LE(std::abs(h1 - h2), 3) << "Height discontinuity at z=" << z;
    }

    // Same test for Z border
    for (int x = 0; x < 100; x += 10) {
        int64_t border_z = CHUNK_SIZE_Z - 1;
        int32_t h1 = gen.get_height(x, border_z);
        int32_t h2 = gen.get_height(x, border_z + 1);

        EXPECT_LE(std::abs(h1 - h2), 3) << "Height discontinuity at x=" << x;
    }
}

// Test: Thread safety - multiple threads can generate concurrently
TEST_F(TerrainGeneratorTest, ThreadSafeGeneration) {
    TerrainConfig config;
    config.seed = 12345;
    TerrainGenerator gen(config);

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&gen, &success_count, i]() {
            ChunkDesc desc;
            desc.position = ChunkPos(i * 10, i * 10);
            Chunk chunk(desc);

            gen.generate(chunk);

            if (chunk.non_air_count() > 0) {
                ++success_count;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), 4);
}

// Test: Thread safety - concurrent height queries
TEST_F(TerrainGeneratorTest, ThreadSafeHeightQueries) {
    TerrainConfig config;
    config.seed = 99999;
    TerrainGenerator gen(config);

    // Pre-compute expected heights
    std::vector<int32_t> expected_heights(100);
    for (int i = 0; i < 100; ++i) {
        expected_heights[i] = gen.get_height(i * 100, i * 100);
    }

    std::atomic<bool> all_match{true};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&gen, &expected_heights, &all_match]() {
            for (int i = 0; i < 100; ++i) {
                int32_t height = gen.get_height(i * 100, i * 100);
                if (height != expected_heights[i]) {
                    all_match = false;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_TRUE(all_match.load()) << "Concurrent height queries returned inconsistent results";
}

// Test: Density produces caves below surface
TEST_F(TerrainGeneratorTest, DensityCreatesCaves) {
    TerrainConfig config;
    config.seed = 12345;
    config.density.enabled = true;
    config.density.surface_blend = 8.0f;  // Caves start 8 blocks below surface
    TerrainGenerator gen(config);

    // Sample many points deep underground looking for caves (negative density)
    // Need to sample deep enough that the surface blend allows caves
    int cave_count = 0;
    int sample_count = 0;

    for (int x = 0; x < 200; x += 5) {
        for (int z = 0; z < 200; z += 5) {
            int32_t surface = gen.get_height(x, z);
            // Sample deep underground (below surface_blend distance)
            for (int y = 5; y < surface - 15; ++y) {
                float density = gen.get_density(x, y, z);
                if (density < 0) {
                    ++cave_count;
                }
                ++sample_count;
            }
        }
    }

    // With enough sampling, we should find some caves
    // The raw noise is roughly [-1, 1], so about half should be caves at depth
    EXPECT_GT(cave_count, 0) << "No caves found in " << sample_count << " samples";
    EXPECT_LT(cave_count, sample_count) << "All samples are caves - something is wrong";
}

// Test: Surface is solid when density is enabled
TEST_F(TerrainGeneratorTest, SurfaceRemainsSolidWithDensity) {
    TerrainConfig config;
    config.seed = 777;
    config.density.enabled = true;
    TerrainGenerator gen(config);

    // Surface blocks should be solid (positive density)
    for (int x = 0; x < 50; x += 5) {
        for (int z = 0; z < 50; z += 5) {
            int32_t surface = gen.get_height(x, z);
            float density = gen.get_density(x, surface, z);
            EXPECT_GT(density, 0) << "Surface not solid at (" << x << ", " << surface << ", " << z << ")";
        }
    }
}

// Test: Heightmap generation matches individual height queries
TEST_F(TerrainGeneratorTest, HeightmapMatchesIndividualQueries) {
    TerrainConfig config;
    config.seed = 555;
    TerrainGenerator gen(config);

    ChunkPos chunk_pos(3, -2);
    auto heightmap = gen.generate_heightmap(chunk_pos);

    int64_t base_x = static_cast<int64_t>(chunk_pos.x) * CHUNK_SIZE_X;
    int64_t base_z = static_cast<int64_t>(chunk_pos.y) * CHUNK_SIZE_Z;

    for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
        for (int x = 0; x < CHUNK_SIZE_X; ++x) {
            int32_t from_map = heightmap[z * CHUNK_SIZE_X + x];
            int32_t from_query = gen.get_height(base_x + x, base_z + z);
            EXPECT_EQ(from_map, from_query) << "Mismatch at local (" << x << ", " << z << ")";
        }
    }
}

// Test: Configuration changes take effect after rebuild
TEST_F(TerrainGeneratorTest, ConfigChangeAfterRebuild) {
    TerrainConfig config;
    config.seed = 111;
    config.base_height = 50;
    TerrainGenerator gen(config);

    int32_t h1 = gen.get_height(0, 0);

    // Change config and rebuild
    TerrainConfig new_config;
    new_config.seed = 222;  // Different seed
    new_config.base_height = 100;
    gen.set_config(new_config);
    gen.rebuild_nodes();

    int32_t h2 = gen.get_height(0, 0);

    // Height should be different after config change
    EXPECT_NE(h1, h2);
}

// Test: Negative chunk positions work correctly
TEST_F(TerrainGeneratorTest, NegativeChunkPositions) {
    TerrainConfig config;
    config.seed = 888;
    TerrainGenerator gen(config);

    ChunkDesc desc;
    desc.position = ChunkPos(-5, -10);
    Chunk chunk(desc);

    gen.generate(chunk);

    EXPECT_GT(chunk.non_air_count(), 0u);
    EXPECT_TRUE(chunk.get_metadata().has_been_generated);
}

// Test: Large coordinate values work without overflow
TEST_F(TerrainGeneratorTest, LargeCoordinates) {
    TerrainConfig config;
    config.seed = 999;
    TerrainGenerator gen(config);

    // Test with large but valid coordinates
    int64_t large_x = 1000000;
    int64_t large_z = -1000000;

    int32_t height = gen.get_height(large_x, large_z);
    EXPECT_GE(height, config.min_height);
    EXPECT_LE(height, config.max_height);

    // Heights should still be deterministic at large coordinates
    EXPECT_EQ(height, gen.get_height(large_x, large_z));
}

// ============================================================================
// Biome System Tests
// ============================================================================

// Test: Chunk has biome_id set after generation
TEST_F(TerrainGeneratorTest, ChunkHasBiomeId) {
    TerrainConfig config;
    config.seed = 12345;
    config.biome_system.enabled = true;
    TerrainGenerator gen(config);

    ChunkDesc desc;
    desc.position = ChunkPos(0, 0);
    Chunk chunk(desc);

    gen.generate(chunk);

    // biome_id should be set (may be any valid biome)
    uint8_t biome_id = chunk.get_metadata().biome_id;
    EXPECT_LT(biome_id, static_cast<uint8_t>(BiomeType::Count));
}

// Test: get_biome returns valid biome
TEST_F(TerrainGeneratorTest, GetBiomeReturnsValid) {
    TerrainConfig config;
    config.seed = 12345;
    config.biome_system.enabled = true;
    TerrainGenerator gen(config);

    for (int i = 0; i < 100; ++i) {
        BiomeType biome = gen.get_biome(i * 100, i * 100);
        EXPECT_LT(static_cast<size_t>(biome), BIOME_COUNT) << "Invalid biome at sample " << i;
    }
}

// Test: get_climate returns valid values
TEST_F(TerrainGeneratorTest, GetClimateReturnsValid) {
    TerrainConfig config;
    config.seed = 12345;
    config.biome_system.enabled = true;
    TerrainGenerator gen(config);

    for (int i = 0; i < 50; ++i) {
        ClimateSample sample = gen.get_climate(i * 200, i * 150);

        EXPECT_GE(sample.temperature, 0.0f);
        EXPECT_LE(sample.temperature, 1.0f);
        EXPECT_GE(sample.humidity, 0.0f);
        EXPECT_LE(sample.humidity, 1.0f);
        EXPECT_LT(static_cast<size_t>(sample.biome), BIOME_COUNT);
    }
}

// Test: get_biome_blend returns valid blend
TEST_F(TerrainGeneratorTest, GetBiomeBlendReturnsValid) {
    TerrainConfig config;
    config.seed = 12345;
    config.biome_system.enabled = true;
    TerrainGenerator gen(config);

    for (int i = 0; i < 50; ++i) {
        BiomeBlend blend = gen.get_biome_blend(i * 200, i * 150);

        EXPECT_LT(static_cast<size_t>(blend.primary_biome), BIOME_COUNT);
        EXPECT_LT(static_cast<size_t>(blend.secondary_biome), BIOME_COUNT);
        EXPECT_GE(blend.blend_factor, 0.0f);
        EXPECT_LE(blend.blend_factor, 1.0f);
    }
}

// Test: Biome system disabled returns default biome
TEST_F(TerrainGeneratorTest, BiomeSystemDisabledReturnsDefault) {
    TerrainConfig config;
    config.seed = 12345;
    config.biome_system.enabled = false;
    TerrainGenerator gen(config);

    BiomeType biome = gen.get_biome(0, 0);
    EXPECT_EQ(biome, BiomeType::Plains);

    ClimateSample sample = gen.get_climate(0, 0);
    EXPECT_FLOAT_EQ(sample.temperature, 0.5f);
    EXPECT_FLOAT_EQ(sample.humidity, 0.5f);
    EXPECT_EQ(sample.biome, BiomeType::Plains);
}

// Test: Multiple biome types are generated across the world
TEST_F(TerrainGeneratorTest, MultipleBiomesGenerated) {
    TerrainConfig config;
    config.seed = 42;
    config.biome_system.enabled = true;
    TerrainGenerator gen(config);

    std::set<BiomeType> found_biomes;

    // Sample across a large area
    for (int x = -5000; x <= 5000; x += 500) {
        for (int z = -5000; z <= 5000; z += 500) {
            BiomeType biome = gen.get_biome(x, z);
            found_biomes.insert(biome);
        }
    }

    // Should find at least 3 different biome types
    EXPECT_GE(found_biomes.size(), 3u) << "Expected to find multiple biome types across world";
}

// Test: Desert chunks have sand surface
TEST_F(TerrainGeneratorTest, DesertHasSandSurface) {
    TerrainConfig config;
    config.seed = 42;
    config.biome_system.enabled = true;
    TerrainGenerator gen(config);

    BlockId sand_id = BlockRegistry::instance().sand_id();

    // Find a desert location by sampling
    int64_t desert_x = 0, desert_z = 0;
    bool found_desert = false;

    for (int x = 0; x < 10000 && !found_desert; x += 100) {
        for (int z = 0; z < 10000 && !found_desert; z += 100) {
            if (gen.get_biome(x, z) == BiomeType::Desert) {
                desert_x = x;
                desert_z = z;
                found_desert = true;
            }
        }
    }

    if (!found_desert) {
        GTEST_SKIP() << "Could not find desert biome in test area";
    }

    // Generate chunk at desert location
    ChunkPos chunk_pos(static_cast<int>(desert_x / CHUNK_SIZE_X), static_cast<int>(desert_z / CHUNK_SIZE_Z));
    ChunkDesc desc;
    desc.position = chunk_pos;
    Chunk chunk(desc);

    gen.generate(chunk);

    // Check that surface blocks are sand
    bool found_sand = false;
    for (int x = 0; x < CHUNK_SIZE_X && !found_sand; ++x) {
        for (int z = 0; z < CHUNK_SIZE_Z && !found_sand; ++z) {
            // Find the surface (highest non-air block)
            for (int y = CHUNK_SIZE_Y - 1; y >= 0; --y) {
                BlockId block = chunk.get_block(LocalBlockPos(x, y, z));
                if (block != BLOCK_AIR) {
                    if (block == sand_id) {
                        found_sand = true;
                    }
                    break;  // Stop at first non-air
                }
            }
        }
    }

    EXPECT_TRUE(found_sand) << "Expected sand blocks in desert chunk";
}

// Test: Biome queries are deterministic
TEST_F(TerrainGeneratorTest, BiomeQueriesDeterministic) {
    TerrainConfig config;
    config.seed = 12345;
    config.biome_system.enabled = true;
    TerrainGenerator gen1(config);
    TerrainGenerator gen2(config);

    for (int i = 0; i < 100; ++i) {
        int64_t x = i * 100 - 5000;
        int64_t z = i * 50 - 2500;

        EXPECT_EQ(gen1.get_biome(x, z), gen2.get_biome(x, z)) << "Biome mismatch at (" << x << ", " << z << ")";
    }
}

}  // namespace
}  // namespace realcraft::world
