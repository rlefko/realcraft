// RealCraft Physics Engine Tests
// debris_system_test.cpp - Unit tests for DebrisSystem

#include <gtest/gtest.h>

#include <realcraft/physics/debris_system.hpp>
#include <realcraft/physics/physics_world.hpp>
#include <realcraft/world/block.hpp>
#include <realcraft/world/world_manager.hpp>

namespace realcraft::physics::tests {

class DebrisSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize block registry
        world::BlockRegistry::instance().register_defaults();

        // Create world manager (stub for testing)
        world_manager_ = std::make_unique<world::WorldManager>();
        world_manager_->initialize();

        // Create physics world
        physics_world_ = std::make_unique<PhysicsWorld>();
        physics_world_->initialize(world_manager_.get());
    }

    void TearDown() override {
        physics_world_->shutdown();
        world_manager_->shutdown();
    }

    BlockCluster create_test_cluster(size_t block_count) {
        BlockCluster cluster;
        for (size_t i = 0; i < block_count; ++i) {
            cluster.positions.push_back({static_cast<int64_t>(i), 100, 0});  // High up to fall
            cluster.block_ids.push_back(world::BLOCK_STONE);
        }
        cluster.compute_properties();
        return cluster;
    }

    std::unique_ptr<world::WorldManager> world_manager_;
    std::unique_ptr<PhysicsWorld> physics_world_;
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(DebrisSystemTest, DebrisSystemInitialized) {
    DebrisSystem* debris_system = physics_world_->get_debris_system();
    ASSERT_NE(debris_system, nullptr);
    EXPECT_TRUE(debris_system->is_initialized());
}

TEST_F(DebrisSystemTest, DefaultConfiguration) {
    DebrisSystem* debris_system = physics_world_->get_debris_system();
    const auto& config = debris_system->get_config();

    EXPECT_EQ(config.max_active_debris, 100u);
    EXPECT_EQ(config.max_debris_per_frame, 8u);
    EXPECT_GT(config.default_lifetime_seconds, 0.0);
    EXPECT_GT(config.impact_damage_threshold, 0.0);
}

// ============================================================================
// Debris Spawning Tests
// ============================================================================

TEST_F(DebrisSystemTest, SpawnDebrisFromCluster) {
    DebrisSystem* debris_system = physics_world_->get_debris_system();
    auto cluster = create_test_cluster(10);

    auto handles = debris_system->spawn_debris_from_cluster(cluster);

    EXPECT_FALSE(handles.empty());
    EXPECT_GT(debris_system->active_debris_count(), 0u);
}

TEST_F(DebrisSystemTest, SpawnSingleDebris) {
    DebrisSystem* debris_system = physics_world_->get_debris_system();

    std::vector<world::WorldBlockPos> positions = {{0, 100, 0}, {1, 100, 0}};
    std::vector<world::BlockId> block_ids = {world::BLOCK_STONE, world::BLOCK_STONE};

    auto handle = debris_system->spawn_single_debris(positions, block_ids);

    EXPECT_NE(handle, INVALID_RIGID_BODY);
    EXPECT_TRUE(debris_system->is_debris(handle));
    EXPECT_EQ(debris_system->active_debris_count(), 1u);
}

TEST_F(DebrisSystemTest, SpawnDebrisRespectsMaxLimit) {
    DebrisSystem* debris_system = physics_world_->get_debris_system();

    // Modify config to have low limit
    DebrisConfig config = debris_system->get_config();
    config.max_active_debris = 5;
    debris_system->set_config(config);

    // Try to spawn more than limit
    for (int i = 0; i < 10; ++i) {
        std::vector<world::WorldBlockPos> positions = {{0, 100 + i, 0}};
        std::vector<world::BlockId> block_ids = {world::BLOCK_STONE};
        debris_system->spawn_single_debris(positions, block_ids);
    }

    EXPECT_LE(debris_system->active_debris_count(), 5u);
}

// ============================================================================
// Debris Data Tests
// ============================================================================

TEST_F(DebrisSystemTest, GetDebrisData) {
    DebrisSystem* debris_system = physics_world_->get_debris_system();

    std::vector<world::WorldBlockPos> positions = {{0, 100, 0}};
    std::vector<world::BlockId> block_ids = {world::BLOCK_STONE};

    auto handle = debris_system->spawn_single_debris(positions, block_ids);
    const DebrisData* data = debris_system->get_debris_data(handle);

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->body_handle, handle);
    EXPECT_GT(data->total_mass, 0.0);
    EXPECT_EQ(data->block_count, 1u);
    EXPECT_FALSE(data->marked_for_removal);
}

TEST_F(DebrisSystemTest, IsDebris) {
    DebrisSystem* debris_system = physics_world_->get_debris_system();

    std::vector<world::WorldBlockPos> positions = {{0, 100, 0}};
    std::vector<world::BlockId> block_ids = {world::BLOCK_STONE};

    auto handle = debris_system->spawn_single_debris(positions, block_ids);

    EXPECT_TRUE(debris_system->is_debris(handle));
    EXPECT_FALSE(debris_system->is_debris(INVALID_RIGID_BODY));
    EXPECT_FALSE(debris_system->is_debris(9999));  // Non-existent handle
}

TEST_F(DebrisSystemTest, GetAllDebrisHandles) {
    DebrisSystem* debris_system = physics_world_->get_debris_system();

    // Spawn multiple debris
    for (int i = 0; i < 3; ++i) {
        std::vector<world::WorldBlockPos> positions = {{i, 100, 0}};
        std::vector<world::BlockId> block_ids = {world::BLOCK_STONE};
        debris_system->spawn_single_debris(positions, block_ids);
    }

    auto handles = debris_system->get_all_debris_handles();

    EXPECT_EQ(handles.size(), 3u);
    for (auto handle : handles) {
        EXPECT_TRUE(debris_system->is_debris(handle));
    }
}

// ============================================================================
// Debris Removal Tests
// ============================================================================

TEST_F(DebrisSystemTest, RemoveDebris) {
    DebrisSystem* debris_system = physics_world_->get_debris_system();

    std::vector<world::WorldBlockPos> positions = {{0, 100, 0}};
    std::vector<world::BlockId> block_ids = {world::BLOCK_STONE};

    auto handle = debris_system->spawn_single_debris(positions, block_ids);
    EXPECT_EQ(debris_system->active_debris_count(), 1u);

    debris_system->remove_debris(handle);

    EXPECT_EQ(debris_system->active_debris_count(), 0u);
    EXPECT_FALSE(debris_system->is_debris(handle));
}

TEST_F(DebrisSystemTest, ClearAllDebris) {
    DebrisSystem* debris_system = physics_world_->get_debris_system();

    // Spawn multiple debris
    for (int i = 0; i < 5; ++i) {
        std::vector<world::WorldBlockPos> positions = {{i, 100, 0}};
        std::vector<world::BlockId> block_ids = {world::BLOCK_STONE};
        debris_system->spawn_single_debris(positions, block_ids);
    }

    EXPECT_EQ(debris_system->active_debris_count(), 5u);

    debris_system->clear_all_debris();

    EXPECT_EQ(debris_system->active_debris_count(), 0u);
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(DebrisSystemTest, DebrisLifetimeExpiration) {
    DebrisSystem* debris_system = physics_world_->get_debris_system();

    // Set very short lifetime
    DebrisConfig config = debris_system->get_config();
    config.default_lifetime_seconds = 0.1;
    config.minimum_lifetime_seconds = 0.0;
    debris_system->set_config(config);

    std::vector<world::WorldBlockPos> positions = {{0, 100, 0}};
    std::vector<world::BlockId> block_ids = {world::BLOCK_STONE};

    debris_system->spawn_single_debris(positions, block_ids);
    EXPECT_EQ(debris_system->active_debris_count(), 1u);

    // Simulate time passing
    for (int i = 0; i < 10; ++i) {
        debris_system->update(0.1);  // 1 second total
    }

    // Debris should have expired
    EXPECT_EQ(debris_system->active_debris_count(), 0u);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(DebrisSystemTest, Statistics) {
    DebrisSystem* debris_system = physics_world_->get_debris_system();
    debris_system->reset_stats();

    std::vector<world::WorldBlockPos> positions = {{0, 100, 0}};
    std::vector<world::BlockId> block_ids = {world::BLOCK_STONE};

    debris_system->spawn_single_debris(positions, block_ids);

    auto stats = debris_system->get_stats();

    EXPECT_EQ(stats.active_debris, 1u);
    EXPECT_EQ(stats.total_spawned, 1u);
    EXPECT_EQ(stats.total_despawned, 0u);
}

TEST_F(DebrisSystemTest, StatisticsAfterRemoval) {
    DebrisSystem* debris_system = physics_world_->get_debris_system();
    debris_system->reset_stats();

    std::vector<world::WorldBlockPos> positions = {{0, 100, 0}};
    std::vector<world::BlockId> block_ids = {world::BLOCK_STONE};

    auto handle = debris_system->spawn_single_debris(positions, block_ids);
    debris_system->remove_debris(handle);

    auto stats = debris_system->get_stats();

    EXPECT_EQ(stats.active_debris, 0u);
    EXPECT_EQ(stats.total_spawned, 1u);
    EXPECT_EQ(stats.total_despawned, 1u);
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(DebrisSystemTest, ConfigurationModification) {
    DebrisSystem* debris_system = physics_world_->get_debris_system();

    DebrisConfig new_config;
    new_config.max_active_debris = 50;
    new_config.default_lifetime_seconds = 60.0;
    new_config.impact_damage_threshold = 20.0;

    debris_system->set_config(new_config);

    const auto& config = debris_system->get_config();
    EXPECT_EQ(config.max_active_debris, 50u);
    EXPECT_EQ(config.default_lifetime_seconds, 60.0);
    EXPECT_EQ(config.impact_damage_threshold, 20.0);
}

// ============================================================================
// Impact Callback Tests
// ============================================================================

TEST_F(DebrisSystemTest, ImpactCallbackSet) {
    DebrisSystem* debris_system = physics_world_->get_debris_system();

    bool callback_invoked = false;
    debris_system->set_impact_callback([&callback_invoked](const DebrisImpactEvent& event) {
        callback_invoked = true;
        (void)event;
    });

    // Callback is set (invocation would require actual physics simulation)
    SUCCEED();
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(DebrisSystemTest, SpawnEmptyCluster) {
    DebrisSystem* debris_system = physics_world_->get_debris_system();

    BlockCluster empty_cluster;
    auto handles = debris_system->spawn_debris_from_cluster(empty_cluster);

    EXPECT_TRUE(handles.empty());
    EXPECT_EQ(debris_system->active_debris_count(), 0u);
}

TEST_F(DebrisSystemTest, RemoveNonExistentDebris) {
    DebrisSystem* debris_system = physics_world_->get_debris_system();

    // Should not crash
    debris_system->remove_debris(INVALID_RIGID_BODY);
    debris_system->remove_debris(9999);

    SUCCEED();
}

TEST_F(DebrisSystemTest, GetDataForNonExistentDebris) {
    DebrisSystem* debris_system = physics_world_->get_debris_system();

    const DebrisData* data = debris_system->get_debris_data(9999);
    EXPECT_EQ(data, nullptr);
}

}  // namespace realcraft::physics::tests
