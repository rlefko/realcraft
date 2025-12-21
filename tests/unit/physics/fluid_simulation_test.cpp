// RealCraft Physics Engine Tests
// fluid_simulation_test.cpp - Tests for fluid simulation system

#include <gtest/gtest.h>

#include <realcraft/physics/fluid_simulation.hpp>
#include <realcraft/physics/physics_world.hpp>
#include <realcraft/world/block.hpp>
#include <realcraft/world/world_manager.hpp>

namespace realcraft::physics {
namespace {

// ============================================================================
// Water State Encoding Tests
// ============================================================================

TEST(WaterStateTest, LevelEncoding) {
    world::BlockStateId state = 0;

    // Test setting and getting level
    world::water_set_level(state, 15);
    EXPECT_EQ(world::water_get_level(state), 15);

    world::water_set_level(state, 8);
    EXPECT_EQ(world::water_get_level(state), 8);

    world::water_set_level(state, 0);
    EXPECT_EQ(world::water_get_level(state), 0);

    // Test boundary values
    world::water_set_level(state, 255);  // Should mask to 15
    EXPECT_EQ(world::water_get_level(state), 15);
}

TEST(WaterStateTest, FlowDirectionEncoding) {
    world::BlockStateId state = 0;

    // Test all flow directions
    world::water_set_flow_direction(state, world::WaterFlowDirection::None);
    EXPECT_EQ(world::water_get_flow_direction(state), world::WaterFlowDirection::None);

    world::water_set_flow_direction(state, world::WaterFlowDirection::PosX);
    EXPECT_EQ(world::water_get_flow_direction(state), world::WaterFlowDirection::PosX);

    world::water_set_flow_direction(state, world::WaterFlowDirection::NegY);
    EXPECT_EQ(world::water_get_flow_direction(state), world::WaterFlowDirection::NegY);

    // Verify level is not affected
    world::water_set_level(state, 10);
    world::water_set_flow_direction(state, world::WaterFlowDirection::PosZ);
    EXPECT_EQ(world::water_get_level(state), 10);
    EXPECT_EQ(world::water_get_flow_direction(state), world::WaterFlowDirection::PosZ);
}

TEST(WaterStateTest, SourceFlagEncoding) {
    world::BlockStateId state = 0;

    EXPECT_FALSE(world::water_is_source(state));

    world::water_set_source(state, true);
    EXPECT_TRUE(world::water_is_source(state));

    world::water_set_source(state, false);
    EXPECT_FALSE(world::water_is_source(state));

    // Verify other fields are not affected
    world::water_set_level(state, 15);
    world::water_set_flow_direction(state, world::WaterFlowDirection::NegX);
    world::water_set_source(state, true);

    EXPECT_EQ(world::water_get_level(state), 15);
    EXPECT_EQ(world::water_get_flow_direction(state), world::WaterFlowDirection::NegX);
    EXPECT_TRUE(world::water_is_source(state));
}

TEST(WaterStateTest, PressureEncoding) {
    world::BlockStateId state = 0;

    world::water_set_pressure(state, 0);
    EXPECT_EQ(world::water_get_pressure(state), 0);

    world::water_set_pressure(state, 15);
    EXPECT_EQ(world::water_get_pressure(state), 15);

    world::water_set_pressure(state, 8);
    EXPECT_EQ(world::water_get_pressure(state), 8);

    // Verify other fields are not affected
    world::water_set_level(state, 12);
    world::water_set_source(state, true);
    world::water_set_pressure(state, 5);

    EXPECT_EQ(world::water_get_level(state), 12);
    EXPECT_TRUE(world::water_is_source(state));
    EXPECT_EQ(world::water_get_pressure(state), 5);
}

TEST(WaterStateTest, MakeState) {
    world::BlockStateId state = world::water_make_state(15, true, world::WaterFlowDirection::NegY, 10);

    EXPECT_EQ(world::water_get_level(state), 15);
    EXPECT_TRUE(world::water_is_source(state));
    EXPECT_EQ(world::water_get_flow_direction(state), world::WaterFlowDirection::NegY);
    EXPECT_EQ(world::water_get_pressure(state), 10);
}

TEST(WaterStateTest, FlowToOffset) {
    EXPECT_EQ(world::water_flow_to_offset(world::WaterFlowDirection::None), glm::ivec3(0, 0, 0));
    EXPECT_EQ(world::water_flow_to_offset(world::WaterFlowDirection::PosX), glm::ivec3(1, 0, 0));
    EXPECT_EQ(world::water_flow_to_offset(world::WaterFlowDirection::NegX), glm::ivec3(-1, 0, 0));
    EXPECT_EQ(world::water_flow_to_offset(world::WaterFlowDirection::PosZ), glm::ivec3(0, 0, 1));
    EXPECT_EQ(world::water_flow_to_offset(world::WaterFlowDirection::NegZ), glm::ivec3(0, 0, -1));
    EXPECT_EQ(world::water_flow_to_offset(world::WaterFlowDirection::NegY), glm::ivec3(0, -1, 0));
    EXPECT_EQ(world::water_flow_to_offset(world::WaterFlowDirection::PosY), glm::ivec3(0, 1, 0));
}

// ============================================================================
// Fluid Cell Info Tests
// ============================================================================

TEST(FluidCellInfoTest, DefaultValues) {
    FluidCellInfo info;

    EXPECT_EQ(info.level, 0);
    EXPECT_EQ(info.flow_direction, world::WaterFlowDirection::None);
    EXPECT_EQ(info.pressure, 0);
    EXPECT_FALSE(info.is_source);
    EXPECT_FALSE(info.is_valid);
}

TEST(FluidCellInfoTest, FlowVelocityNoFlow) {
    FluidCellInfo info;
    info.is_valid = true;
    info.flow_direction = world::WaterFlowDirection::None;

    auto velocity = info.get_flow_velocity();
    EXPECT_DOUBLE_EQ(velocity.x, 0.0);
    EXPECT_DOUBLE_EQ(velocity.y, 0.0);
    EXPECT_DOUBLE_EQ(velocity.z, 0.0);
}

TEST(FluidCellInfoTest, FlowVelocityWithDirection) {
    FluidCellInfo info;
    info.is_valid = true;
    info.flow_direction = world::WaterFlowDirection::PosX;
    info.pressure = 0;

    auto velocity = info.get_flow_velocity(1.0);
    EXPECT_DOUBLE_EQ(velocity.x, 1.0);
    EXPECT_DOUBLE_EQ(velocity.y, 0.0);
    EXPECT_DOUBLE_EQ(velocity.z, 0.0);
}

TEST(FluidCellInfoTest, FlowVelocityWithPressure) {
    FluidCellInfo info;
    info.is_valid = true;
    info.flow_direction = world::WaterFlowDirection::NegY;
    info.pressure = 15;  // Max pressure

    auto velocity = info.get_flow_velocity(1.0);
    EXPECT_DOUBLE_EQ(velocity.x, 0.0);
    EXPECT_DOUBLE_EQ(velocity.y, -2.0);  // 1.0 * (1 + 15/15) = 2.0
    EXPECT_DOUBLE_EQ(velocity.z, 0.0);
}

// ============================================================================
// Fluid Simulation Config Tests
// ============================================================================

TEST(FluidSimulationConfigTest, DefaultValues) {
    FluidSimulationConfig config;

    EXPECT_GT(config.flow_rate, 0.0);
    EXPECT_GT(config.gravity_flow_speed, 0.0);
    EXPECT_EQ(config.source_creation_threshold, 2);
    EXPECT_EQ(config.full_level, 15);
    EXPECT_GT(config.water_density, 0.0);
    EXPECT_GT(config.buoyancy_scale, 0.0);
    EXPECT_GT(config.max_updates_per_frame, 0u);
}

// ============================================================================
// FluidSimulation System Tests
// ============================================================================

class FluidSimulationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize block registry
        world::BlockRegistry::instance().register_defaults();

        // Initialize world manager with minimal config
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

TEST_F(FluidSimulationTest, SystemExists) {
    FluidSimulation* fluid = physics_world.get_fluid_simulation();
    ASSERT_NE(fluid, nullptr);
    EXPECT_TRUE(fluid->is_initialized());
    EXPECT_TRUE(fluid->is_enabled());
}

TEST_F(FluidSimulationTest, GetConfigReturnsDefaults) {
    FluidSimulation* fluid = physics_world.get_fluid_simulation();
    ASSERT_NE(fluid, nullptr);

    const auto& config = fluid->get_config();
    EXPECT_EQ(config.full_level, 15);
    EXPECT_GT(config.water_density, 0.0);
}

TEST_F(FluidSimulationTest, SetEnabled) {
    FluidSimulation* fluid = physics_world.get_fluid_simulation();
    ASSERT_NE(fluid, nullptr);

    EXPECT_TRUE(fluid->is_enabled());

    fluid->set_enabled(false);
    EXPECT_FALSE(fluid->is_enabled());

    fluid->set_enabled(true);
    EXPECT_TRUE(fluid->is_enabled());
}

TEST_F(FluidSimulationTest, NoWaterAtEmptyPosition) {
    FluidSimulation* fluid = physics_world.get_fluid_simulation();
    ASSERT_NE(fluid, nullptr);

    // Check position far from any generated terrain
    world::WorldBlockPos pos(10000, 100, 10000);
    EXPECT_FALSE(fluid->is_water_at(pos));
    EXPECT_EQ(fluid->get_water_level(pos), 0);
}

TEST_F(FluidSimulationTest, PlaceWaterSource) {
    FluidSimulation* fluid = physics_world.get_fluid_simulation();
    ASSERT_NE(fluid, nullptr);

    // Place a water source block - use position that we can force load
    world::WorldBlockPos pos(8, 100, 8);
    world::ChunkPos chunk_pos = world::world_to_chunk(pos);

    // Force load the chunk
    world::Chunk* chunk = world_manager.load_chunk_sync(chunk_pos);
    ASSERT_NE(chunk, nullptr);

    // First ensure position is air
    world_manager.set_block(pos, world::BLOCK_AIR, 0);

    // Place water
    bool success = fluid->place_water(pos, 15, true);
    EXPECT_TRUE(success);

    // Verify water exists
    EXPECT_TRUE(fluid->is_water_at(pos));
    EXPECT_EQ(fluid->get_water_level(pos), 15);
    EXPECT_TRUE(fluid->is_source_block(pos));
}

TEST_F(FluidSimulationTest, RemoveWater) {
    FluidSimulation* fluid = physics_world.get_fluid_simulation();
    ASSERT_NE(fluid, nullptr);

    world::WorldBlockPos pos(9, 100, 9);
    world::ChunkPos chunk_pos = world::world_to_chunk(pos);

    // Force load the chunk
    world::Chunk* chunk = world_manager.load_chunk_sync(chunk_pos);
    ASSERT_NE(chunk, nullptr);

    // Place and then remove water
    world_manager.set_block(pos, world::BLOCK_AIR, 0);
    fluid->place_water(pos, 15, true);
    EXPECT_TRUE(fluid->is_water_at(pos));

    bool removed = fluid->remove_water(pos);
    EXPECT_TRUE(removed);
    EXPECT_FALSE(fluid->is_water_at(pos));
}

TEST_F(FluidSimulationTest, GetFluidInfo) {
    FluidSimulation* fluid = physics_world.get_fluid_simulation();
    ASSERT_NE(fluid, nullptr);

    world::WorldBlockPos pos(10, 100, 10);
    world::ChunkPos chunk_pos = world::world_to_chunk(pos);

    // Force load the chunk
    world::Chunk* chunk = world_manager.load_chunk_sync(chunk_pos);
    ASSERT_NE(chunk, nullptr);

    world_manager.set_block(pos, world::BLOCK_AIR, 0);
    fluid->place_water(pos, 12, false);

    FluidCellInfo info = fluid->get_fluid_info(pos);
    EXPECT_TRUE(info.is_valid);
    EXPECT_EQ(info.level, 12);
    EXPECT_FALSE(info.is_source);
}

TEST_F(FluidSimulationTest, BuoyancyForceZeroWhenNotSubmerged) {
    FluidSimulation* fluid = physics_world.get_fluid_simulation();
    ASSERT_NE(fluid, nullptr);

    // Create a rigid body in the air
    RigidBodyDesc desc;
    desc.position = glm::dvec3(2000.0, 200.0, 2000.0);
    desc.half_extents = glm::dvec3(0.5);
    desc.mass = 10.0;
    desc.motion_type = MotionType::Dynamic;
    desc.collider_type = ColliderType::AABB;

    RigidBodyHandle handle = physics_world.create_rigid_body(desc);
    ASSERT_NE(handle, INVALID_RIGID_BODY);

    const RigidBody* body = physics_world.get_rigid_body(handle);
    ASSERT_NE(body, nullptr);

    // No water around - buoyancy should be zero
    glm::dvec3 buoyancy = fluid->calculate_buoyancy_force(*body);
    EXPECT_NEAR(buoyancy.x, 0.0, 0.001);
    EXPECT_NEAR(buoyancy.y, 0.0, 0.001);
    EXPECT_NEAR(buoyancy.z, 0.0, 0.001);
}

TEST_F(FluidSimulationTest, SubmergedFractionZeroOutOfWater) {
    FluidSimulation* fluid = physics_world.get_fluid_simulation();
    ASSERT_NE(fluid, nullptr);

    // AABB in the air
    AABB bounds(glm::dvec3(3000.0, 200.0, 3000.0), glm::dvec3(3001.0, 201.0, 3001.0));

    double fraction = fluid->get_submerged_fraction(bounds);
    EXPECT_NEAR(fraction, 0.0, 0.01);
}

TEST_F(FluidSimulationTest, CurrentVelocityZeroWithNoWater) {
    FluidSimulation* fluid = physics_world.get_fluid_simulation();
    ASSERT_NE(fluid, nullptr);

    world::WorldPos pos(4000.0, 200.0, 4000.0);
    glm::dvec3 current = fluid->get_current_velocity(pos);

    EXPECT_NEAR(current.x, 0.0, 0.001);
    EXPECT_NEAR(current.y, 0.0, 0.001);
    EXPECT_NEAR(current.z, 0.0, 0.001);
}

TEST_F(FluidSimulationTest, StatsTracking) {
    FluidSimulation* fluid = physics_world.get_fluid_simulation();
    ASSERT_NE(fluid, nullptr);

    FluidSimulation::Stats stats = fluid->get_stats();

    // Initial stats should show system is ready
    EXPECT_GE(stats.active_water_blocks, 0u);
    EXPECT_GE(stats.pending_updates, 0u);
}

TEST_F(FluidSimulationTest, UpdateDoesNotCrash) {
    FluidSimulation* fluid = physics_world.get_fluid_simulation();
    ASSERT_NE(fluid, nullptr);

    // Run several update cycles
    for (int i = 0; i < 10; ++i) {
        fluid->update(0.05);  // 50ms per update
    }

    // System should still be functional
    EXPECT_TRUE(fluid->is_initialized());
}

TEST_F(FluidSimulationTest, ConfigCanBeModified) {
    FluidSimulation* fluid = physics_world.get_fluid_simulation();
    ASSERT_NE(fluid, nullptr);

    FluidSimulationConfig new_config;
    new_config.water_density = 500.0;  // Half normal density
    new_config.buoyancy_scale = 2.0;

    fluid->set_config(new_config);

    const auto& config = fluid->get_config();
    EXPECT_DOUBLE_EQ(config.water_density, 500.0);
    EXPECT_DOUBLE_EQ(config.buoyancy_scale, 2.0);
}

}  // namespace
}  // namespace realcraft::physics
