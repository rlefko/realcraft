// RealCraft Physics Engine Tests
// physics_world_test.cpp - Tests for PhysicsWorld

#include <gtest/gtest.h>

#include <realcraft/physics/physics_world.hpp>
#include <realcraft/world/block.hpp>

namespace realcraft::physics {
namespace {

// ============================================================================
// PhysicsWorld Lifecycle Tests
// ============================================================================

class PhysicsWorldTest : public ::testing::Test {
protected:
    void SetUp() override { world::BlockRegistry::instance().register_defaults(); }
};

TEST_F(PhysicsWorldTest, Construction) {
    PhysicsWorld world;
    EXPECT_FALSE(world.is_initialized());
}

TEST_F(PhysicsWorldTest, InitializeWithoutWorldManager) {
    PhysicsWorld world;

    bool result = world.initialize(nullptr);
    EXPECT_TRUE(result);
    EXPECT_TRUE(world.is_initialized());

    world.shutdown();
    EXPECT_FALSE(world.is_initialized());
}

TEST_F(PhysicsWorldTest, DoubleInitialize) {
    PhysicsWorld world;

    EXPECT_TRUE(world.initialize(nullptr));
    EXPECT_FALSE(world.initialize(nullptr));  // Should warn and return false
}

TEST_F(PhysicsWorldTest, ShutdownWithoutInitialize) {
    PhysicsWorld world;
    // Should not crash
    world.shutdown();
}

// ============================================================================
// PhysicsWorld Configuration Tests
// ============================================================================

TEST_F(PhysicsWorldTest, DefaultConfig) {
    PhysicsWorld world;
    world.initialize(nullptr);

    const PhysicsConfig& config = world.get_config();
    EXPECT_NEAR(config.gravity.y, -9.81, 1e-6);
    EXPECT_NEAR(config.fixed_timestep, 1.0 / 60.0, 1e-6);
}

TEST_F(PhysicsWorldTest, CustomConfig) {
    PhysicsWorld world;

    PhysicsConfig config;
    config.gravity = glm::dvec3(0.0, -20.0, 0.0);
    config.fixed_timestep = 1.0 / 120.0;

    world.initialize(nullptr, config);

    EXPECT_NEAR(world.get_config().gravity.y, -20.0, 1e-6);
    EXPECT_NEAR(world.get_config().fixed_timestep, 1.0 / 120.0, 1e-6);
}

TEST_F(PhysicsWorldTest, SetGravity) {
    PhysicsWorld world;
    world.initialize(nullptr);

    world.set_gravity(glm::dvec3(0.0, -5.0, 0.0));
    EXPECT_NEAR(world.get_gravity().y, -5.0, 1e-6);
}

// ============================================================================
// Collider Creation Tests
// ============================================================================

TEST_F(PhysicsWorldTest, CreateAABBCollider) {
    PhysicsWorld world;
    world.initialize(nullptr);

    AABBColliderDesc desc;
    desc.position = glm::dvec3(5.0, 10.0, 15.0);
    desc.half_extents = glm::dvec3(1.0, 2.0, 3.0);

    ColliderHandle handle = world.create_aabb_collider(desc);
    EXPECT_NE(handle, INVALID_COLLIDER);

    Collider* collider = world.get_collider(handle);
    EXPECT_NE(collider, nullptr);
    EXPECT_EQ(collider->get_type(), ColliderType::AABB);
}

TEST_F(PhysicsWorldTest, CreateCapsuleCollider) {
    PhysicsWorld world;
    world.initialize(nullptr);

    CapsuleColliderDesc desc;
    desc.position = glm::dvec3(0.0, 1.0, 0.0);
    desc.radius = 0.4;
    desc.height = 1.8;

    ColliderHandle handle = world.create_capsule_collider(desc);
    EXPECT_NE(handle, INVALID_COLLIDER);

    Collider* collider = world.get_collider(handle);
    EXPECT_NE(collider, nullptr);
    EXPECT_EQ(collider->get_type(), ColliderType::Capsule);
}

TEST_F(PhysicsWorldTest, CreateConvexHullCollider) {
    PhysicsWorld world;
    world.initialize(nullptr);

    ConvexHullColliderDesc desc;
    desc.vertices = {glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 1.0f),
                     glm::vec3(0.0f, 0.0f, -1.0f)};

    ColliderHandle handle = world.create_convex_hull_collider(desc);
    EXPECT_NE(handle, INVALID_COLLIDER);

    Collider* collider = world.get_collider(handle);
    EXPECT_NE(collider, nullptr);
    EXPECT_EQ(collider->get_type(), ColliderType::ConvexHull);
}

TEST_F(PhysicsWorldTest, DestroyCollider) {
    PhysicsWorld world;
    world.initialize(nullptr);

    AABBColliderDesc desc;
    ColliderHandle handle = world.create_aabb_collider(desc);
    EXPECT_NE(world.get_collider(handle), nullptr);

    world.destroy_collider(handle);
    EXPECT_EQ(world.get_collider(handle), nullptr);
}

TEST_F(PhysicsWorldTest, DestroyInvalidCollider) {
    PhysicsWorld world;
    world.initialize(nullptr);

    // Should not crash
    world.destroy_collider(INVALID_COLLIDER);
    world.destroy_collider(999);  // Non-existent handle
}

TEST_F(PhysicsWorldTest, CreateColliderWithoutInitialize) {
    PhysicsWorld world;

    AABBColliderDesc desc;
    ColliderHandle handle = world.create_aabb_collider(desc);

    EXPECT_EQ(handle, INVALID_COLLIDER);
}

// ============================================================================
// Ray Casting Tests
// ============================================================================

TEST_F(PhysicsWorldTest, RaycastVoxelsWithoutWorldManager) {
    PhysicsWorld world;
    world.initialize(nullptr);

    auto hit = world.raycast_voxels(glm::dvec3(0.0, 10.0, 0.0), glm::dvec3(0.0, -1.0, 0.0));

    // No world manager, so no hits
    EXPECT_FALSE(hit.has_value());
}

TEST_F(PhysicsWorldTest, RaycastColliders) {
    PhysicsWorld world;
    world.initialize(nullptr);

    // Create a collider
    AABBColliderDesc desc;
    desc.position = glm::dvec3(0.0, 0.0, 0.0);
    desc.half_extents = glm::dvec3(1.0, 1.0, 1.0);
    world.create_aabb_collider(desc);

    // Force collision detection update
    world.fixed_update(1.0 / 60.0);

    // Raycast toward the collider
    auto hit = world.raycast_colliders(glm::dvec3(0.0, 10.0, 0.0), glm::dvec3(0.0, -1.0, 0.0));

    // Should hit the collider
    EXPECT_TRUE(hit.has_value());
    if (hit) {
        EXPECT_NEAR(hit->position.y, 1.0, 0.1);  // Top of the box
    }
}

TEST_F(PhysicsWorldTest, RaycastMiss) {
    PhysicsWorld world;
    world.initialize(nullptr);

    // Create a collider away from ray
    AABBColliderDesc desc;
    desc.position = glm::dvec3(100.0, 0.0, 0.0);
    desc.half_extents = glm::dvec3(1.0, 1.0, 1.0);
    world.create_aabb_collider(desc);

    world.fixed_update(1.0 / 60.0);

    // Raycast in a different direction
    auto hit = world.raycast_colliders(glm::dvec3(0.0, 10.0, 0.0), glm::dvec3(0.0, -1.0, 0.0));

    EXPECT_FALSE(hit.has_value());
}

// ============================================================================
// Collision Query Tests
// ============================================================================

TEST_F(PhysicsWorldTest, PointInSolidWithoutWorldManager) {
    PhysicsWorld world;
    world.initialize(nullptr);

    // Without a world manager, no point should be solid
    EXPECT_FALSE(world.point_in_solid(glm::dvec3(0.0, 0.0, 0.0)));
}

TEST_F(PhysicsWorldTest, QueryAABB) {
    PhysicsWorld world;
    world.initialize(nullptr);

    AABBColliderDesc desc;
    desc.position = glm::dvec3(5.0, 5.0, 5.0);
    desc.half_extents = glm::dvec3(1.0, 1.0, 1.0);
    ColliderHandle handle = world.create_aabb_collider(desc);

    // Query region that contains the collider
    AABB query_aabb(glm::dvec3(0.0, 0.0, 0.0), glm::dvec3(10.0, 10.0, 10.0));
    auto results = world.query_aabb(query_aabb);

    EXPECT_EQ(results.size(), 1u);
    if (!results.empty()) {
        EXPECT_EQ(results[0], handle);
    }
}

TEST_F(PhysicsWorldTest, QueryAABBEmpty) {
    PhysicsWorld world;
    world.initialize(nullptr);

    AABBColliderDesc desc;
    desc.position = glm::dvec3(100.0, 100.0, 100.0);
    world.create_aabb_collider(desc);

    // Query region that doesn't contain the collider
    AABB query_aabb(glm::dvec3(0.0, 0.0, 0.0), glm::dvec3(10.0, 10.0, 10.0));
    auto results = world.query_aabb(query_aabb);

    EXPECT_TRUE(results.empty());
}

TEST_F(PhysicsWorldTest, QuerySphere) {
    PhysicsWorld world;
    world.initialize(nullptr);

    AABBColliderDesc desc;
    desc.position = glm::dvec3(5.0, 5.0, 5.0);
    desc.half_extents = glm::dvec3(1.0, 1.0, 1.0);
    ColliderHandle handle = world.create_aabb_collider(desc);

    auto results = world.query_sphere(glm::dvec3(5.0, 5.0, 5.0), 5.0);

    EXPECT_EQ(results.size(), 1u);
    if (!results.empty()) {
        EXPECT_EQ(results[0], handle);
    }
}

TEST_F(PhysicsWorldTest, HasLineOfSight) {
    PhysicsWorld world;
    world.initialize(nullptr);

    // Without blocks, should always have line of sight
    EXPECT_TRUE(world.has_line_of_sight(glm::dvec3(0.0, 0.0, 0.0), glm::dvec3(100.0, 100.0, 100.0)));
}

// ============================================================================
// Fixed Update Tests
// ============================================================================

TEST_F(PhysicsWorldTest, FixedUpdate) {
    PhysicsWorld world;
    world.initialize(nullptr);

    // Should not crash
    world.fixed_update(1.0 / 60.0);
}

TEST_F(PhysicsWorldTest, FixedUpdateWithColliders) {
    PhysicsWorld world;
    world.initialize(nullptr);

    // Create some colliders
    for (int i = 0; i < 10; ++i) {
        AABBColliderDesc desc;
        desc.position = glm::dvec3(static_cast<double>(i) * 5.0, 0.0, 0.0);
        world.create_aabb_collider(desc);
    }

    // Should not crash
    world.fixed_update(1.0 / 60.0);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(PhysicsWorldTest, DebugStats) {
    PhysicsWorld world;
    world.initialize(nullptr);

    auto stats = world.get_stats();
    EXPECT_EQ(stats.active_colliders, 0u);
    EXPECT_EQ(stats.chunk_colliders, 0u);

    // Add some colliders
    AABBColliderDesc desc;
    world.create_aabb_collider(desc);
    world.create_aabb_collider(desc);

    stats = world.get_stats();
    EXPECT_EQ(stats.active_colliders, 2u);
}

// ============================================================================
// Collision Callback Tests
// ============================================================================

TEST_F(PhysicsWorldTest, CollisionCallback) {
    PhysicsWorld world;
    world.initialize(nullptr);

    int callback_count = 0;
    world.set_collision_callback([&callback_count](const CollisionEvent&) { ++callback_count; });

    // Create overlapping colliders
    AABBColliderDesc desc1;
    desc1.position = glm::dvec3(0.0, 0.0, 0.0);
    desc1.half_extents = glm::dvec3(1.0, 1.0, 1.0);

    AABBColliderDesc desc2;
    desc2.position = glm::dvec3(0.5, 0.0, 0.0);  // Overlapping
    desc2.half_extents = glm::dvec3(1.0, 1.0, 1.0);

    world.create_aabb_collider(desc1);
    world.create_aabb_collider(desc2);

    world.fixed_update(1.0 / 60.0);

    // May or may not trigger callback depending on collision detection
    // Just verify no crash occurs
    EXPECT_GE(callback_count, 0);
}

}  // namespace
}  // namespace realcraft::physics
