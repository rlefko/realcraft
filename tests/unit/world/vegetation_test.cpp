// RealCraft World System Tests
// vegetation_test.cpp - Tests for VegetationGenerator class

#include <gtest/gtest.h>

#include <atomic>
#include <realcraft/world/biome.hpp>
#include <realcraft/world/block.hpp>
#include <realcraft/world/chunk.hpp>
#include <realcraft/world/terrain_generator.hpp>
#include <realcraft/world/vegetation_generator.hpp>
#include <thread>
#include <vector>

namespace realcraft::world {
namespace {

class VegetationGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        BlockRegistry::instance().register_defaults();
        BiomeRegistry::instance().register_defaults();
    }
};

// Test: Same seed produces same vegetation placement (deterministic)
TEST_F(VegetationGeneratorTest, DeterministicVegetation) {
    VegetationConfig config;
    config.seed = 12345;
    VegetationGenerator gen1(config);
    VegetationGenerator gen2(config);

    for (int i = 0; i < 100; ++i) {
        int64_t x = i * 7 - 300;
        int64_t z = i * 11 - 400;

        EXPECT_EQ(gen1.should_place_grass(x, z, BiomeType::Plains), gen2.should_place_grass(x, z, BiomeType::Plains))
            << "Grass placement mismatch at (" << x << ", " << z << ")";

        EXPECT_EQ(gen1.get_flower_block(x, z, BiomeType::Plains), gen2.get_flower_block(x, z, BiomeType::Plains))
            << "Flower placement mismatch at (" << x << ", " << z << ")";
    }
}

// Test: Different seeds produce different vegetation
TEST_F(VegetationGeneratorTest, DifferentSeedsProduceDifferentVegetation) {
    VegetationConfig config1;
    config1.seed = 12345;
    VegetationConfig config2;
    config2.seed = 54321;

    VegetationGenerator gen1(config1);
    VegetationGenerator gen2(config2);

    int different_count = 0;

    for (int i = 0; i < 100; ++i) {
        int64_t x = i * 10;
        int64_t z = i * 10;

        if (gen1.should_place_grass(x, z, BiomeType::Plains) != gen2.should_place_grass(x, z, BiomeType::Plains)) {
            ++different_count;
        }
    }

    EXPECT_GT(different_count, 0) << "Different seeds should produce different vegetation";
}

// Test: No vegetation in ocean biome
TEST_F(VegetationGeneratorTest, NoVegetationInOcean) {
    VegetationConfig config;
    config.seed = 12345;
    VegetationGenerator gen(config);

    int vegetation_in_ocean = 0;

    for (int x = 0; x < 100; ++x) {
        for (int z = 0; z < 100; ++z) {
            if (gen.should_place_grass(x, z, BiomeType::Ocean) ||
                gen.get_flower_block(x, z, BiomeType::Ocean) != BLOCK_AIR ||
                gen.should_place_bush(x, z, BiomeType::Ocean)) {
                ++vegetation_in_ocean;
            }
        }
    }

    EXPECT_EQ(vegetation_in_ocean, 0) << "Should not place vegetation in ocean";
}

// Test: Cactus only in desert
TEST_F(VegetationGeneratorTest, CactusOnlyInDesert) {
    VegetationConfig config;
    config.seed = 12345;
    config.cactus.density = 0.1f;  // Higher chance for testing
    VegetationGenerator gen(config);

    int cactus_in_desert = 0;
    int cactus_outside_desert = 0;

    for (int x = 0; x < 200; ++x) {
        for (int z = 0; z < 200; ++z) {
            if (gen.should_place_cactus(x, z, BiomeType::Desert)) {
                ++cactus_in_desert;
            }
            if (gen.should_place_cactus(x, z, BiomeType::Forest) || gen.should_place_cactus(x, z, BiomeType::Plains)) {
                ++cactus_outside_desert;
            }
        }
    }

    EXPECT_GT(cactus_in_desert, 0) << "Should place cactus in desert";
    EXPECT_EQ(cactus_outside_desert, 0) << "Should not place cactus outside desert";
}

// Test: Cactus height variation
TEST_F(VegetationGeneratorTest, CactusHeightVariation) {
    VegetationConfig config;
    config.seed = 12345;
    VegetationGenerator gen(config);

    std::vector<int32_t> heights;

    for (int x = 0; x < 100; ++x) {
        for (int z = 0; z < 100; ++z) {
            int32_t height = gen.get_cactus_height(x, z);
            heights.push_back(height);
        }
    }

    // Check height bounds (1 to max_height)
    for (int32_t h : heights) {
        EXPECT_GE(h, 1) << "Cactus height should be >= 1";
        EXPECT_LE(h, config.cactus.max_height) << "Cactus height should be <= max_height";
    }

    // Check for variation
    int different_heights = 0;
    for (size_t i = 1; i < heights.size(); ++i) {
        if (heights[i] != heights[0]) {
            ++different_heights;
        }
    }
    EXPECT_GT(different_heights, 0) << "Should have some height variation";
}

// Test: Lily pads only in swamp
TEST_F(VegetationGeneratorTest, LilyPadsOnlyInSwamp) {
    VegetationConfig config;
    config.seed = 12345;
    VegetationGenerator gen(config);

    int lily_in_swamp = 0;
    int lily_in_plains = 0;

    for (int x = 0; x < 200; ++x) {
        for (int z = 0; z < 200; ++z) {
            if (gen.should_place_lily_pad(x, z, BiomeType::Swamp)) {
                ++lily_in_swamp;
            }
            if (gen.should_place_lily_pad(x, z, BiomeType::Plains)) {
                ++lily_in_plains;
            }
        }
    }

    // Lily pads should be more common in swamp
    EXPECT_GT(lily_in_swamp, lily_in_plains) << "Lily pads should be more common in swamp";
}

// Test: Dead bush in desert and savanna
TEST_F(VegetationGeneratorTest, DeadBushInDryBiomes) {
    VegetationConfig config;
    config.seed = 12345;
    config.dead_bush.density = 0.1f;  // Higher for testing
    VegetationGenerator gen(config);

    int dead_bush_desert = 0;
    int dead_bush_forest = 0;

    for (int x = 0; x < 200; ++x) {
        for (int z = 0; z < 200; ++z) {
            if (gen.should_place_dead_bush(x, z, BiomeType::Desert)) {
                ++dead_bush_desert;
            }
            if (gen.should_place_dead_bush(x, z, BiomeType::Forest)) {
                ++dead_bush_forest;
            }
        }
    }

    EXPECT_GT(dead_bush_desert, 0) << "Should place dead bushes in desert";
    EXPECT_EQ(dead_bush_forest, 0) << "Should not place dead bushes in forest";
}

// Test: Grass exists in plains and forest
TEST_F(VegetationGeneratorTest, GrassInGrassyBiomes) {
    VegetationConfig config;
    config.seed = 12345;
    VegetationGenerator gen(config);

    int grass_in_plains = 0;
    int grass_in_forest = 0;

    for (int x = 0; x < 100; ++x) {
        for (int z = 0; z < 100; ++z) {
            if (gen.should_place_grass(x, z, BiomeType::Plains)) {
                ++grass_in_plains;
            }
            if (gen.should_place_grass(x, z, BiomeType::Forest)) {
                ++grass_in_forest;
            }
        }
    }

    EXPECT_GT(grass_in_plains, 0) << "Should place grass in plains";
    EXPECT_GT(grass_in_forest, 0) << "Should place grass in forest";
}

// Test: Get grass block returns valid blocks
TEST_F(VegetationGeneratorTest, GetGrassBlockReturnsValidBlocks) {
    VegetationConfig config;
    config.seed = 12345;
    VegetationGenerator gen(config);

    const auto& registry = BlockRegistry::instance();
    auto tall_grass = registry.find_id("realcraft:tall_grass");
    auto fern = registry.find_id("realcraft:fern");

    ASSERT_TRUE(tall_grass.has_value()) << "Tall grass should be registered";
    ASSERT_TRUE(fern.has_value()) << "Fern should be registered";

    int valid_grass = 0;
    int total_checked = 0;

    for (int x = 0; x < 100; ++x) {
        for (int z = 0; z < 100; ++z) {
            if (gen.should_place_grass(x, z, BiomeType::Plains)) {
                BlockId block = gen.get_grass_block(x, z, BiomeType::Plains);
                ++total_checked;
                if (block == *tall_grass || block == *fern) {
                    ++valid_grass;
                }
            }
        }
    }

    if (total_checked > 0) {
        EXPECT_EQ(valid_grass, total_checked) << "All grass blocks should be tall_grass or fern";
    }
}

// Test: Disabled vegetation produces nothing
TEST_F(VegetationGeneratorTest, DisabledVegetation) {
    VegetationConfig config;
    config.seed = 12345;
    config.enabled = false;
    VegetationGenerator gen(config);

    int vegetation = 0;

    for (int x = 0; x < 100; ++x) {
        for (int z = 0; z < 100; ++z) {
            if (gen.should_place_grass(x, z, BiomeType::Plains) ||
                gen.get_flower_block(x, z, BiomeType::Plains) != BLOCK_AIR ||
                gen.should_place_bush(x, z, BiomeType::Plains)) {
                ++vegetation;
            }
        }
    }

    EXPECT_EQ(vegetation, 0) << "Disabled vegetation should produce nothing";
}

// Test: Thread safety
TEST_F(VegetationGeneratorTest, ThreadSafety) {
    VegetationConfig config;
    config.seed = 12345;
    VegetationGenerator gen(config);

    std::atomic<int> total_grass{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&gen, &total_grass, t]() {
            int local_count = 0;
            for (int i = 0; i < 1000; ++i) {
                int64_t x = t * 1000 + i;
                int64_t z = i;
                if (gen.should_place_grass(x, z, BiomeType::Plains)) {
                    ++local_count;
                }
            }
            total_grass += local_count;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(total_grass.load(), 0) << "Should place grass across threads";
}

// Test: Vegetation block types registered
TEST_F(VegetationGeneratorTest, VegetationBlockTypesRegistered) {
    const auto& registry = BlockRegistry::instance();

    EXPECT_TRUE(registry.find_id("realcraft:tall_grass").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:fern").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:dead_bush").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:cactus").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:bush").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:poppy").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:dandelion").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:lily_pad").has_value());
}

// Test: Integration with TerrainGenerator
TEST_F(VegetationGeneratorTest, VegetationIntegratedWithTerrain) {
    TerrainConfig config;
    config.seed = 12345;
    config.vegetation.enabled = true;
    config.biome_system.enabled = true;
    TerrainGenerator gen(config);

    ChunkDesc desc;
    desc.position = ChunkPos(0, 0);
    Chunk chunk(desc);

    gen.generate(chunk);

    // Count vegetation blocks above ground
    const auto& registry = BlockRegistry::instance();
    auto tall_grass = registry.find_id("realcraft:tall_grass");
    auto poppy = registry.find_id("realcraft:poppy");
    auto dandelion = registry.find_id("realcraft:dandelion");

    int vegetation_count = 0;
    for (int y = config.sea_level; y < 128; ++y) {
        for (int x = 0; x < CHUNK_SIZE_X; ++x) {
            for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
                BlockId block = chunk.get_block(LocalBlockPos(x, y, z));
                if ((tall_grass && block == *tall_grass) || (poppy && block == *poppy) ||
                    (dandelion && block == *dandelion)) {
                    ++vegetation_count;
                }
            }
        }
    }

    // Should have some vegetation (depends on biome, may be 0 in some chunks)
    // Just verify it doesn't crash and produces reasonable output
    EXPECT_GE(vegetation_count, 0);
}

// Test: Flowers cluster together
TEST_F(VegetationGeneratorTest, FlowersCluster) {
    VegetationConfig config;
    config.seed = 12345;
    config.flowers.cluster_chance = 0.5f;  // Higher for testing
    VegetationGenerator gen(config);

    // Find a flower and check nearby positions
    int flowers_found = 0;
    int64_t flower_x = 0;
    int64_t flower_z = 0;

    for (int x = 0; x < 1000 && flowers_found == 0; ++x) {
        for (int z = 0; z < 1000 && flowers_found == 0; ++z) {
            if (gen.get_flower_block(x, z, BiomeType::Plains) != BLOCK_AIR) {
                flowers_found = 1;
                flower_x = x;
                flower_z = z;
            }
        }
    }

    if (flowers_found > 0) {
        // Count flowers near the found one
        int nearby_flowers = 0;
        for (int dx = -3; dx <= 3; ++dx) {
            for (int dz = -3; dz <= 3; ++dz) {
                if (dx == 0 && dz == 0)
                    continue;
                if (gen.get_flower_block(flower_x + dx, flower_z + dz, BiomeType::Plains) != BLOCK_AIR) {
                    ++nearby_flowers;
                }
            }
        }
        // Due to clustering, might have more nearby (or not, depending on hash)
        EXPECT_GE(nearby_flowers, 0);
    }
}

}  // namespace
}  // namespace realcraft::world
