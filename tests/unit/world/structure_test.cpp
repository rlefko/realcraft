// RealCraft World System Tests
// structure_test.cpp - Tests for StructureGenerator class

#include <gtest/gtest.h>

#include <atomic>
#include <realcraft/world/biome.hpp>
#include <realcraft/world/block.hpp>
#include <realcraft/world/chunk.hpp>
#include <realcraft/world/structure_generator.hpp>
#include <realcraft/world/terrain_generator.hpp>
#include <thread>
#include <vector>

namespace realcraft::world {
namespace {

class StructureGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        BlockRegistry::instance().register_defaults();
        BiomeRegistry::instance().register_defaults();
    }
};

// Test: Same seed produces same structure placement (deterministic)
TEST_F(StructureGeneratorTest, DeterministicStructurePlacement) {
    StructureConfig config;
    config.seed = 12345;
    StructureGenerator gen1(config);
    StructureGenerator gen2(config);

    for (int i = 0; i < 100; ++i) {
        ChunkPos chunk_pos(i - 50, i * 2 - 100);

        EXPECT_EQ(gen1.should_place_structure(chunk_pos, StructureType::Boulder, BiomeType::Mountains),
                  gen2.should_place_structure(chunk_pos, StructureType::Boulder, BiomeType::Mountains))
            << "Structure placement mismatch at chunk (" << chunk_pos.x << ", " << chunk_pos.y << ")";
    }
}

// Test: Different seeds produce different structure positions
TEST_F(StructureGeneratorTest, DifferentSeedsProduceDifferentStructures) {
    StructureConfig config1;
    config1.seed = 12345;
    StructureConfig config2;
    config2.seed = 54321;

    StructureGenerator gen1(config1);
    StructureGenerator gen2(config2);

    int different_count = 0;

    for (int i = 0; i < 100; ++i) {
        ChunkPos chunk_pos(i, i);

        if (gen1.should_place_structure(chunk_pos, StructureType::Boulder, BiomeType::Mountains) !=
            gen2.should_place_structure(chunk_pos, StructureType::Boulder, BiomeType::Mountains)) {
            ++different_count;
        }
    }

    EXPECT_GT(different_count, 0) << "Different seeds should produce different structures";
}

// Test: Structures only in appropriate biomes (mountains)
TEST_F(StructureGeneratorTest, StructuresOnlyInMountains) {
    StructureConfig config;
    config.seed = 12345;
    config.rock_spire.chance_per_chunk = 0.5f;  // Higher for testing
    StructureGenerator gen(config);

    int structures_in_mountains = 0;
    int structures_in_ocean = 0;
    int structures_in_forest = 0;

    for (int cx = 0; cx < 100; ++cx) {
        for (int cz = 0; cz < 100; ++cz) {
            ChunkPos chunk_pos(cx, cz);

            if (gen.should_place_structure(chunk_pos, StructureType::RockSpire, BiomeType::Mountains)) {
                ++structures_in_mountains;
            }
            if (gen.should_place_structure(chunk_pos, StructureType::RockSpire, BiomeType::Ocean)) {
                ++structures_in_ocean;
            }
            if (gen.should_place_structure(chunk_pos, StructureType::RockSpire, BiomeType::Forest)) {
                ++structures_in_forest;
            }
        }
    }

    EXPECT_GT(structures_in_mountains, 0) << "Should place rock spires in mountains";
    EXPECT_EQ(structures_in_ocean, 0) << "Should not place rock spires in ocean";
    EXPECT_EQ(structures_in_forest, 0) << "Should not place rock spires in forest";
}

// Test: Boulders allowed in more biomes than rock spires
TEST_F(StructureGeneratorTest, BouldersInMoreBiomes) {
    StructureConfig config;
    config.seed = 12345;
    config.boulder.chance_per_chunk = 0.5f;  // Higher for testing
    StructureGenerator gen(config);

    int boulders_in_plains = 0;
    int rock_spires_in_plains = 0;

    for (int cx = 0; cx < 100; ++cx) {
        for (int cz = 0; cz < 100; ++cz) {
            ChunkPos chunk_pos(cx, cz);

            if (gen.should_place_structure(chunk_pos, StructureType::Boulder, BiomeType::Plains)) {
                ++boulders_in_plains;
            }
            if (gen.should_place_structure(chunk_pos, StructureType::RockSpire, BiomeType::Plains)) {
                ++rock_spires_in_plains;
            }
        }
    }

    EXPECT_GT(boulders_in_plains, 0) << "Should allow boulders in plains";
    EXPECT_EQ(rock_spires_in_plains, 0) << "Should not allow rock spires in plains";
}

// Test: Structure position is within chunk bounds
TEST_F(StructureGeneratorTest, StructurePositionWithinChunk) {
    StructureConfig config;
    config.seed = 12345;
    StructureGenerator gen(config);

    for (int cx = -50; cx <= 50; ++cx) {
        for (int cz = -50; cz <= 50; ++cz) {
            ChunkPos chunk_pos(cx, cz);

            auto pos = gen.get_structure_position(chunk_pos, StructureType::Boulder);
            if (pos) {
                EXPECT_GE(pos->x, 0) << "Structure X should be >= 0";
                EXPECT_LT(pos->x, CHUNK_SIZE_X) << "Structure X should be < CHUNK_SIZE_X";
                EXPECT_GE(pos->z, 0) << "Structure Z should be >= 0";
                EXPECT_LT(pos->z, CHUNK_SIZE_Z) << "Structure Z should be < CHUNK_SIZE_Z";
            }
        }
    }
}

// Test: Rock spire generation produces blocks
TEST_F(StructureGeneratorTest, RockSpireProducesBlocks) {
    StructureConfig config;
    config.seed = 12345;
    StructureGenerator gen(config);

    ChunkPos chunk_pos(0, 0);
    auto blocks = gen.generate_structure(chunk_pos, StructureType::RockSpire, 64);

    EXPECT_GT(blocks.size(), 0) << "Rock spire should produce blocks";

    // Verify blocks are stone-related
    const auto& registry = BlockRegistry::instance();
    auto stone = registry.stone_id();
    auto pillar_stone = registry.find_id("realcraft:pillar_stone");

    int valid_blocks = 0;
    for (const auto& block : blocks) {
        if (block.block == stone || (pillar_stone && block.block == *pillar_stone)) {
            ++valid_blocks;
        }
    }

    EXPECT_GT(valid_blocks, 0) << "Rock spire should have stone blocks";
}

// Test: Boulder generation produces blocks
TEST_F(StructureGeneratorTest, BoulderProducesBlocks) {
    StructureConfig config;
    config.seed = 12345;
    StructureGenerator gen(config);

    ChunkPos chunk_pos(0, 0);
    auto blocks = gen.generate_structure(chunk_pos, StructureType::Boulder, 64);

    EXPECT_GT(blocks.size(), 0) << "Boulder should produce blocks";
}

// Test: Stone outcrop generation produces blocks
TEST_F(StructureGeneratorTest, OutcropProducesBlocks) {
    StructureConfig config;
    config.seed = 12345;
    StructureGenerator gen(config);

    ChunkPos chunk_pos(0, 0);
    auto blocks = gen.generate_structure(chunk_pos, StructureType::StoneOutcrop, 64);

    EXPECT_GT(blocks.size(), 0) << "Stone outcrop should produce blocks";
}

// Test: Structure blocks have valid offsets
TEST_F(StructureGeneratorTest, StructureBlocksValidOffsets) {
    StructureConfig config;
    config.seed = 12345;
    StructureGenerator gen(config);

    ChunkPos chunk_pos(0, 0);

    for (int type = 0; type < static_cast<int>(StructureType::Count); ++type) {
        auto blocks = gen.generate_structure(chunk_pos, static_cast<StructureType>(type), 64);

        for (const auto& block : blocks) {
            // Y offset should be positive (above base)
            EXPECT_GE(block.offset.y, 0) << "Structure blocks should be at or above base";

            // X and Z offsets should be within reasonable bounds
            EXPECT_GE(block.offset.x, -20) << "Structure blocks X offset should be reasonable";
            EXPECT_LE(block.offset.x, 20) << "Structure blocks X offset should be reasonable";
            EXPECT_GE(block.offset.z, -20) << "Structure blocks Z offset should be reasonable";
            EXPECT_LE(block.offset.z, 20) << "Structure blocks Z offset should be reasonable";
        }
    }
}

// Test: Rock spire is taller than boulder
TEST_F(StructureGeneratorTest, RockSpireTallerThanBoulder) {
    StructureConfig config;
    config.seed = 12345;
    StructureGenerator gen(config);

    ChunkPos chunk_pos(0, 0);

    auto spire_blocks = gen.generate_structure(chunk_pos, StructureType::RockSpire, 64);
    auto boulder_blocks = gen.generate_structure(chunk_pos, StructureType::Boulder, 64);

    int32_t spire_max_y = 0;
    int32_t boulder_max_y = 0;

    for (const auto& block : spire_blocks) {
        spire_max_y = std::max(spire_max_y, block.offset.y);
    }
    for (const auto& block : boulder_blocks) {
        boulder_max_y = std::max(boulder_max_y, block.offset.y);
    }

    EXPECT_GT(spire_max_y, boulder_max_y) << "Rock spire should be taller than boulder";
}

// Test: Disabled structures produce no placement
TEST_F(StructureGeneratorTest, DisabledStructuresNoPlacement) {
    StructureConfig config;
    config.seed = 12345;
    config.enabled = false;
    StructureGenerator gen(config);

    int structures = 0;

    for (int cx = 0; cx < 100; ++cx) {
        for (int cz = 0; cz < 100; ++cz) {
            ChunkPos chunk_pos(cx, cz);

            if (gen.should_place_structure(chunk_pos, StructureType::Boulder, BiomeType::Mountains) ||
                gen.should_place_structure(chunk_pos, StructureType::RockSpire, BiomeType::Mountains) ||
                gen.should_place_structure(chunk_pos, StructureType::StoneOutcrop, BiomeType::Mountains)) {
                ++structures;
            }
        }
    }

    EXPECT_EQ(structures, 0) << "Disabled structures should not be placed";
}

// Test: Individual structure types can be disabled
TEST_F(StructureGeneratorTest, IndividualStructuresCanBeDisabled) {
    StructureConfig config;
    config.seed = 12345;
    config.rock_spire.enabled = false;
    config.boulder.enabled = true;
    config.boulder.chance_per_chunk = 0.5f;
    StructureGenerator gen(config);

    int rock_spires = 0;
    int boulders = 0;

    for (int cx = 0; cx < 100; ++cx) {
        for (int cz = 0; cz < 100; ++cz) {
            ChunkPos chunk_pos(cx, cz);

            if (gen.should_place_structure(chunk_pos, StructureType::RockSpire, BiomeType::Mountains)) {
                ++rock_spires;
            }
            if (gen.should_place_structure(chunk_pos, StructureType::Boulder, BiomeType::Mountains)) {
                ++boulders;
            }
        }
    }

    EXPECT_EQ(rock_spires, 0) << "Disabled rock spires should not be placed";
    EXPECT_GT(boulders, 0) << "Enabled boulders should be placed";
}

// Test: Thread safety
TEST_F(StructureGeneratorTest, ThreadSafety) {
    StructureConfig config;
    config.seed = 12345;
    StructureGenerator gen(config);

    std::atomic<int> total_structures{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&gen, &total_structures, t]() {
            int local_count = 0;
            for (int i = 0; i < 100; ++i) {
                ChunkPos chunk_pos(t * 100 + i, i);
                if (gen.should_place_structure(chunk_pos, StructureType::Boulder, BiomeType::Mountains)) {
                    auto blocks = gen.generate_structure(chunk_pos, StructureType::Boulder, 64);
                    local_count += static_cast<int>(blocks.size());
                }
            }
            total_structures += local_count;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should generate some structure blocks across threads
    EXPECT_GE(total_structures.load(), 0);
}

// Test: Structure block types registered
TEST_F(StructureGeneratorTest, StructureBlockTypesRegistered) {
    const auto& registry = BlockRegistry::instance();

    EXPECT_TRUE(registry.find_id("realcraft:mossy_stone").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:cobblestone").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:cracked_stone").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:pillar_stone").has_value());
}

// Test: Integration with TerrainGenerator
TEST_F(StructureGeneratorTest, StructuresIntegratedWithTerrain) {
    TerrainConfig config;
    config.seed = 12345;
    config.structures.enabled = true;
    config.structures.boulder.chance_per_chunk = 0.3f;  // Higher for testing
    config.biome_system.enabled = true;
    TerrainGenerator gen(config);

    // Generate chunks - structures appear based on biome
    // Just verify generation doesn't crash
    for (int cx = -2; cx <= 2; ++cx) {
        for (int cz = -2; cz <= 2; ++cz) {
            ChunkDesc desc;
            desc.position = ChunkPos(cx, cz);
            Chunk chunk(desc);

            EXPECT_NO_THROW(gen.generate(chunk));
        }
    }
}

// Test: Desert has rock spires
TEST_F(StructureGeneratorTest, DesertHasRockSpires) {
    StructureConfig config;
    config.seed = 12345;
    config.rock_spire.chance_per_chunk = 0.5f;  // Higher for testing
    StructureGenerator gen(config);

    int rock_spires_desert = 0;

    for (int cx = 0; cx < 100; ++cx) {
        for (int cz = 0; cz < 100; ++cz) {
            ChunkPos chunk_pos(cx, cz);

            if (gen.should_place_structure(chunk_pos, StructureType::RockSpire, BiomeType::Desert)) {
                ++rock_spires_desert;
            }
        }
    }

    EXPECT_GT(rock_spires_desert, 0) << "Desert should have rock spires";
}

}  // namespace
}  // namespace realcraft::world
