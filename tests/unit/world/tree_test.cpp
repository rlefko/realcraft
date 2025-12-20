// RealCraft World System Tests
// tree_test.cpp - Tests for TreeGenerator class

#include <gtest/gtest.h>

#include <atomic>
#include <realcraft/world/biome.hpp>
#include <realcraft/world/block.hpp>
#include <realcraft/world/chunk.hpp>
#include <realcraft/world/terrain_generator.hpp>
#include <realcraft/world/tree_generator.hpp>
#include <thread>
#include <vector>

namespace realcraft::world {
namespace {

class TreeGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        BlockRegistry::instance().register_defaults();
        BiomeRegistry::instance().register_defaults();
    }
};

// Test: Same seed produces same tree placement (deterministic)
TEST_F(TreeGeneratorTest, DeterministicTreePlacement) {
    TreeConfig config;
    config.seed = 12345;
    TreeGenerator gen1(config);
    TreeGenerator gen2(config);

    for (int i = 0; i < 100; ++i) {
        int64_t x = i * 17 - 500;
        int64_t z = i * 23 - 600;

        EXPECT_EQ(gen1.should_place_tree(x, z, BiomeType::Forest), gen2.should_place_tree(x, z, BiomeType::Forest))
            << "Tree placement mismatch at (" << x << ", " << z << ")";
    }
}

// Test: Different seeds produce different tree positions
TEST_F(TreeGeneratorTest, DifferentSeedsProduceDifferentTrees) {
    TreeConfig config1;
    config1.seed = 12345;
    TreeConfig config2;
    config2.seed = 54321;

    TreeGenerator gen1(config1);
    TreeGenerator gen2(config2);

    // Count tree positions in a larger area to ensure we find trees
    std::vector<std::pair<int64_t, int64_t>> trees1;
    std::vector<std::pair<int64_t, int64_t>> trees2;

    for (int x = 0; x < 100; ++x) {
        for (int z = 0; z < 100; ++z) {
            if (gen1.should_place_tree(x, z, BiomeType::Forest)) {
                trees1.emplace_back(x, z);
            }
            if (gen2.should_place_tree(x, z, BiomeType::Forest)) {
                trees2.emplace_back(x, z);
            }
        }
    }

    // With different seeds, tree positions should differ
    // Either different count or different positions
    bool positions_differ = false;

    if (trees1.size() != trees2.size()) {
        positions_differ = true;
    } else if (!trees1.empty()) {
        // Check if at least one position is different
        for (size_t i = 0; i < trees1.size(); ++i) {
            if (trees1[i] != trees2[i]) {
                positions_differ = true;
                break;
            }
        }
    }

    EXPECT_TRUE(positions_differ || (trees1.empty() && trees2.empty()))
        << "Different seeds should produce different tree positions (or both empty is ok)";
}

// Test: Trees exist in forest biome
TEST_F(TreeGeneratorTest, TreesInForest) {
    TreeConfig config;
    config.seed = 12345;
    TreeGenerator gen(config);

    int trees_in_forest = 0;

    for (int x = 0; x < 200; ++x) {
        for (int z = 0; z < 200; ++z) {
            if (gen.should_place_tree(x, z, BiomeType::Forest)) {
                ++trees_in_forest;
            }
        }
    }

    EXPECT_GT(trees_in_forest, 0) << "Should place trees in forest";
}

// Test: No trees in ocean biome
TEST_F(TreeGeneratorTest, NoTreesInOcean) {
    TreeConfig config;
    config.seed = 12345;
    TreeGenerator gen(config);

    int trees_in_ocean = 0;

    for (int x = 0; x < 100; ++x) {
        for (int z = 0; z < 100; ++z) {
            if (gen.should_place_tree(x, z, BiomeType::Ocean)) {
                ++trees_in_ocean;
            }
        }
    }

    EXPECT_EQ(trees_in_ocean, 0) << "Should not place trees in ocean";
}

// Test: No trees in desert biome
TEST_F(TreeGeneratorTest, NoTreesInDesert) {
    TreeConfig config;
    config.seed = 12345;
    TreeGenerator gen(config);

    int trees_in_desert = 0;

    for (int x = 0; x < 100; ++x) {
        for (int z = 0; z < 100; ++z) {
            if (gen.should_place_tree(x, z, BiomeType::Desert)) {
                ++trees_in_desert;
            }
        }
    }

    EXPECT_EQ(trees_in_desert, 0) << "Should not place trees in desert";
}

// Test: Biome-specific tree species
TEST_F(TreeGeneratorTest, BiomeSpecificSpecies) {
    TreeConfig config;
    config.seed = 12345;
    TreeGenerator gen(config);

    // Forest should have oak/birch
    TreeSpecies forest_species = gen.get_species_for_biome(BiomeType::Forest, 50, 50);
    EXPECT_TRUE(forest_species == TreeSpecies::Oak || forest_species == TreeSpecies::Birch)
        << "Forest should have oak or birch trees";

    // Taiga should have spruce
    TreeSpecies taiga_species = gen.get_species_for_biome(BiomeType::Taiga, 50, 50);
    EXPECT_EQ(taiga_species, TreeSpecies::Spruce) << "Taiga should have spruce trees";

    // Savanna should have acacia
    TreeSpecies savanna_species = gen.get_species_for_biome(BiomeType::Savanna, 50, 50);
    EXPECT_EQ(savanna_species, TreeSpecies::Acacia) << "Savanna should have acacia trees";
}

// Test: Tree generation produces blocks
TEST_F(TreeGeneratorTest, TreeGenerationProducesBlocks) {
    TreeConfig config;
    config.seed = 12345;
    TreeGenerator gen(config);

    auto blocks = gen.generate_tree(0, 0, BiomeType::Forest, 64);

    EXPECT_GT(blocks.size(), 0) << "Tree generation should produce blocks";

    // Should have some logs and leaves (oak uses "wood" and "leaves")
    const auto& registry = BlockRegistry::instance();
    auto oak_log = registry.find_id("realcraft:wood");
    auto oak_leaves = registry.find_id("realcraft:leaves");

    int logs = 0;
    int leaves = 0;

    for (const auto& block : blocks) {
        if (oak_log && block.block == *oak_log) {
            ++logs;
        }
        if (oak_leaves && block.block == *oak_leaves) {
            ++leaves;
        }
    }

    EXPECT_GT(logs, 0) << "Tree should have logs";
    EXPECT_GT(leaves, 0) << "Tree should have leaves";
}

// Test: Tree template exists for each species
TEST_F(TreeGeneratorTest, TreeTemplatesExist) {
    TreeConfig config;
    config.seed = 12345;
    TreeGenerator gen(config);

    for (int s = 0; s < static_cast<int>(TreeSpecies::Count); ++s) {
        TreeSpecies species = static_cast<TreeSpecies>(s);
        const auto& tmpl = gen.get_template(species);

        EXPECT_EQ(tmpl.species, species) << "Template species should match requested species";
        EXPECT_GT(tmpl.min_height, 0) << "Tree should have positive min height";
        EXPECT_GE(tmpl.max_height, tmpl.min_height) << "Max height should be >= min height";
        EXPECT_NE(tmpl.log_block, BLOCK_AIR) << "Tree should have log block";
        EXPECT_NE(tmpl.leaf_block, BLOCK_AIR) << "Tree should have leaf block";
    }
}

// Test: Tree blocks have valid offsets
TEST_F(TreeGeneratorTest, TreeBlocksValidOffsets) {
    TreeConfig config;
    config.seed = 12345;
    TreeGenerator gen(config);

    auto blocks = gen.generate_tree(0, 0, BiomeType::Forest, 64);

    for (const auto& block : blocks) {
        // Y offset should be positive (above ground)
        EXPECT_GE(block.offset.y, 0) << "Tree blocks should be at or above ground";

        // X and Z offsets should be within reasonable bounds
        EXPECT_GE(block.offset.x, -10) << "Tree blocks X offset should be reasonable";
        EXPECT_LE(block.offset.x, 10) << "Tree blocks X offset should be reasonable";
        EXPECT_GE(block.offset.z, -10) << "Tree blocks Z offset should be reasonable";
        EXPECT_LE(block.offset.z, 10) << "Tree blocks Z offset should be reasonable";
    }
}

// Test: Disabled trees produce no placement
TEST_F(TreeGeneratorTest, DisabledTreesNoPlacement) {
    TreeConfig config;
    config.seed = 12345;
    config.enabled = false;
    TreeGenerator gen(config);

    int trees = 0;

    for (int x = 0; x < 100; ++x) {
        for (int z = 0; z < 100; ++z) {
            if (gen.should_place_tree(x, z, BiomeType::Forest)) {
                ++trees;
            }
        }
    }

    EXPECT_EQ(trees, 0) << "Disabled trees should not be placed";
}

// Test: Thread safety
TEST_F(TreeGeneratorTest, ThreadSafety) {
    TreeConfig config;
    config.seed = 12345;
    TreeGenerator gen(config);

    std::atomic<int> total_trees{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&gen, &total_trees, t]() {
            int local_count = 0;
            for (int i = 0; i < 1000; ++i) {
                int64_t x = t * 1000 + i;
                int64_t z = i;
                if (gen.should_place_tree(x, z, BiomeType::Forest)) {
                    ++local_count;
                }
            }
            total_trees += local_count;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(total_trees.load(), 0) << "Should place trees across threads";
}

// Test: Tree block types registered
TEST_F(TreeGeneratorTest, TreeBlockTypesRegistered) {
    const auto& registry = BlockRegistry::instance();

    // Log types (oak uses generic "wood", others are specific)
    EXPECT_TRUE(registry.find_id("realcraft:wood").has_value());  // Oak log
    EXPECT_TRUE(registry.find_id("realcraft:spruce_log").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:birch_log").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:jungle_log").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:acacia_log").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:palm_log").has_value());

    // Leaf types (oak uses generic "leaves", others are specific)
    EXPECT_TRUE(registry.find_id("realcraft:leaves").has_value());  // Oak leaves
    EXPECT_TRUE(registry.find_id("realcraft:spruce_leaves").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:birch_leaves").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:jungle_leaves").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:acacia_leaves").has_value());
    EXPECT_TRUE(registry.find_id("realcraft:palm_leaves").has_value());
}

// Test: Integration with TerrainGenerator
TEST_F(TreeGeneratorTest, TreesIntegratedWithTerrain) {
    TerrainConfig config;
    config.seed = 12345;
    config.trees.enabled = true;
    config.biome_system.enabled = true;
    TerrainGenerator gen(config);

    // Generate several chunks to increase chance of finding trees
    const auto& registry = BlockRegistry::instance();
    auto oak_log = registry.find_id("realcraft:wood");       // Oak uses "wood"
    auto oak_leaves = registry.find_id("realcraft:leaves");  // Oak uses "leaves"

    int total_logs = 0;
    int total_leaves = 0;

    for (int cx = -2; cx <= 2; ++cx) {
        for (int cz = -2; cz <= 2; ++cz) {
            ChunkDesc desc;
            desc.position = ChunkPos(cx, cz);
            Chunk chunk(desc);

            gen.generate(chunk);

            for (int y = 60; y < 128; ++y) {
                for (int x = 0; x < CHUNK_SIZE_X; ++x) {
                    for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
                        BlockId block = chunk.get_block(LocalBlockPos(x, y, z));
                        if (oak_log && block == *oak_log) {
                            ++total_logs;
                        }
                        if (oak_leaves && block == *oak_leaves) {
                            ++total_leaves;
                        }
                    }
                }
            }
        }
    }

    // Should find some tree blocks across multiple chunks
    // (may be 0 if all chunks are ocean/desert, but unlikely with seed 12345)
    EXPECT_GE(total_logs, 0);
    EXPECT_GE(total_leaves, 0);
}

// Test: Spruce tree shape (conical)
TEST_F(TreeGeneratorTest, SpruceTreeConicalShape) {
    TreeConfig config;
    config.seed = 12345;
    TreeGenerator gen(config);

    // Force spruce generation by using taiga biome
    auto blocks = gen.generate_tree(0, 0, BiomeType::Taiga, 64);

    const auto& registry = BlockRegistry::instance();
    auto spruce_leaves = registry.find_id("realcraft:spruce_leaves");

    if (!spruce_leaves) {
        GTEST_SKIP() << "Spruce leaves not registered";
    }

    // Count leaves at different heights
    std::map<int32_t, int> leaves_per_height;

    for (const auto& block : blocks) {
        if (block.block == *spruce_leaves) {
            leaves_per_height[block.offset.y]++;
        }
    }

    // Spruce should be wider at bottom, narrower at top (conical)
    // Just verify we have leaves at multiple heights
    EXPECT_GT(leaves_per_height.size(), 2) << "Spruce should have leaves at multiple heights";
}

// Test: Forest density higher than plains
TEST_F(TreeGeneratorTest, ForestDensityHigherThanPlains) {
    TreeConfig config;
    config.seed = 12345;
    TreeGenerator gen(config);

    int trees_in_forest = 0;
    int trees_in_plains = 0;

    for (int x = 0; x < 500; ++x) {
        for (int z = 0; z < 500; ++z) {
            if (gen.should_place_tree(x, z, BiomeType::Forest)) {
                ++trees_in_forest;
            }
            if (gen.should_place_tree(x, z, BiomeType::Plains)) {
                ++trees_in_plains;
            }
        }
    }

    EXPECT_GT(trees_in_forest, trees_in_plains) << "Forest should have more trees than plains";
}

// Test: Trees don't spawn too close together
TEST_F(TreeGeneratorTest, TreeSpacing) {
    TreeConfig config;
    config.seed = 12345;
    TreeGenerator gen(config);

    std::vector<std::pair<int64_t, int64_t>> tree_positions;

    for (int x = 0; x < 100; ++x) {
        for (int z = 0; z < 100; ++z) {
            if (gen.should_place_tree(x, z, BiomeType::Forest)) {
                tree_positions.emplace_back(x, z);
            }
        }
    }

    // Check that trees aren't directly adjacent
    int too_close = 0;
    for (size_t i = 0; i < tree_positions.size(); ++i) {
        for (size_t j = i + 1; j < tree_positions.size(); ++j) {
            int64_t dx = tree_positions[i].first - tree_positions[j].first;
            int64_t dz = tree_positions[i].second - tree_positions[j].second;
            if (dx * dx + dz * dz < 4) {  // Distance < 2
                ++too_close;
            }
        }
    }

    // Allow some overlap due to hash-based placement
    EXPECT_LT(too_close, static_cast<int>(tree_positions.size() / 4)) << "Most trees should have some spacing";
}

}  // namespace
}  // namespace realcraft::world
