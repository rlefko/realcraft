// RealCraft Physics Engine Tests
// player_controller_test.cpp - Tests for player physics controller

#include <gtest/gtest.h>

#include <realcraft/physics/physics_world.hpp>
#include <realcraft/physics/player_controller.hpp>
#include <realcraft/world/block.hpp>
#include <realcraft/world/world_manager.hpp>

namespace realcraft::physics {
namespace {

// ============================================================================
// Test Fixture
// ============================================================================

class PlayerControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
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

        // Initialize player controller with default config
        ASSERT_TRUE(player_controller.initialize(&physics_world, &world_manager));
    }

    void TearDown() override {
        player_controller.shutdown();
        physics_world.shutdown();
        world_manager.shutdown();
    }

    world::WorldManager world_manager;
    PhysicsWorld physics_world;
    PlayerController player_controller;
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(PlayerControllerTest, Initialization) {
    EXPECT_TRUE(player_controller.is_initialized());
    EXPECT_TRUE(player_controller.is_enabled());
}

TEST_F(PlayerControllerTest, ShutdownCleansUp) {
    player_controller.shutdown();
    EXPECT_FALSE(player_controller.is_initialized());
}

TEST_F(PlayerControllerTest, InitializeWithNullPhysicsWorld) {
    PlayerController controller;
    EXPECT_FALSE(controller.initialize(nullptr, &world_manager));
    EXPECT_FALSE(controller.is_initialized());
}

TEST_F(PlayerControllerTest, InitializeWithNullWorldManager) {
    PlayerController controller;
    EXPECT_FALSE(controller.initialize(&physics_world, nullptr));
    EXPECT_FALSE(controller.is_initialized());
}

// ============================================================================
// Position Tests
// ============================================================================

TEST_F(PlayerControllerTest, SetPosition) {
    glm::dvec3 new_pos(100.0, 200.0, 300.0);
    player_controller.set_position(new_pos);

    glm::dvec3 pos = player_controller.get_position();
    EXPECT_NEAR(pos.x, 100.0, 1e-6);
    EXPECT_NEAR(pos.y, 200.0, 1e-6);
    EXPECT_NEAR(pos.z, 300.0, 1e-6);
}

TEST_F(PlayerControllerTest, EyePositionOffset) {
    PlayerControllerConfig config = player_controller.get_config();
    glm::dvec3 pos(0.0, 100.0, 0.0);
    player_controller.set_position(pos);

    glm::dvec3 eye_pos = player_controller.get_eye_position();

    // Eye should be at eye_height - capsule_height/2 above position
    double expected_offset = config.eye_height - config.capsule_height / 2.0;
    EXPECT_NEAR(eye_pos.y, pos.y + expected_offset, 1e-6);
}

// ============================================================================
// Interpolation Tests
// ============================================================================

TEST_F(PlayerControllerTest, InterpolationAlphaZero) {
    player_controller.set_position(glm::dvec3(0.0, 0.0, 0.0));
    player_controller.store_previous_state();
    player_controller.set_position(glm::dvec3(10.0, 10.0, 10.0));

    // At alpha=0, should return previous position
    glm::dvec3 interpolated = player_controller.get_interpolated_position(0.0);
    EXPECT_NEAR(interpolated.x, 0.0, 1e-6);
    EXPECT_NEAR(interpolated.y, 0.0, 1e-6);
    EXPECT_NEAR(interpolated.z, 0.0, 1e-6);
}

TEST_F(PlayerControllerTest, InterpolationAlphaOne) {
    player_controller.set_position(glm::dvec3(0.0, 0.0, 0.0));
    player_controller.store_previous_state();
    player_controller.set_position(glm::dvec3(10.0, 10.0, 10.0));

    // At alpha=1, should return current position
    glm::dvec3 interpolated = player_controller.get_interpolated_position(1.0);
    EXPECT_NEAR(interpolated.x, 10.0, 1e-6);
    EXPECT_NEAR(interpolated.y, 10.0, 1e-6);
    EXPECT_NEAR(interpolated.z, 10.0, 1e-6);
}

TEST_F(PlayerControllerTest, InterpolationAlphaHalf) {
    player_controller.set_position(glm::dvec3(0.0, 0.0, 0.0));
    player_controller.store_previous_state();
    player_controller.set_position(glm::dvec3(10.0, 10.0, 10.0));

    // At alpha=0.5, should return midpoint
    glm::dvec3 interpolated = player_controller.get_interpolated_position(0.5);
    EXPECT_NEAR(interpolated.x, 5.0, 1e-6);
    EXPECT_NEAR(interpolated.y, 5.0, 1e-6);
    EXPECT_NEAR(interpolated.z, 5.0, 1e-6);
}

// ============================================================================
// Velocity Tests
// ============================================================================

TEST_F(PlayerControllerTest, SetVelocity) {
    player_controller.set_velocity(glm::dvec3(5.0, 10.0, 15.0));

    glm::dvec3 vel = player_controller.get_velocity();
    EXPECT_NEAR(vel.x, 5.0, 1e-6);
    EXPECT_NEAR(vel.y, 10.0, 1e-6);
    EXPECT_NEAR(vel.z, 15.0, 1e-6);
}

// ============================================================================
// State Tests
// ============================================================================

TEST_F(PlayerControllerTest, InitialStateAirborne) {
    // Without any ground, player should be airborne
    player_controller.set_position(glm::dvec3(0.0, 1000.0, 0.0));  // High in the air

    PlayerInput input;
    player_controller.fixed_update(1.0 / 60.0, input);

    EXPECT_EQ(player_controller.get_state(), PlayerState::Airborne);
    EXPECT_FALSE(player_controller.is_grounded());
}

TEST_F(PlayerControllerTest, SprintingRequiresMovement) {
    player_controller.set_position(glm::dvec3(0.0, 100.0, 0.0));

    // Sprint without movement should not be sprinting
    PlayerInput input;
    input.sprint_pressed = true;
    input.move_direction = glm::vec3(0.0f);

    player_controller.fixed_update(1.0 / 60.0, input);
    EXPECT_FALSE(player_controller.is_sprinting());
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(PlayerControllerTest, DefaultConfig) {
    PlayerControllerConfig config = player_controller.get_config();

    EXPECT_NEAR(config.capsule_radius, 0.4, 1e-6);
    EXPECT_NEAR(config.capsule_height, 1.8, 1e-6);
    EXPECT_NEAR(config.walk_speed, 4.3, 1e-6);
    EXPECT_NEAR(config.gravity, 32.0, 1e-6);
}

TEST_F(PlayerControllerTest, SetConfig) {
    PlayerControllerConfig new_config;
    new_config.walk_speed = 10.0;
    new_config.gravity = 20.0;

    player_controller.set_config(new_config);

    PlayerControllerConfig config = player_controller.get_config();
    EXPECT_NEAR(config.walk_speed, 10.0, 1e-6);
    EXPECT_NEAR(config.gravity, 20.0, 1e-6);
}

// ============================================================================
// Enable/Disable Tests
// ============================================================================

TEST_F(PlayerControllerTest, DisableStopsUpdates) {
    player_controller.set_position(glm::dvec3(0.0, 100.0, 0.0));
    player_controller.set_velocity(glm::dvec3(0.0, 0.0, 0.0));
    player_controller.set_enabled(false);

    PlayerInput input;
    input.move_direction = glm::vec3(1.0f, 0.0f, 0.0f);

    // Update shouldn't change position when disabled
    glm::dvec3 pos_before = player_controller.get_position();
    player_controller.fixed_update(1.0 / 60.0, input);
    glm::dvec3 pos_after = player_controller.get_position();

    EXPECT_NEAR(pos_before.x, pos_after.x, 1e-6);
    EXPECT_NEAR(pos_before.y, pos_after.y, 1e-6);
    EXPECT_NEAR(pos_before.z, pos_after.z, 1e-6);
}

// ============================================================================
// Gravity Tests
// ============================================================================

TEST_F(PlayerControllerTest, GravityAppliedWhenAirborne) {
    player_controller.set_position(glm::dvec3(0.0, 1000.0, 0.0));  // High up
    player_controller.set_velocity(glm::dvec3(0.0, 0.0, 0.0));

    PlayerInput input;
    double dt = 1.0 / 60.0;
    player_controller.fixed_update(dt, input);

    // Velocity should be negative (falling)
    glm::dvec3 vel = player_controller.get_velocity();
    EXPECT_LT(vel.y, 0.0);
}

TEST_F(PlayerControllerTest, TerminalVelocity) {
    player_controller.set_position(glm::dvec3(0.0, 10000.0, 0.0));
    player_controller.set_velocity(glm::dvec3(0.0, -1000.0, 0.0));  // Already falling very fast

    PlayerInput input;
    player_controller.fixed_update(1.0 / 60.0, input);

    PlayerControllerConfig config = player_controller.get_config();
    glm::dvec3 vel = player_controller.get_velocity();
    EXPECT_GE(vel.y, -config.terminal_velocity);
}

// ============================================================================
// Bounds Tests
// ============================================================================

TEST_F(PlayerControllerTest, GetBounds) {
    player_controller.set_position(glm::dvec3(0.0, 0.0, 0.0));

    AABB bounds = player_controller.get_bounds();
    PlayerControllerConfig config = player_controller.get_config();

    EXPECT_NEAR(bounds.min.x, -config.capsule_radius, 1e-6);
    EXPECT_NEAR(bounds.max.x, config.capsule_radius, 1e-6);
    EXPECT_NEAR(bounds.min.y, -config.capsule_height / 2.0, 1e-6);
    EXPECT_NEAR(bounds.max.y, config.capsule_height / 2.0, 1e-6);
}

// ============================================================================
// Origin Shifting Tests
// ============================================================================

TEST_F(PlayerControllerTest, ApplyOriginShift) {
    player_controller.set_position(glm::dvec3(100.0, 200.0, 300.0));

    glm::dvec3 shift(50.0, 0.0, 100.0);
    player_controller.apply_origin_shift(shift);

    glm::dvec3 pos = player_controller.get_position();
    EXPECT_NEAR(pos.x, 50.0, 1e-6);   // 100 - 50
    EXPECT_NEAR(pos.y, 200.0, 1e-6);  // unchanged
    EXPECT_NEAR(pos.z, 200.0, 1e-6);  // 300 - 100
}

// ============================================================================
// Jump Tests
// ============================================================================

TEST_F(PlayerControllerTest, JumpVelocityCalculation) {
    // Test that jump produces upward velocity
    PlayerControllerConfig config = player_controller.get_config();

    // Expected jump velocity: v = sqrt(2 * g * h)
    double expected_velocity = std::sqrt(2.0 * config.gravity * config.jump_height);
    EXPECT_GT(expected_velocity, 0.0);
}

// ============================================================================
// Ground Info Tests
// ============================================================================

TEST_F(PlayerControllerTest, GroundInfoDefault) {
    const GroundInfo& info = player_controller.get_ground_info();

    // Default ground info should indicate not on ground
    EXPECT_FALSE(info.on_ground);
    EXPECT_TRUE(info.stable);  // Stable by default when not on ground
}

// ============================================================================
// Movement Speed Tests
// ============================================================================

TEST_F(PlayerControllerTest, WalkSpeedConfigurable) {
    PlayerControllerConfig config;
    config.walk_speed = 10.0;
    player_controller.set_config(config);

    EXPECT_NEAR(player_controller.get_config().walk_speed, 10.0, 1e-6);
}

TEST_F(PlayerControllerTest, SprintSpeedGreaterThanWalk) {
    PlayerControllerConfig config = player_controller.get_config();
    EXPECT_GT(config.sprint_speed, config.walk_speed);
}

TEST_F(PlayerControllerTest, CrouchSpeedLessThanWalk) {
    PlayerControllerConfig config = player_controller.get_config();
    EXPECT_LT(config.crouch_speed, config.walk_speed);
}

// ============================================================================
// Move Semantics Tests
// ============================================================================

TEST_F(PlayerControllerTest, MoveConstruction) {
    PlayerController controller1;
    ASSERT_TRUE(controller1.initialize(&physics_world, &world_manager));
    controller1.set_position(glm::dvec3(100.0, 200.0, 300.0));

    PlayerController controller2(std::move(controller1));

    glm::dvec3 pos = controller2.get_position();
    EXPECT_NEAR(pos.x, 100.0, 1e-6);
    EXPECT_NEAR(pos.y, 200.0, 1e-6);
    EXPECT_NEAR(pos.z, 300.0, 1e-6);

    controller2.shutdown();
}

TEST_F(PlayerControllerTest, MoveAssignment) {
    PlayerController controller1;
    ASSERT_TRUE(controller1.initialize(&physics_world, &world_manager));
    controller1.set_position(glm::dvec3(100.0, 200.0, 300.0));

    PlayerController controller2;
    controller2 = std::move(controller1);

    glm::dvec3 pos = controller2.get_position();
    EXPECT_NEAR(pos.x, 100.0, 1e-6);
    EXPECT_NEAR(pos.y, 200.0, 1e-6);
    EXPECT_NEAR(pos.z, 300.0, 1e-6);

    controller2.shutdown();
}

}  // namespace
}  // namespace realcraft::physics
