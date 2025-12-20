// RealCraft Physics Engine Tests
// rigid_body_test.cpp - Tests for rigid body dynamics

#include <gtest/gtest.h>

#include <realcraft/physics/rigid_body.hpp>

namespace realcraft::physics {
namespace {

// ============================================================================
// RigidBody Basic Tests
// ============================================================================

class RigidBodyTest : public ::testing::Test {
protected:
    void SetUp() override {
        desc.position = glm::dvec3(5.0, 10.0, 15.0);
        desc.rotation = glm::dquat(1.0, 0.0, 0.0, 0.0);
        desc.mass = 1.0;
        desc.motion_type = MotionType::Dynamic;
        desc.collider_type = ColliderType::AABB;
        desc.half_extents = glm::dvec3(0.5, 0.5, 0.5);
        desc.material = PhysicsMaterial::default_material();
    }

    RigidBodyDesc desc;
};

TEST_F(RigidBodyTest, Construction) {
    RigidBody body(1, desc);

    EXPECT_EQ(body.get_handle(), 1);
    EXPECT_EQ(body.get_motion_type(), MotionType::Dynamic);
    EXPECT_EQ(body.get_collider_type(), ColliderType::AABB);
    EXPECT_NEAR(body.get_mass(), 1.0, 1e-6);
}

TEST_F(RigidBodyTest, Position) {
    RigidBody body(1, desc);

    glm::dvec3 pos = body.get_position();
    EXPECT_NEAR(pos.x, 5.0, 1e-6);
    EXPECT_NEAR(pos.y, 10.0, 1e-6);
    EXPECT_NEAR(pos.z, 15.0, 1e-6);

    body.set_position(glm::dvec3(100.0, 200.0, 300.0));
    pos = body.get_position();
    EXPECT_NEAR(pos.x, 100.0, 1e-6);
    EXPECT_NEAR(pos.y, 200.0, 1e-6);
    EXPECT_NEAR(pos.z, 300.0, 1e-6);
}

TEST_F(RigidBodyTest, Rotation) {
    RigidBody body(1, desc);

    glm::dquat rot = body.get_rotation();
    EXPECT_NEAR(rot.w, 1.0, 1e-6);
    EXPECT_NEAR(rot.x, 0.0, 1e-6);
    EXPECT_NEAR(rot.y, 0.0, 1e-6);
    EXPECT_NEAR(rot.z, 0.0, 1e-6);

    // Rotate 90 degrees around Y axis
    glm::dquat new_rot = glm::angleAxis(glm::radians(90.0), glm::dvec3(0.0, 1.0, 0.0));
    body.set_rotation(new_rot);
    rot = body.get_rotation();
    EXPECT_NEAR(rot.w, new_rot.w, 1e-6);
    EXPECT_NEAR(rot.x, new_rot.x, 1e-6);
    EXPECT_NEAR(rot.y, new_rot.y, 1e-6);
    EXPECT_NEAR(rot.z, new_rot.z, 1e-6);
}

TEST_F(RigidBodyTest, AABB) {
    RigidBody body(1, desc);
    AABB aabb = body.get_aabb();

    EXPECT_NEAR(aabb.min.x, 4.5, 1e-6);
    EXPECT_NEAR(aabb.min.y, 9.5, 1e-6);
    EXPECT_NEAR(aabb.min.z, 14.5, 1e-6);
    EXPECT_NEAR(aabb.max.x, 5.5, 1e-6);
    EXPECT_NEAR(aabb.max.y, 10.5, 1e-6);
    EXPECT_NEAR(aabb.max.z, 15.5, 1e-6);
}

TEST_F(RigidBodyTest, BulletObjectsExist) {
    RigidBody body(1, desc);
    EXPECT_NE(body.get_bullet_body(), nullptr);
    EXPECT_NE(body.get_bullet_shape(), nullptr);
}

// ============================================================================
// Motion Type Tests
// ============================================================================

TEST_F(RigidBodyTest, DynamicMotionType) {
    desc.motion_type = MotionType::Dynamic;
    RigidBody body(1, desc);

    EXPECT_EQ(body.get_motion_type(), MotionType::Dynamic);
    EXPECT_NEAR(body.get_mass(), 1.0, 1e-6);
}

TEST_F(RigidBodyTest, KinematicMotionType) {
    desc.motion_type = MotionType::Kinematic;
    RigidBody body(1, desc);

    EXPECT_EQ(body.get_motion_type(), MotionType::Kinematic);
}

TEST_F(RigidBodyTest, StaticMotionType) {
    desc.motion_type = MotionType::Static;
    RigidBody body(1, desc);

    EXPECT_EQ(body.get_motion_type(), MotionType::Static);
    // Static bodies have zero mass
    EXPECT_NEAR(body.get_mass(), 0.0, 1e-6);
}

// ============================================================================
// Velocity Tests
// ============================================================================

TEST_F(RigidBodyTest, LinearVelocity) {
    RigidBody body(1, desc);

    body.set_linear_velocity(glm::dvec3(10.0, 20.0, 30.0));
    glm::dvec3 vel = body.get_linear_velocity();
    EXPECT_NEAR(vel.x, 10.0, 1e-6);
    EXPECT_NEAR(vel.y, 20.0, 1e-6);
    EXPECT_NEAR(vel.z, 30.0, 1e-6);
}

TEST_F(RigidBodyTest, AngularVelocity) {
    RigidBody body(1, desc);

    body.set_angular_velocity(glm::dvec3(1.0, 2.0, 3.0));
    glm::dvec3 vel = body.get_angular_velocity();
    EXPECT_NEAR(vel.x, 1.0, 1e-6);
    EXPECT_NEAR(vel.y, 2.0, 1e-6);
    EXPECT_NEAR(vel.z, 3.0, 1e-6);
}

TEST_F(RigidBodyTest, KinematicVelocityIgnored) {
    desc.motion_type = MotionType::Kinematic;
    RigidBody body(1, desc);

    // Setting velocity on kinematic body should be ignored
    body.set_linear_velocity(glm::dvec3(10.0, 20.0, 30.0));
    glm::dvec3 vel = body.get_linear_velocity();
    EXPECT_NEAR(vel.x, 0.0, 1e-6);
    EXPECT_NEAR(vel.y, 0.0, 1e-6);
    EXPECT_NEAR(vel.z, 0.0, 1e-6);
}

// ============================================================================
// Force and Impulse Tests
// ============================================================================

TEST_F(RigidBodyTest, ApplyCentralImpulse) {
    RigidBody body(1, desc);

    // Apply upward impulse
    body.apply_central_impulse(glm::dvec3(0.0, 10.0, 0.0));

    // Velocity should change immediately (impulse = mass * delta_velocity)
    // For mass 1.0, impulse 10 = delta_v 10
    glm::dvec3 vel = body.get_linear_velocity();
    EXPECT_NEAR(vel.y, 10.0, 1e-6);
}

TEST_F(RigidBodyTest, ApplyImpulseAtOffset) {
    RigidBody body(1, desc);

    // Apply impulse at offset to create rotation
    body.apply_impulse(glm::dvec3(10.0, 0.0, 0.0), glm::dvec3(0.0, 0.5, 0.0));

    // Should create both linear and angular velocity
    glm::dvec3 linear = body.get_linear_velocity();
    glm::dvec3 angular = body.get_angular_velocity();

    // Linear velocity should change
    EXPECT_GT(std::abs(linear.x), 0.0);
    // Angular velocity should be non-zero (torque was applied)
    EXPECT_GT(glm::length(angular), 0.0);
}

TEST_F(RigidBodyTest, KinematicImpulseIgnored) {
    desc.motion_type = MotionType::Kinematic;
    RigidBody body(1, desc);

    // Impulses on kinematic body should be ignored
    body.apply_central_impulse(glm::dvec3(0.0, 10.0, 0.0));

    glm::dvec3 vel = body.get_linear_velocity();
    EXPECT_NEAR(vel.y, 0.0, 1e-6);
}

// ============================================================================
// Material Tests
// ============================================================================

TEST_F(RigidBodyTest, DefaultMaterial) {
    RigidBody body(1, desc);

    const auto& mat = body.get_material();
    EXPECT_NEAR(mat.friction, 0.5, 1e-6);
    EXPECT_NEAR(mat.restitution, 0.0, 1e-6);
}

TEST_F(RigidBodyTest, SetMaterial) {
    RigidBody body(1, desc);

    PhysicsMaterial bouncy = PhysicsMaterial::bouncy();
    body.set_material(bouncy);

    const auto& mat = body.get_material();
    EXPECT_NEAR(mat.friction, 0.5, 1e-6);
    EXPECT_NEAR(mat.restitution, 0.8, 1e-6);
}

TEST_F(RigidBodyTest, MaterialPresets) {
    EXPECT_NEAR(PhysicsMaterial::ice().friction, 0.05, 1e-6);
    EXPECT_NEAR(PhysicsMaterial::rubber().friction, 0.9, 1e-6);
    EXPECT_NEAR(PhysicsMaterial::metal().friction, 0.4, 1e-6);
    EXPECT_NEAR(PhysicsMaterial::wood().friction, 0.6, 1e-6);
}

// ============================================================================
// Gravity Scale Tests
// ============================================================================

TEST_F(RigidBodyTest, GravityScale) {
    RigidBody body(1, desc);

    EXPECT_NEAR(body.get_gravity_scale(), 1.0, 1e-6);

    body.set_gravity_scale(0.5);
    EXPECT_NEAR(body.get_gravity_scale(), 0.5, 1e-6);

    body.set_gravity_scale(0.0);
    EXPECT_NEAR(body.get_gravity_scale(), 0.0, 1e-6);
}

// ============================================================================
// Interpolation Tests
// ============================================================================

TEST_F(RigidBodyTest, InterpolationAlphaZero) {
    RigidBody body(1, desc);

    // Store initial transform
    body.store_previous_transform();

    // Move to new position
    body.set_position(glm::dvec3(10.0, 20.0, 30.0));

    // At alpha=0, should return previous position (5, 10, 15)
    glm::dvec3 interpolated = body.get_interpolated_position(0.0);
    EXPECT_NEAR(interpolated.x, 5.0, 1e-6);
    EXPECT_NEAR(interpolated.y, 10.0, 1e-6);
    EXPECT_NEAR(interpolated.z, 15.0, 1e-6);
}

TEST_F(RigidBodyTest, InterpolationAlphaOne) {
    RigidBody body(1, desc);

    // Store initial transform
    body.store_previous_transform();

    // Move to new position
    body.set_position(glm::dvec3(10.0, 20.0, 30.0));

    // At alpha=1, should return current position (10, 20, 30)
    glm::dvec3 interpolated = body.get_interpolated_position(1.0);
    EXPECT_NEAR(interpolated.x, 10.0, 1e-6);
    EXPECT_NEAR(interpolated.y, 20.0, 1e-6);
    EXPECT_NEAR(interpolated.z, 30.0, 1e-6);
}

TEST_F(RigidBodyTest, InterpolationAlphaHalf) {
    RigidBody body(1, desc);

    // Store initial transform at (5, 10, 15)
    body.store_previous_transform();

    // Move to new position (15, 30, 45)
    body.set_position(glm::dvec3(15.0, 30.0, 45.0));

    // At alpha=0.5, should return midpoint (10, 20, 30)
    glm::dvec3 interpolated = body.get_interpolated_position(0.5);
    EXPECT_NEAR(interpolated.x, 10.0, 1e-6);
    EXPECT_NEAR(interpolated.y, 20.0, 1e-6);
    EXPECT_NEAR(interpolated.z, 30.0, 1e-6);
}

// ============================================================================
// Sleep State Tests
// ============================================================================

TEST_F(RigidBodyTest, InitiallyAwake) {
    RigidBody body(1, desc);

    // Bodies should start awake
    EXPECT_FALSE(body.is_sleeping());
}

TEST_F(RigidBodyTest, ActivateWakesSleepingBody) {
    RigidBody body(1, desc);

    // Deactivate
    body.deactivate();

    // Activate should wake it up
    body.activate();
    EXPECT_FALSE(body.is_sleeping());
}

// ============================================================================
// User Data Tests
// ============================================================================

TEST_F(RigidBodyTest, UserData) {
    static int test_data = 42;
    desc.user_data = &test_data;

    RigidBody body(1, desc);
    EXPECT_EQ(body.get_user_data(), &test_data);

    static int other_data = 123;
    body.set_user_data(&other_data);
    EXPECT_EQ(body.get_user_data(), &other_data);
}

// ============================================================================
// Collision Layer Tests
// ============================================================================

TEST_F(RigidBodyTest, CollisionLayers) {
    desc.collision_layer = static_cast<uint32_t>(CollisionLayer::Player);
    desc.collision_mask = static_cast<uint32_t>(CollisionLayer::Static | CollisionLayer::Entity);

    RigidBody body(1, desc);

    EXPECT_EQ(body.get_collision_layer(), static_cast<uint32_t>(CollisionLayer::Player));
    EXPECT_EQ(body.get_collision_mask(), static_cast<uint32_t>(CollisionLayer::Static | CollisionLayer::Entity));
}

// ============================================================================
// Collider Type Tests
// ============================================================================

TEST_F(RigidBodyTest, AABBColliderType) {
    desc.collider_type = ColliderType::AABB;
    desc.half_extents = glm::dvec3(1.0, 2.0, 3.0);

    RigidBody body(1, desc);
    EXPECT_EQ(body.get_collider_type(), ColliderType::AABB);
}

TEST_F(RigidBodyTest, CapsuleColliderType) {
    desc.collider_type = ColliderType::Capsule;
    desc.radius = 0.5;
    desc.height = 2.0;

    RigidBody body(1, desc);
    EXPECT_EQ(body.get_collider_type(), ColliderType::Capsule);
}

// ============================================================================
// Origin Shifting Tests
// ============================================================================

TEST_F(RigidBodyTest, ApplyOriginShift) {
    RigidBody body(1, desc);

    // Initial position (5, 10, 15)
    glm::dvec3 shift(100.0, 0.0, 0.0);
    body.apply_origin_shift(shift);

    // Position should be shifted
    glm::dvec3 pos = body.get_position();
    EXPECT_NEAR(pos.x, -95.0, 1e-6);  // 5 - 100 = -95
    EXPECT_NEAR(pos.y, 10.0, 1e-6);
    EXPECT_NEAR(pos.z, 15.0, 1e-6);
}

// ============================================================================
// Move Semantics Tests
// ============================================================================

TEST_F(RigidBodyTest, MoveConstruction) {
    RigidBody body1(1, desc);
    body1.set_position(glm::dvec3(100.0, 200.0, 300.0));

    RigidBody body2(std::move(body1));

    EXPECT_EQ(body2.get_handle(), 1);
    glm::dvec3 pos = body2.get_position();
    EXPECT_NEAR(pos.x, 100.0, 1e-6);
    EXPECT_NEAR(pos.y, 200.0, 1e-6);
    EXPECT_NEAR(pos.z, 300.0, 1e-6);
}

TEST_F(RigidBodyTest, MoveAssignment) {
    RigidBody body1(1, desc);
    body1.set_position(glm::dvec3(100.0, 200.0, 300.0));

    desc.position = glm::dvec3(0.0);
    RigidBody body2(2, desc);

    body2 = std::move(body1);

    EXPECT_EQ(body2.get_handle(), 1);
    glm::dvec3 pos = body2.get_position();
    EXPECT_NEAR(pos.x, 100.0, 1e-6);
    EXPECT_NEAR(pos.y, 200.0, 1e-6);
    EXPECT_NEAR(pos.z, 300.0, 1e-6);
}

}  // namespace
}  // namespace realcraft::physics
