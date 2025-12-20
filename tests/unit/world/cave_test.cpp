// RealCraft World System Tests
// cave_test.cpp - Tests for CaveGenerator class

#include <gtest/gtest.h>

#include <atomic>
#include <cmath>
#include <realcraft/world/biome.hpp>
#include <realcraft/world/block.hpp>
#include <realcraft/world/cave_generator.hpp>
#include <realcraft/world/chunk.hpp>
#include <realcraft/world/terrain_generator.hpp>
#include <thread>
#include <vector>

namespace realcraft::world {
namespace {

class CaveGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        BlockRegistry::instance().register_defaults();
        BiomeRegistry::instance().register_defaults();
    }
};

// Test: Same seed produces same carving decisions (deterministic)
TEST_F(CaveGeneratorTest, DeterministicCarving) {
    CaveConfig config;
    config.seed = 12345;
    CaveGenerator gen1(config);
    CaveGenerator gen2(config);

    // Same coordinates with same seed should produce identical results
    for (int i = 0; i < 100; ++i) {
        int64_t x = i * 13 - 500;
        int64_t y = 30;
        int64_t z = i * 17 - 400;
        int32_t surface = 80;

        EXPECT_EQ(gen1.should_carve(x, y, z, surface), gen2.should_carve(x, y, z, surface))
            << "Carving mismatch at (" << x << ", " << y << ", " << z << ")";
    }
}

// Test: Different seeds produce different carving patterns
TEST_F(CaveGeneratorTest, DifferentSeedsProduceDifferentCaves) {
    CaveConfig config1;
    config1.seed = 12345;
    CaveConfig config2;
    config2.seed = 54321;

    CaveGenerator gen1(config1);
    CaveGenerator gen2(config2);

    int different_count = 0;
    int32_t surface = 80;

    for (int i = 0; i < 100; ++i) {
        int64_t x = i * 10;
        int64_t y = 30;
        int64_t z = i * 10;

        if (gen1.should_carve(x, y, z, surface) != gen2.should_carve(x, y, z, surface)) {
            ++different_count;
        }
    }

    // At least some should differ
    EXPECT_GT(different_count, 0) << "Different seeds should produce different caves";
}

// Test: No caves above max_y
TEST_F(CaveGeneratorTest, NoCavesAboveMaxY) {
    CaveConfig config;
    config.seed = 12345;
    config.max_y = 100;
    CaveGenerator gen(config);

    int carved_above = 0;
    int32_t surface = 150;  // Surface well above max_y

    for (int x = 0; x < 100; ++x) {
        for (int z = 0; z < 100; ++z) {
            for (int y = 101; y < 150; ++y) {
                if (gen.should_carve(x, y, z, surface)) {
                    ++carved_above;
                }
            }
        }
    }

    EXPECT_EQ(carved_above, 0) << "Should not carve above max_y";
}

// Test: No caves below min_y
TEST_F(CaveGeneratorTest, NoCavesBelowMinY) {
    CaveConfig config;
    config.seed = 12345;
    config.min_y = 10;
    CaveGenerator gen(config);

    int carved_below = 0;
    int32_t surface = 80;

    for (int x = 0; x < 100; ++x) {
        for (int z = 0; z < 100; ++z) {
            for (int y = 0; y < 10; ++y) {
                if (gen.should_carve(x, y, z, surface)) {
                    ++carved_below;
                }
            }
        }
    }

    EXPECT_EQ(carved_below, 0) << "Should not carve below min_y";
}

// Test: No caves too close to surface
TEST_F(CaveGeneratorTest, NoCavesNearSurface) {
    CaveConfig config;
    config.seed = 12345;
    config.surface_margin = 5;
    CaveGenerator gen(config);

    int32_t surface = 60;
    int carved_near_surface = 0;

    for (int x = 0; x < 100; ++x) {
        for (int z = 0; z < 100; ++z) {
            for (int y = surface - 4; y <= surface; ++y) {
                if (gen.should_carve(x, y, z, surface)) {
                    ++carved_near_surface;
                }
            }
        }
    }

    EXPECT_EQ(carved_near_surface, 0) << "Should not carve within surface_margin of surface";
}

// Test: Caves exist underground
TEST_F(CaveGeneratorTest, CavesExistUnderground) {
    CaveConfig config;
    config.seed = 12345;
    CaveGenerator gen(config);

    int carved = 0;
    int32_t surface = 80;

    for (int x = 0; x < 100; ++x) {
        for (int z = 0; z < 100; ++z) {
            for (int y = config.min_y; y < surface - config.surface_margin; ++y) {
                if (gen.should_carve(x, y, z, surface)) {
                    ++carved;
                }
            }
        }
    }

    EXPECT_GT(carved, 0) << "Should find caves underground";
}

// Test: Caves are not everywhere (not all blocks carved)
TEST_F(CaveGeneratorTest, CavesAreNotEverywhere) {
    CaveConfig config;
    config.seed = 12345;
    config.worm.threshold = 0.4f;    // Lower threshold for fewer caves
    config.chamber.enabled = false;  // Disable chambers for this test
    CaveGenerator gen(config);

    int carved = 0;
    int total = 0;
    int32_t surface = 80;

    // Sample a larger area with larger steps to get more variation
    for (int x = 0; x < 200; x += 4) {
        for (int z = 0; z < 200; z += 4) {
            for (int y = 20; y < 60; y += 2) {
                ++total;
                if (gen.should_carve(x, y, z, surface)) {
                    ++carved;
                }
            }
        }
    }

    EXPECT_LT(carved, total) << "Not all blocks should be caves";
    EXPECT_GT(carved, 0) << "Some blocks should be caves";
}

// Test: Thread safety - concurrent carving checks
TEST_F(CaveGeneratorTest, ThreadSafety) {
    CaveConfig config;
    config.seed = 12345;
    CaveGenerator gen(config);

    std::atomic<int> total_carved{0};
    std::vector<std::thread> threads;
    int32_t surface = 80;

    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&gen, &total_carved, t, surface]() {
            int local_carved = 0;
            for (int i = 0; i < 1000; ++i) {
                int64_t x = t * 1000 + i;
                int64_t y = 30;
                int64_t z = i;
                if (gen.should_carve(x, y, z, surface)) {
                    ++local_carved;
                }
            }
            total_carved += local_carved;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should have found some caves across all threads
    EXPECT_GT(total_carved.load(), 0);
}

// Test: Chamber generation works
TEST_F(CaveGeneratorTest, ChambersExist) {
    CaveConfig config;
    config.seed = 12345;
    config.chamber.enabled = true;
    config.chamber.min_y = 10;
    config.chamber.max_y = 50;
    CaveGenerator gen(config);

    int chambers = 0;

    for (int x = 0; x < 200; ++x) {
        for (int z = 0; z < 200; ++z) {
            for (int y = config.chamber.min_y; y <= config.chamber.max_y; ++y) {
                if (gen.is_chamber(x, y, z)) {
                    ++chambers;
                }
            }
        }
    }

    EXPECT_GT(chambers, 0) << "Should find chambers underground";
}

// Test: Chambers only in Y range
TEST_F(CaveGeneratorTest, ChambersOnlyInYRange) {
    CaveConfig config;
    config.seed = 12345;
    config.chamber.enabled = true;
    config.chamber.min_y = 20;
    config.chamber.max_y = 40;
    CaveGenerator gen(config);

    int chambers_outside = 0;

    for (int x = 0; x < 100; ++x) {
        for (int z = 0; z < 100; ++z) {
            // Check below min_y
            for (int y = 0; y < config.chamber.min_y; ++y) {
                if (gen.is_chamber(x, y, z)) {
                    ++chambers_outside;
                }
            }
            // Check above max_y
            for (int y = config.chamber.max_y + 1; y < 100; ++y) {
                if (gen.is_chamber(x, y, z)) {
                    ++chambers_outside;
                }
            }
        }
    }

    EXPECT_EQ(chambers_outside, 0) << "Chambers should only exist in configured Y range";
}

// Test: Water table flooding works
TEST_F(CaveGeneratorTest, WaterTableFlooding) {
    CaveConfig config;
    config.seed = 12345;
    config.water.enabled = true;
    config.water.level = 30;
    config.water.flood_chance = 0.5f;  // 50% chance
    CaveGenerator gen(config);

    int flooded = 0;
    int not_flooded = 0;

    for (int x = 0; x < 100; ++x) {
        for (int z = 0; z < 100; ++z) {
            for (int y = 10; y <= config.water.level; ++y) {
                if (gen.is_flooded(x, y, z)) {
                    ++flooded;
                } else {
                    ++not_flooded;
                }
            }
        }
    }

    // With 50% flood chance, should have some flooded and some not
    EXPECT_GT(flooded, 0) << "Some areas should be flooded";
    EXPECT_GT(not_flooded, 0) << "Some areas should not be flooded";
}

// Test: No flooding above water table
TEST_F(CaveGeneratorTest, NoFloodingAboveWaterTable) {
    CaveConfig config;
    config.seed = 12345;
    config.water.enabled = true;
    config.water.level = 30;
    config.water.flood_chance = 1.0f;  // Always flood below
    CaveGenerator gen(config);

    int flooded_above = 0;

    for (int x = 0; x < 100; ++x) {
        for (int z = 0; z < 100; ++z) {
            for (int y = config.water.level + 1; y < 80; ++y) {
                if (gen.is_flooded(x, y, z)) {
                    ++flooded_above;
                }
            }
        }
    }

    EXPECT_EQ(flooded_above, 0) << "Should not flood above water table";
}

// Test: Decoration placement is deterministic
TEST_F(CaveGeneratorTest, DecorationsDeterministic) {
    CaveConfig config;
    config.seed = 12345;
    config.decorations.enabled = true;
    CaveGenerator gen1(config);
    CaveGenerator gen2(config);

    for (int i = 0; i < 100; ++i) {
        int64_t x = i * 7;
        int64_t y = 30;
        int64_t z = i * 11;

        EXPECT_EQ(gen1.should_place_stalactite(x, y, z), gen2.should_place_stalactite(x, y, z));
        EXPECT_EQ(gen1.should_place_stalagmite(x, y, z), gen2.should_place_stalagmite(x, y, z));
        EXPECT_EQ(gen1.should_place_crystal(x, y, z), gen2.should_place_crystal(x, y, z));
        EXPECT_EQ(gen1.should_place_moss(x, y, z), gen2.should_place_moss(x, y, z));
        EXPECT_EQ(gen1.should_place_mushroom(x, y, z), gen2.should_place_mushroom(x, y, z));
    }
}

// Test: Decorations respect chances
TEST_F(CaveGeneratorTest, DecorationsRespectChances) {
    CaveConfig config;
    config.seed = 12345;
    config.decorations.enabled = true;
    config.decorations.stalactite_chance = 0.08f;
    config.decorations.stalagmite_chance = 0.06f;
    config.decorations.crystal_chance = 0.02f;
    CaveGenerator gen(config);

    int stalactites = 0;
    int stalagmites = 0;
    int crystals = 0;
    int total = 10000;

    for (int i = 0; i < total; ++i) {
        int64_t x = i % 100;
        int64_t y = 20;  // Deep enough for crystals
        int64_t z = i / 100;

        if (gen.should_place_stalactite(x, y, z))
            ++stalactites;
        if (gen.should_place_stalagmite(x, y, z))
            ++stalagmites;
        if (gen.should_place_crystal(x, y, z))
            ++crystals;
    }

    // Check approximate distribution (with tolerance for randomness)
    // Expected: stalactites ~800, stalagmites ~600, crystals ~200
    EXPECT_GT(stalactites, 400);  // Should be roughly 8%
    EXPECT_LT(stalactites, 1200);
    EXPECT_GT(stalagmites, 300);  // Should be roughly 6%
    EXPECT_LT(stalagmites, 900);
    EXPECT_GT(crystals, 50);  // Should be roughly 2%
    EXPECT_LT(crystals, 400);
}

// Test: Crystals only in deep caves
TEST_F(CaveGeneratorTest, CrystalsOnlyDeep) {
    CaveConfig config;
    config.seed = 12345;
    config.decorations.enabled = true;
    config.decorations.crystal_chance = 0.1f;  // High chance for testing
    CaveGenerator gen(config);

    int crystals_deep = 0;
    int crystals_shallow = 0;

    for (int x = 0; x < 100; ++x) {
        for (int z = 0; z < 100; ++z) {
            // Deep (y < 40)
            if (gen.should_place_crystal(x, 20, z)) {
                ++crystals_deep;
            }
            // Shallow (y >= 40)
            if (gen.should_place_crystal(x, 60, z)) {
                ++crystals_shallow;
            }
        }
    }

    EXPECT_GT(crystals_deep, 0) << "Should find crystals in deep caves";
    EXPECT_EQ(crystals_shallow, 0) << "Should not find crystals in shallow caves";
}

// Test: Damp areas near water table
TEST_F(CaveGeneratorTest, DampAreasNearWater) {
    CaveConfig config;
    config.seed = 12345;
    config.water.enabled = true;
    config.water.level = 30;
    config.decorations.damp_radius = 8;
    CaveGenerator gen(config);

    // Check area right at water level
    EXPECT_TRUE(gen.is_damp_area(0, config.water.level, 0));

    // Check area just above water level (within damp radius)
    EXPECT_TRUE(gen.is_damp_area(0, config.water.level + 5, 0));

    // Check area well above water level (outside damp radius)
    EXPECT_FALSE(gen.is_damp_area(0, config.water.level + 20, 0));
}

// Test: Configuration changes take effect after rebuild
TEST_F(CaveGeneratorTest, ConfigChangeAfterRebuild) {
    CaveConfig config;
    config.seed = 111;
    config.worm.threshold = 0.2f;  // Very low threshold (few caves)
    config.chamber.enabled = false;
    CaveGenerator gen(config);

    int carved1 = 0;
    int32_t surface = 80;
    // Sample a larger area with more spread
    for (int x = 0; x < 200; x += 4) {
        for (int z = 0; z < 200; z += 4) {
            if (gen.should_carve(x, 30, z, surface)) {
                ++carved1;
            }
        }
    }

    // Change config and rebuild with much higher threshold (more caves)
    CaveConfig new_config;
    new_config.seed = 111;
    new_config.worm.threshold = 0.8f;  // High threshold (many caves)
    new_config.chamber.enabled = false;
    gen.set_config(new_config);
    gen.rebuild_nodes();

    int carved2 = 0;
    for (int x = 0; x < 200; x += 4) {
        for (int z = 0; z < 200; z += 4) {
            if (gen.should_carve(x, 30, z, surface)) {
                ++carved2;
            }
        }
    }

    // Higher threshold should result in more caves
    EXPECT_GT(carved2, carved1) << "Higher threshold should produce more caves";
}

// Test: Disabled caves produce no carving
TEST_F(CaveGeneratorTest, DisabledCavesNoCarving) {
    CaveConfig config;
    config.seed = 12345;
    config.enabled = false;
    CaveGenerator gen(config);

    int carved = 0;
    int32_t surface = 80;

    for (int x = 0; x < 100; ++x) {
        for (int z = 0; z < 100; ++z) {
            for (int y = 10; y < 70; ++y) {
                if (gen.should_carve(x, y, z, surface)) {
                    ++carved;
                }
            }
        }
    }

    EXPECT_EQ(carved, 0) << "Disabled caves should not carve anything";
}

// ============================================================================
// Integration Tests with TerrainGenerator
// ============================================================================

TEST_F(CaveGeneratorTest, CavesIntegratedWithTerrain) {
    TerrainConfig config;
    config.seed = 12345;
    config.caves.enabled = true;
    config.caves.worm.threshold = 0.6f;  // Moderate cave density
    TerrainGenerator gen(config);

    ChunkDesc desc;
    desc.position = ChunkPos(0, 0);
    Chunk chunk(desc);

    gen.generate(chunk);

    // Count air blocks underground (should be more than without caves)
    int underground_air = 0;
    for (int y = 10; y < 50; ++y) {
        for (int x = 0; x < CHUNK_SIZE_X; ++x) {
            for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
                if (chunk.get_block(LocalBlockPos(x, y, z)) == BLOCK_AIR) {
                    ++underground_air;
                }
            }
        }
    }

    EXPECT_GT(underground_air, 0) << "Should have cave air underground with caves enabled";
}

TEST_F(CaveGeneratorTest, NoCavesWhenDisabled) {
    TerrainConfig config;
    config.seed = 12345;
    config.caves.enabled = false;
    config.density.enabled = false;  // Also disable density caves
    TerrainGenerator gen(config);

    ChunkDesc desc;
    desc.position = ChunkPos(0, 0);
    Chunk chunk(desc);

    gen.generate(chunk);

    // Count air blocks deep underground (should be minimal)
    int underground_air = 0;
    for (int y = 10; y < 30; ++y) {  // Check very deep
        for (int x = 0; x < CHUNK_SIZE_X; ++x) {
            for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
                if (chunk.get_block(LocalBlockPos(x, y, z)) == BLOCK_AIR) {
                    ++underground_air;
                }
            }
        }
    }

    // Should have no or very few air blocks deep underground
    EXPECT_EQ(underground_air, 0) << "Should have no cave air when caves disabled";
}

TEST_F(CaveGeneratorTest, CaveBlockTypesRegistered) {
    const auto& registry = BlockRegistry::instance();

    // Verify cave block types are registered
    EXPECT_TRUE(registry.find_id("realcraft:stalactite").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:stalagmite").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:crystal").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:cave_moss").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:glowing_mushroom").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:dripstone").has_value());
}

}  // namespace
}  // namespace realcraft::world
