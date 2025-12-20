// RealCraft Physics Engine Tests
// colliders_test.cpp - Tests for collider implementations

#include <gtest/gtest.h>

#include <realcraft/physics/colliders.hpp>

namespace realcraft::physics {
namespace {

// ============================================================================
// AABBCollider Tests
// ============================================================================

class AABBColliderTest : public ::testing::Test {
protected:
    void SetUp() override {
        desc.position = glm::dvec3(5.0, 10.0, 15.0);
        desc.half_extents = glm::dvec3(1.0, 2.0, 3.0);
        desc.collision_layer = static_cast<uint32_t>(CollisionLayer::Player);
        desc.collision_mask = static_cast<uint32_t>(CollisionLayer::All);
    }

    AABBColliderDesc desc;
};

TEST_F(AABBColliderTest, Construction) {
    AABBCollider collider(1, desc);

    EXPECT_EQ(collider.get_handle(), 1);
    EXPECT_EQ(collider.get_type(), ColliderType::AABB);
    EXPECT_EQ(collider.get_half_extents(), desc.half_extents);
}

TEST_F(AABBColliderTest, Position) {
    AABBCollider collider(1, desc);

    glm::dvec3 pos = collider.get_position();
    EXPECT_NEAR(pos.x, 5.0, 1e-6);
    EXPECT_NEAR(pos.y, 10.0, 1e-6);
    EXPECT_NEAR(pos.z, 15.0, 1e-6);

    collider.set_position(glm::dvec3(100.0, 200.0, 300.0));
    pos = collider.get_position();
    EXPECT_NEAR(pos.x, 100.0, 1e-6);
    EXPECT_NEAR(pos.y, 200.0, 1e-6);
    EXPECT_NEAR(pos.z, 300.0, 1e-6);
}

TEST_F(AABBColliderTest, AABB) {
    AABBCollider collider(1, desc);
    AABB aabb = collider.get_aabb();

    EXPECT_NEAR(aabb.min.x, 4.0, 1e-6);
    EXPECT_NEAR(aabb.min.y, 8.0, 1e-6);
    EXPECT_NEAR(aabb.min.z, 12.0, 1e-6);
    EXPECT_NEAR(aabb.max.x, 6.0, 1e-6);
    EXPECT_NEAR(aabb.max.y, 12.0, 1e-6);
    EXPECT_NEAR(aabb.max.z, 18.0, 1e-6);
}

TEST_F(AABBColliderTest, CollisionLayers) {
    AABBCollider collider(1, desc);

    EXPECT_EQ(collider.get_collision_layer(), static_cast<uint32_t>(CollisionLayer::Player));
    EXPECT_EQ(collider.get_collision_mask(), static_cast<uint32_t>(CollisionLayer::All));

    collider.set_collision_layer(static_cast<uint32_t>(CollisionLayer::Entity));
    EXPECT_EQ(collider.get_collision_layer(), static_cast<uint32_t>(CollisionLayer::Entity));
}

TEST_F(AABBColliderTest, UserData) {
    int test_data = 42;
    desc.user_data = &test_data;

    AABBCollider collider(1, desc);
    EXPECT_EQ(collider.get_user_data(), &test_data);

    int other_data = 123;
    collider.set_user_data(&other_data);
    EXPECT_EQ(collider.get_user_data(), &other_data);
}

TEST_F(AABBColliderTest, BulletObjectExists) {
    AABBCollider collider(1, desc);
    EXPECT_NE(collider.get_bullet_object(), nullptr);
    EXPECT_NE(collider.get_bullet_shape(), nullptr);
}

// ============================================================================
// CapsuleCollider Tests
// ============================================================================

class CapsuleColliderTest : public ::testing::Test {
protected:
    void SetUp() override {
        desc.position = glm::dvec3(0.0, 1.0, 0.0);
        desc.radius = 0.4;
        desc.height = 1.8;
    }

    CapsuleColliderDesc desc;
};

TEST_F(CapsuleColliderTest, Construction) {
    CapsuleCollider collider(2, desc);

    EXPECT_EQ(collider.get_handle(), 2);
    EXPECT_EQ(collider.get_type(), ColliderType::Capsule);
    EXPECT_NEAR(collider.get_radius(), 0.4, 1e-6);
    EXPECT_NEAR(collider.get_height(), 1.8, 1e-6);
}

TEST_F(CapsuleColliderTest, Position) {
    CapsuleCollider collider(2, desc);

    glm::dvec3 pos = collider.get_position();
    EXPECT_NEAR(pos.x, 0.0, 1e-6);
    EXPECT_NEAR(pos.y, 1.0, 1e-6);
    EXPECT_NEAR(pos.z, 0.0, 1e-6);
}

TEST_F(CapsuleColliderTest, AABB) {
    CapsuleCollider collider(2, desc);
    AABB aabb = collider.get_aabb();

    // Capsule AABB should be approximately radius * 2 wide and height tall
    double half_height = desc.height * 0.5;
    EXPECT_NEAR(aabb.min.x, -desc.radius, 1e-6);
    EXPECT_NEAR(aabb.min.y, 1.0 - half_height, 1e-6);
    EXPECT_NEAR(aabb.max.x, desc.radius, 1e-6);
    EXPECT_NEAR(aabb.max.y, 1.0 + half_height, 1e-6);
}

TEST_F(CapsuleColliderTest, BulletObjectExists) {
    CapsuleCollider collider(2, desc);
    EXPECT_NE(collider.get_bullet_object(), nullptr);
    EXPECT_NE(collider.get_bullet_shape(), nullptr);
}

// ============================================================================
// ConvexHullCollider Tests
// ============================================================================

class ConvexHullColliderTest : public ::testing::Test {
protected:
    void SetUp() override {
        desc.position = glm::dvec3(0.0);
        // Simple tetrahedron
        desc.vertices = {
            glm::vec3(0.0f, 1.0f, 0.0f),   // Top
            glm::vec3(-1.0f, 0.0f, 1.0f),  // Front-left
            glm::vec3(1.0f, 0.0f, 1.0f),   // Front-right
            glm::vec3(0.0f, 0.0f, -1.0f)   // Back
        };
    }

    ConvexHullColliderDesc desc;
};

TEST_F(ConvexHullColliderTest, Construction) {
    ConvexHullCollider collider(3, desc);

    EXPECT_EQ(collider.get_handle(), 3);
    EXPECT_EQ(collider.get_type(), ColliderType::ConvexHull);
}

TEST_F(ConvexHullColliderTest, Position) {
    ConvexHullCollider collider(3, desc);

    glm::dvec3 pos = collider.get_position();
    EXPECT_NEAR(pos.x, 0.0, 1e-6);
    EXPECT_NEAR(pos.y, 0.0, 1e-6);
    EXPECT_NEAR(pos.z, 0.0, 1e-6);

    collider.set_position(glm::dvec3(10.0, 20.0, 30.0));
    pos = collider.get_position();
    EXPECT_NEAR(pos.x, 10.0, 1e-6);
    EXPECT_NEAR(pos.y, 20.0, 1e-6);
    EXPECT_NEAR(pos.z, 30.0, 1e-6);
}

TEST_F(ConvexHullColliderTest, BulletObjectExists) {
    ConvexHullCollider collider(3, desc);
    EXPECT_NE(collider.get_bullet_object(), nullptr);
    EXPECT_NE(collider.get_bullet_shape(), nullptr);
}

// ============================================================================
// Trigger Mode Tests
// ============================================================================

TEST(TriggerModeTest, AABBTrigger) {
    AABBColliderDesc desc;
    desc.is_trigger = true;

    AABBCollider collider(1, desc);
    EXPECT_TRUE(collider.is_trigger());
}

TEST(TriggerModeTest, CapsuleTrigger) {
    CapsuleColliderDesc desc;
    desc.is_trigger = true;

    CapsuleCollider collider(1, desc);
    EXPECT_TRUE(collider.is_trigger());
}

TEST(TriggerModeTest, ConvexHullTrigger) {
    ConvexHullColliderDesc desc;
    desc.is_trigger = true;
    desc.vertices = {glm::vec3(0, 1, 0), glm::vec3(-1, 0, 1), glm::vec3(1, 0, 1), glm::vec3(0, 0, -1)};

    ConvexHullCollider collider(1, desc);
    EXPECT_TRUE(collider.is_trigger());
}

}  // namespace
}  // namespace realcraft::physics
