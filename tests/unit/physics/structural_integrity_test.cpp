// RealCraft Physics Engine Tests
// structural_integrity_test.cpp - Tests for structural integrity system

#include <gtest/gtest.h>

#include <realcraft/physics/physics_world.hpp>
#include <realcraft/physics/structural_integrity.hpp>
#include <realcraft/physics/support_graph.hpp>
#include <realcraft/world/block.hpp>
#include <realcraft/world/world_manager.hpp>

namespace realcraft::physics {
namespace {

// ============================================================================
// Test Fixture
// ============================================================================

class StructuralIntegrityTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize block registry
        world::BlockRegistry::instance().register_defaults();

        // Initialize world manager
        world::WorldConfig world_config;
        world_config.name = "test_world";
        world_config.seed = 12345;
        world_config.view_distance = 2;
        world_config.enable_saving = false;
        world_config.generation_threads = 1;
        ASSERT_TRUE(world_manager.initialize(world_config));

        // Initialize physics world
        PhysicsConfig physics_config;
        physics_config.gravity = glm::dvec3(0.0, -9.81, 0.0);
        ASSERT_TRUE(physics_world.initialize(&world_manager, physics_config));
    }

    void TearDown() override {
        physics_world.shutdown();
        world_manager.shutdown();
    }

    world::WorldManager world_manager;
    PhysicsWorld physics_world;
};

// ============================================================================
// SupportGraph Tests
// ============================================================================

class SupportGraphTest : public ::testing::Test {
protected:
    void SetUp() override {
        world::BlockRegistry::instance().register_defaults();

        SupportGraphConfig config;
        config.ground_level = 0;
        config.max_search_blocks = 10000;
        graph.initialize(config);
    }

    void TearDown() override { graph.shutdown(); }

    SupportGraph graph;
};

TEST_F(SupportGraphTest, Initialization) {
    EXPECT_TRUE(graph.is_initialized());
    EXPECT_EQ(graph.tracked_block_count(), 0);
}

TEST_F(SupportGraphTest, AddBlock) {
    world::WorldBlockPos pos(0, 0, 0);
    graph.add_block(pos, world::BlockRegistry::instance().stone_id());
    EXPECT_TRUE(graph.has_block(pos));
    EXPECT_EQ(graph.tracked_block_count(), 1);
}

TEST_F(SupportGraphTest, RemoveBlock) {
    world::WorldBlockPos pos(0, 0, 0);
    graph.add_block(pos, world::BlockRegistry::instance().stone_id());
    EXPECT_TRUE(graph.has_block(pos));

    auto neighbors = graph.remove_block(pos);
    EXPECT_FALSE(graph.has_block(pos));
    EXPECT_EQ(graph.tracked_block_count(), 0);
}

TEST_F(SupportGraphTest, GroundLevelBlockIsSupported) {
    // Block at ground level (y=0) is always supported
    world::WorldBlockPos pos(0, 0, 0);
    graph.add_block(pos, world::BlockRegistry::instance().stone_id());

    std::vector<world::WorldBlockPos> path;
    EXPECT_TRUE(graph.check_ground_connectivity(pos, path));
}

TEST_F(SupportGraphTest, BlockOnGroundIsSupported) {
    // Stack of blocks from ground
    graph.add_block(world::WorldBlockPos(0, 0, 0), world::BlockRegistry::instance().stone_id());
    graph.add_block(world::WorldBlockPos(0, 1, 0), world::BlockRegistry::instance().stone_id());
    graph.add_block(world::WorldBlockPos(0, 2, 0), world::BlockRegistry::instance().stone_id());

    std::vector<world::WorldBlockPos> path;
    EXPECT_TRUE(graph.check_ground_connectivity(world::WorldBlockPos(0, 2, 0), path));
    EXPECT_FALSE(path.empty());
}

TEST_F(SupportGraphTest, FloatingBlockNotSupported) {
    // Single floating block (no connection to ground)
    world::WorldBlockPos pos(0, 10, 0);
    graph.add_block(pos, world::BlockRegistry::instance().stone_id());

    std::vector<world::WorldBlockPos> path;
    EXPECT_FALSE(graph.check_ground_connectivity(pos, path));
}

TEST_F(SupportGraphTest, RemovingBaseCollapsesTower) {
    // Create a tower from ground
    graph.add_block(world::WorldBlockPos(0, 0, 0), world::BlockRegistry::instance().stone_id());
    graph.add_block(world::WorldBlockPos(0, 1, 0), world::BlockRegistry::instance().stone_id());
    graph.add_block(world::WorldBlockPos(0, 2, 0), world::BlockRegistry::instance().stone_id());
    graph.add_block(world::WorldBlockPos(0, 3, 0), world::BlockRegistry::instance().stone_id());

    // Remove base block
    auto affected = graph.remove_block(world::WorldBlockPos(0, 0, 0));
    EXPECT_FALSE(affected.empty());

    // Find unsupported clusters
    auto clusters = graph.find_unsupported_clusters(affected);
    EXPECT_EQ(clusters.size(), 1);
    EXPECT_EQ(clusters[0].size(), 3);  // 3 blocks should collapse
}

TEST_F(SupportGraphTest, BridgeWithSupportDoesNotCollapse) {
    // Create a simple supported bridge:
    //   [2]---[3]---[4]
    //    |         |
    //   [1]       [5]
    //    |         |
    //   [0]       [6]  (ground)

    // Left pillar
    graph.add_block(world::WorldBlockPos(0, 0, 0), world::BlockRegistry::instance().stone_id());
    graph.add_block(world::WorldBlockPos(0, 1, 0), world::BlockRegistry::instance().stone_id());
    graph.add_block(world::WorldBlockPos(0, 2, 0), world::BlockRegistry::instance().stone_id());

    // Bridge
    graph.add_block(world::WorldBlockPos(1, 2, 0), world::BlockRegistry::instance().stone_id());
    graph.add_block(world::WorldBlockPos(2, 2, 0), world::BlockRegistry::instance().stone_id());

    // Right pillar
    graph.add_block(world::WorldBlockPos(2, 1, 0), world::BlockRegistry::instance().stone_id());
    graph.add_block(world::WorldBlockPos(2, 0, 0), world::BlockRegistry::instance().stone_id());

    // All blocks should be supported
    std::vector<world::WorldBlockPos> path;
    EXPECT_TRUE(graph.check_ground_connectivity(world::WorldBlockPos(1, 2, 0), path));
}

TEST_F(SupportGraphTest, SupportDistanceCalculation) {
    // Create vertical column
    for (int y = 0; y <= 5; ++y) {
        graph.add_block(world::WorldBlockPos(0, y, 0), world::BlockRegistry::instance().stone_id());
    }

    // Block at y=5 should have support distance of 5
    float dist = graph.get_support_distance(world::WorldBlockPos(0, 5, 0));
    EXPECT_GT(dist, 0.0f);
    EXPECT_LT(dist, std::numeric_limits<float>::infinity());
}

// ============================================================================
// BlockCluster Tests
// ============================================================================

TEST(BlockClusterTest, EmptyCluster) {
    BlockCluster cluster;
    EXPECT_TRUE(cluster.empty());
    EXPECT_EQ(cluster.size(), 0);
}

TEST(BlockClusterTest, ComputeProperties) {
    world::BlockRegistry::instance().register_defaults();

    BlockCluster cluster;
    cluster.positions = {world::WorldBlockPos(0, 0, 0), world::WorldBlockPos(1, 0, 0), world::WorldBlockPos(0, 1, 0)};
    cluster.block_ids = {world::BlockRegistry::instance().stone_id(), world::BlockRegistry::instance().stone_id(),
                         world::BlockRegistry::instance().stone_id()};

    cluster.compute_properties();

    EXPECT_EQ(cluster.size(), 3);
    EXPECT_GT(cluster.total_mass, 0.0);
    EXPECT_NE(cluster.center_of_mass, glm::dvec3(0.0));
}

// ============================================================================
// StructuralIntegritySystem Tests
// ============================================================================

TEST_F(StructuralIntegrityTest, Initialization) {
    auto* integrity = physics_world.get_structural_integrity();
    ASSERT_NE(integrity, nullptr);
    EXPECT_TRUE(integrity->is_initialized());
    EXPECT_TRUE(integrity->is_enabled());
}

TEST_F(StructuralIntegrityTest, DisableEnable) {
    auto* integrity = physics_world.get_structural_integrity();
    ASSERT_NE(integrity, nullptr);

    integrity->set_enabled(false);
    EXPECT_FALSE(integrity->is_enabled());

    integrity->set_enabled(true);
    EXPECT_TRUE(integrity->is_enabled());
}

TEST_F(StructuralIntegrityTest, ConfigurationAccess) {
    auto* integrity = physics_world.get_structural_integrity();
    ASSERT_NE(integrity, nullptr);

    const auto& config = integrity->get_config();
    EXPECT_GT(config.max_search_blocks, 0u);
    EXPECT_GT(config.max_cluster_size, 0u);
}

TEST_F(StructuralIntegrityTest, StatsTracking) {
    auto* integrity = physics_world.get_structural_integrity();
    ASSERT_NE(integrity, nullptr);

    auto stats = integrity->get_stats();
    EXPECT_EQ(stats.total_collapses, 0);
}

// ============================================================================
// Material Properties Tests
// ============================================================================

TEST(MaterialPropertiesTest, StructuralPropertiesExist) {
    world::MaterialProperties stone = world::MaterialProperties::stone();
    EXPECT_GT(stone.max_support_distance, 0.0f);
    EXPECT_GT(stone.horizontal_cost, 0.0f);
    EXPECT_GE(stone.brittleness, 0.0f);
    EXPECT_LE(stone.brittleness, 1.0f);
    EXPECT_TRUE(stone.requires_support);
}

TEST(MaterialPropertiesTest, WaterDoesNotRequireSupport) {
    world::MaterialProperties water = world::MaterialProperties::water();
    EXPECT_FALSE(water.requires_support);
}

TEST(MaterialPropertiesTest, SandHasLowSupportDistance) {
    world::MaterialProperties sand = world::MaterialProperties::sand();
    world::MaterialProperties stone = world::MaterialProperties::stone();

    EXPECT_LT(sand.max_support_distance, stone.max_support_distance);
}

TEST(MaterialPropertiesTest, GlassIsBrittle) {
    world::MaterialProperties glass = world::MaterialProperties::glass();
    world::MaterialProperties stone = world::MaterialProperties::stone();

    EXPECT_GT(glass.brittleness, stone.brittleness);
}

}  // namespace
}  // namespace realcraft::physics
