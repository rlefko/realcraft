// RealCraft Physics Engine Tests
// fragmentation_test.cpp - Unit tests for FragmentationAlgorithm

#include <gtest/gtest.h>

#include <realcraft/physics/fragmentation.hpp>
#include <realcraft/world/block.hpp>

namespace realcraft::physics::tests {

class FragmentationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure block registry is initialized
        world::BlockRegistry::instance().register_defaults();
        rng_.seed(42);  // Fixed seed for reproducibility
    }

    BlockCluster create_linear_cluster(size_t length) {
        BlockCluster cluster;
        for (size_t i = 0; i < length; ++i) {
            cluster.positions.push_back({static_cast<int64_t>(i), 0, 0});
            cluster.block_ids.push_back(world::BLOCK_STONE);
        }
        cluster.compute_properties();
        return cluster;
    }

    BlockCluster create_cube_cluster(size_t size) {
        BlockCluster cluster;
        for (size_t x = 0; x < size; ++x) {
            for (size_t y = 0; y < size; ++y) {
                for (size_t z = 0; z < size; ++z) {
                    cluster.positions.push_back(
                        {static_cast<int64_t>(x), static_cast<int64_t>(y), static_cast<int64_t>(z)});
                    cluster.block_ids.push_back(world::BLOCK_STONE);
                }
            }
        }
        cluster.compute_properties();
        return cluster;
    }

    BlockCluster create_mixed_material_cluster() {
        BlockCluster cluster;
        // Stone on left half
        for (int x = 0; x < 4; ++x) {
            cluster.positions.push_back({x, 0, 0});
            cluster.block_ids.push_back(world::BLOCK_STONE);
        }
        // Glass on right half (more brittle)
        auto glass_id = world::BlockRegistry::instance().find_id("glass");
        world::BlockId glass = glass_id.value_or(world::BLOCK_STONE);
        for (int x = 4; x < 8; ++x) {
            cluster.positions.push_back({x, 0, 0});
            cluster.block_ids.push_back(glass);
        }
        cluster.compute_properties();
        return cluster;
    }

    std::mt19937 rng_;
};

// ============================================================================
// Fragment Properties Tests
// ============================================================================

TEST_F(FragmentationTest, FragmentComputeProperties) {
    Fragment frag;
    frag.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    frag.block_ids = {world::BLOCK_STONE, world::BLOCK_STONE, world::BLOCK_STONE};

    frag.compute_properties();

    EXPECT_GT(frag.total_mass, 0.0);
    EXPECT_NE(frag.center_of_mass, glm::dvec3(0.0));
    EXPECT_FALSE(frag.empty());
    EXPECT_EQ(frag.size(), 3u);
}

TEST_F(FragmentationTest, EmptyFragmentProperties) {
    Fragment frag;
    frag.compute_properties();

    EXPECT_EQ(frag.total_mass, 0.0);
    EXPECT_EQ(frag.center_of_mass, glm::dvec3(0.0));
    EXPECT_TRUE(frag.empty());
    EXPECT_EQ(frag.size(), 0u);
}

// ============================================================================
// Uniform Fragmentation Tests
// ============================================================================

TEST_F(FragmentationTest, UniformFragmentSmallCluster) {
    auto cluster = create_linear_cluster(5);

    auto fragments = FragmentationAlgorithm::fragment_uniform(cluster, 1, 10, rng_);

    // Small cluster should stay as one fragment
    EXPECT_EQ(fragments.size(), 1u);
    EXPECT_EQ(fragments[0].size(), 5u);
}

TEST_F(FragmentationTest, UniformFragmentLargeCluster) {
    auto cluster = create_linear_cluster(100);

    auto fragments = FragmentationAlgorithm::fragment_uniform(cluster, 1, 20, rng_);

    // Large cluster should be split
    EXPECT_GT(fragments.size(), 1u);

    // All fragments should be at most max_size
    for (const auto& frag : fragments) {
        EXPECT_LE(frag.size(), 20u);
    }

    // Total blocks should be preserved
    size_t total_blocks = 0;
    for (const auto& frag : fragments) {
        total_blocks += frag.size();
    }
    EXPECT_EQ(total_blocks, 100u);
}

TEST_F(FragmentationTest, UniformFragmentRespectsMinSize) {
    auto cluster = create_linear_cluster(20);

    auto fragments = FragmentationAlgorithm::fragment_uniform(cluster, 5, 10, rng_);

    // All fragments (except possibly the last) should be at least min_size
    size_t small_fragments = 0;
    for (const auto& frag : fragments) {
        if (frag.size() < 5) {
            ++small_fragments;
        }
    }
    // Allow at most 1 small fragment (merged remainder)
    EXPECT_LE(small_fragments, 1u);
}

// ============================================================================
// Brittleness-Based Fragmentation Tests
// ============================================================================

TEST_F(FragmentationTest, BrittlenessFragmentBrittleMaterial) {
    // Create cluster of glass (high brittleness)
    BlockCluster cluster;
    auto glass_id = world::BlockRegistry::instance().find_id("glass");
    world::BlockId glass = glass_id.value_or(world::BLOCK_STONE);

    for (int i = 0; i < 32; ++i) {
        cluster.positions.push_back({i, 0, 0});
        cluster.block_ids.push_back(glass);
    }
    cluster.compute_properties();

    auto fragments = FragmentationAlgorithm::fragment_by_brittleness(cluster, 1, 32, rng_);

    // Brittle material should fragment into more pieces
    EXPECT_GE(fragments.size(), 1u);
}

TEST_F(FragmentationTest, BrittlenessFragmentSolidMaterial) {
    // Create cluster of stone (low brittleness)
    auto cluster = create_linear_cluster(32);

    auto fragments = FragmentationAlgorithm::fragment_by_brittleness(cluster, 1, 32, rng_);

    // Stone should fragment into fewer pieces than glass
    EXPECT_GE(fragments.size(), 1u);
}

// ============================================================================
// Structural Fragmentation Tests
// ============================================================================

TEST_F(FragmentationTest, StructuralFragmentFindsWeakPoints) {
    // Create an H-shaped cluster with a narrow bridge
    BlockCluster cluster;
    // Left pillar
    for (int y = 0; y < 4; ++y) {
        cluster.positions.push_back({0, y, 0});
        cluster.block_ids.push_back(world::BLOCK_STONE);
    }
    // Right pillar
    for (int y = 0; y < 4; ++y) {
        cluster.positions.push_back({4, y, 0});
        cluster.block_ids.push_back(world::BLOCK_STONE);
    }
    // Bridge (single block connection)
    cluster.positions.push_back({2, 2, 0});
    cluster.block_ids.push_back(world::BLOCK_STONE);

    cluster.compute_properties();

    auto weak_points = FragmentationAlgorithm::find_weak_points(
        std::unordered_set<world::WorldBlockPos>(cluster.positions.begin(), cluster.positions.end()));

    // The bridge block should be identified as a weak point
    EXPECT_FALSE(weak_points.empty());
}

// ============================================================================
// Material-Based Fragmentation Tests
// ============================================================================

TEST_F(FragmentationTest, MaterialBasedFragmentDifferentMaterials) {
    auto cluster = create_mixed_material_cluster();

    auto fragments = FragmentationAlgorithm::fragment_by_material(cluster, 1, 32, rng_);

    // Should separate into at least 2 fragments (stone and glass)
    EXPECT_GE(fragments.size(), 2u);
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST_F(FragmentationTest, ComputeAverageBrittleness) {
    std::vector<world::WorldBlockPos> positions = {{0, 0, 0}, {1, 0, 0}};
    std::vector<world::BlockId> block_ids = {world::BLOCK_STONE, world::BLOCK_STONE};

    float brittleness = FragmentationAlgorithm::compute_average_brittleness(positions, block_ids);

    // Stone has low brittleness (0.0)
    EXPECT_LE(brittleness, 0.1f);
}

TEST_F(FragmentationTest, FindConnectedComponents) {
    // Two separate groups
    std::unordered_set<world::WorldBlockPos> positions = {
        {0, 0, 0},
        {1, 0, 0},
        {2, 0, 0},  // Group 1
        {10, 0, 0},
        {11, 0, 0}  // Group 2 (disconnected)
    };

    auto components = FragmentationAlgorithm::find_connected_components(positions);

    EXPECT_EQ(components.size(), 2u);
}

TEST_F(FragmentationTest, CountNeighbors) {
    std::unordered_set<world::WorldBlockPos> positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

    int center_neighbors = FragmentationAlgorithm::count_neighbors({0, 0, 0}, positions);
    int edge_neighbors = FragmentationAlgorithm::count_neighbors({1, 0, 0}, positions);

    EXPECT_EQ(center_neighbors, 3);  // 3 face-adjacent neighbors
    EXPECT_EQ(edge_neighbors, 1);    // Only center is adjacent
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(FragmentationTest, FragmentSingleBlock) {
    BlockCluster cluster;
    cluster.positions.push_back({0, 0, 0});
    cluster.block_ids.push_back(world::BLOCK_STONE);
    cluster.compute_properties();

    auto fragments = FragmentationAlgorithm::fragment_cluster(cluster, FragmentationPattern::Uniform, 1, 32, rng_);

    EXPECT_EQ(fragments.size(), 1u);
    EXPECT_EQ(fragments[0].size(), 1u);
}

TEST_F(FragmentationTest, FragmentEmptyCluster) {
    BlockCluster cluster;

    auto fragments = FragmentationAlgorithm::fragment_cluster(cluster, FragmentationPattern::Uniform, 1, 32, rng_);

    EXPECT_TRUE(fragments.empty());
}

TEST_F(FragmentationTest, AllFragmentPatternsPreserveBlockCount) {
    auto cluster = create_cube_cluster(4);  // 64 blocks
    size_t original_count = cluster.size();

    std::vector<FragmentationPattern> patterns = {FragmentationPattern::Uniform, FragmentationPattern::Brittle,
                                                  FragmentationPattern::Structural,
                                                  FragmentationPattern::MaterialBased};

    for (auto pattern : patterns) {
        std::mt19937 test_rng(42);
        auto fragments = FragmentationAlgorithm::fragment_cluster(cluster, pattern, 1, 16, test_rng);

        size_t total_blocks = 0;
        for (const auto& frag : fragments) {
            total_blocks += frag.size();
        }

        EXPECT_EQ(total_blocks, original_count) << "Pattern " << static_cast<int>(pattern) << " lost blocks";
    }
}

}  // namespace realcraft::physics::tests
