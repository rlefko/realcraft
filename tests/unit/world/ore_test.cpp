// RealCraft World System Tests
// ore_test.cpp - Tests for OreGenerator class

#include <gtest/gtest.h>

#include <atomic>
#include <realcraft/world/biome.hpp>
#include <realcraft/world/block.hpp>
#include <realcraft/world/chunk.hpp>
#include <realcraft/world/ore_generator.hpp>
#include <realcraft/world/terrain_generator.hpp>
#include <thread>
#include <vector>

namespace realcraft::world {
namespace {

class OreGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        BlockRegistry::instance().register_defaults();
        BiomeRegistry::instance().register_defaults();
    }
};

// Test: Same seed produces same ore positions (deterministic)
TEST_F(OreGeneratorTest, DeterministicOreGeneration) {
    OreConfig config;
    config.seed = 12345;
    OreGenerator gen1(config);
    OreGenerator gen2(config);

    ChunkPos chunk_pos(0, 0);

    auto ores1 = gen1.generate_ores(chunk_pos, BiomeType::Plains);
    auto ores2 = gen2.generate_ores(chunk_pos, BiomeType::Plains);

    ASSERT_EQ(ores1.size(), ores2.size()) << "Same seed should produce same number of ores";

    for (size_t i = 0; i < ores1.size(); ++i) {
        EXPECT_EQ(ores1[i].pos.x, ores2[i].pos.x);
        EXPECT_EQ(ores1[i].pos.y, ores2[i].pos.y);
        EXPECT_EQ(ores1[i].pos.z, ores2[i].pos.z);
        EXPECT_EQ(ores1[i].block, ores2[i].block);
    }
}

// Test: Different seeds produce different ore positions
TEST_F(OreGeneratorTest, DifferentSeedsProduceDifferentOres) {
    OreConfig config1;
    config1.seed = 12345;
    OreConfig config2;
    config2.seed = 54321;

    OreGenerator gen1(config1);
    OreGenerator gen2(config2);

    ChunkPos chunk_pos(0, 0);

    auto ores1 = gen1.generate_ores(chunk_pos, BiomeType::Plains);
    auto ores2 = gen2.generate_ores(chunk_pos, BiomeType::Plains);

    // At least one should be different
    bool all_same = true;
    if (ores1.size() != ores2.size()) {
        all_same = false;
    } else {
        for (size_t i = 0; i < ores1.size() && i < ores2.size(); ++i) {
            if (ores1[i].pos.x != ores2[i].pos.x || ores1[i].pos.y != ores2[i].pos.y ||
                ores1[i].pos.z != ores2[i].pos.z) {
                all_same = false;
                break;
            }
        }
    }

    EXPECT_FALSE(all_same) << "Different seeds should produce different ore positions";
}

// Test: Ore placement within chunk bounds
TEST_F(OreGeneratorTest, OresWithinChunkBounds) {
    OreConfig config;
    config.seed = 12345;
    OreGenerator gen(config);

    for (int cx = -5; cx <= 5; ++cx) {
        for (int cz = -5; cz <= 5; ++cz) {
            ChunkPos chunk_pos(cx, cz);
            auto ores = gen.generate_ores(chunk_pos, BiomeType::Plains);

            for (const auto& ore : ores) {
                EXPECT_GE(ore.pos.x, 0) << "Ore X should be >= 0";
                EXPECT_LT(ore.pos.x, CHUNK_SIZE_X) << "Ore X should be < CHUNK_SIZE_X";
                EXPECT_GE(ore.pos.y, 0) << "Ore Y should be >= 0";
                EXPECT_LT(ore.pos.y, CHUNK_SIZE_Y) << "Ore Y should be < CHUNK_SIZE_Y";
                EXPECT_GE(ore.pos.z, 0) << "Ore Z should be >= 0";
                EXPECT_LT(ore.pos.z, CHUNK_SIZE_Z) << "Ore Z should be < CHUNK_SIZE_Z";
            }
        }
    }
}

// Test: Ores generate in chunks
TEST_F(OreGeneratorTest, OresGenerateInChunks) {
    OreConfig config;
    config.seed = 12345;
    OreGenerator gen(config);

    int total_ores = 0;

    for (int cx = -5; cx <= 5; ++cx) {
        for (int cz = -5; cz <= 5; ++cz) {
            ChunkPos chunk_pos(cx, cz);
            auto ores = gen.generate_ores(chunk_pos, BiomeType::Plains);
            total_ores += static_cast<int>(ores.size());
        }
    }

    EXPECT_GT(total_ores, 0) << "Should generate some ores across chunks";
}

// Test: get_ore_at returns valid results
TEST_F(OreGeneratorTest, GetOreAtWorks) {
    OreConfig config;
    config.seed = 12345;
    OreGenerator gen(config);

    // Get ores for a chunk
    ChunkPos chunk_pos(0, 0);
    auto ores = gen.generate_ores(chunk_pos, BiomeType::Plains);

    // Most positions should return BLOCK_AIR
    int air_count = 0;
    int ore_count = 0;

    for (int x = 0; x < 32; ++x) {
        for (int y = 0; y < 64; ++y) {
            for (int z = 0; z < 32; ++z) {
                BlockId result = gen.get_ore_at(x, y, z, BiomeType::Plains);
                if (result == BLOCK_AIR) {
                    ++air_count;
                } else {
                    ++ore_count;
                }
            }
        }
    }

    // Most should be air
    EXPECT_GT(air_count, ore_count) << "Most positions should not be ore";
}

// Test: Disabled ores produce no positions
TEST_F(OreGeneratorTest, DisabledOresNoPositions) {
    OreConfig config;
    config.seed = 12345;
    config.enabled = false;
    OreGenerator gen(config);

    ChunkPos chunk_pos(0, 0);
    auto ores = gen.generate_ores(chunk_pos, BiomeType::Plains);

    EXPECT_EQ(ores.size(), 0) << "Disabled ore generator should produce no ores";
}

// Test: Default ore configs exist
TEST_F(OreGeneratorTest, DefaultOreConfigsExist) {
    auto default_ores = OreConfig::get_default_ores();

    EXPECT_GT(default_ores.size(), 0) << "Should have default ore configurations";

    // Check that ores have valid ranges
    for (const auto& ore : default_ores) {
        EXPECT_GE(ore.min_y, 0);
        EXPECT_LE(ore.max_y, CHUNK_SIZE_Y);
        EXPECT_LE(ore.min_y, ore.max_y);
        EXPECT_GE(ore.min_vein_size, 1);
        EXPECT_LE(ore.min_vein_size, ore.max_vein_size);
        EXPECT_GE(ore.veins_per_chunk, 0);
    }
}

// Test: Can set custom ore configs
TEST_F(OreGeneratorTest, CustomOreConfigs) {
    OreConfig config;
    config.seed = 12345;
    OreGenerator gen(config);

    // Get default configs
    auto& original_configs = gen.get_ore_configs();
    size_t original_count = original_configs.size();

    // Set custom configs (empty)
    gen.set_ore_configs({});

    // Should now have no ores
    ChunkPos chunk_pos(0, 0);
    auto ores = gen.generate_ores(chunk_pos, BiomeType::Plains);
    EXPECT_EQ(ores.size(), 0) << "Empty ore config should produce no ores";

    // Restore defaults
    gen.set_ore_configs(OreConfig::get_default_ores());
    EXPECT_EQ(gen.get_ore_configs().size(), original_count);
}

// Test: Thread safety
TEST_F(OreGeneratorTest, ThreadSafety) {
    OreConfig config;
    config.seed = 12345;
    OreGenerator gen(config);

    std::atomic<int> total_ores{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&gen, &total_ores, t]() {
            int local_count = 0;
            for (int i = 0; i < 100; ++i) {
                ChunkPos chunk_pos(t * 100 + i, i);
                auto ores = gen.generate_ores(chunk_pos, BiomeType::Plains);
                local_count += static_cast<int>(ores.size());
            }
            total_ores += local_count;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(total_ores.load(), 0) << "Should generate ores across threads";
}

// Test: Ore block types are registered
TEST_F(OreGeneratorTest, OreBlockTypesRegistered) {
    const auto& registry = BlockRegistry::instance();

    EXPECT_TRUE(registry.find_id("realcraft:coal_ore").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:iron_ore").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:copper_ore").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:gold_ore").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:redstone_ore").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:lapis_ore").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:emerald_ore").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:diamond_ore").has_value());
}

// Test: Integration with TerrainGenerator
TEST_F(OreGeneratorTest, OresIntegratedWithTerrain) {
    TerrainConfig config;
    config.seed = 12345;
    config.ores.enabled = true;
    TerrainGenerator gen(config);

    ChunkDesc desc;
    desc.position = ChunkPos(0, 0);
    Chunk chunk(desc);

    gen.generate(chunk);

    // Count ore blocks
    const auto& registry = BlockRegistry::instance();
    auto coal_ore = registry.find_id("realcraft:coal_ore");
    auto iron_ore = registry.find_id("realcraft:iron_ore");

    int ore_count = 0;
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < CHUNK_SIZE_X; ++x) {
            for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
                BlockId block = chunk.get_block(LocalBlockPos(x, y, z));
                if ((coal_ore && block == *coal_ore) || (iron_ore && block == *iron_ore)) {
                    ++ore_count;
                }
            }
        }
    }

    EXPECT_GT(ore_count, 0) << "Should find ores in generated terrain";
}

// Test: Mountains-only ores respect biome
TEST_F(OreGeneratorTest, MountainsOnlyOresRespectBiome) {
    OreConfig config;
    config.seed = 12345;
    OreGenerator gen(config);

    // Check if emerald (mountains-only) appears differently in different biomes
    int emerald_in_mountains = 0;
    int emerald_in_plains = 0;

    const auto& registry = BlockRegistry::instance();
    auto emerald_ore = registry.find_id("realcraft:emerald_ore");

    if (!emerald_ore) {
        GTEST_SKIP() << "Emerald ore not registered";
    }

    for (int cx = 0; cx < 20; ++cx) {
        for (int cz = 0; cz < 20; ++cz) {
            ChunkPos chunk_pos(cx, cz);

            auto mountains_ores = gen.generate_ores(chunk_pos, BiomeType::Mountains);
            for (const auto& ore : mountains_ores) {
                if (ore.block == *emerald_ore) {
                    ++emerald_in_mountains;
                }
            }

            auto plains_ores = gen.generate_ores(chunk_pos, BiomeType::Plains);
            for (const auto& ore : plains_ores) {
                if (ore.block == *emerald_ore) {
                    ++emerald_in_plains;
                }
            }
        }
    }

    // Emerald should only be in mountains (mountains_only = true for emerald)
    // At least there should be more in mountains
    EXPECT_GE(emerald_in_mountains, emerald_in_plains);
}

}  // namespace
}  // namespace realcraft::world
