// RealCraft Physics Engine Tests
// types_test.cpp - Tests for physics types (AABB, Ray, RayHit)

#include <gtest/gtest.h>

#include <realcraft/physics/types.hpp>

namespace realcraft::physics {
namespace {

// ============================================================================
// AABB Tests
// ============================================================================

class AABBTest : public ::testing::Test {
protected:
    AABB unit_box{glm::dvec3(0.0), glm::dvec3(1.0)};
    AABB offset_box{glm::dvec3(5.0, 5.0, 5.0), glm::dvec3(7.0, 7.0, 7.0)};
};

TEST_F(AABBTest, Construction) {
    AABB aabb(glm::dvec3(1.0, 2.0, 3.0), glm::dvec3(4.0, 5.0, 6.0));
    EXPECT_EQ(aabb.min, glm::dvec3(1.0, 2.0, 3.0));
    EXPECT_EQ(aabb.max, glm::dvec3(4.0, 5.0, 6.0));
}

TEST_F(AABBTest, Center) {
    EXPECT_EQ(unit_box.center(), glm::dvec3(0.5, 0.5, 0.5));
    EXPECT_EQ(offset_box.center(), glm::dvec3(6.0, 6.0, 6.0));
}

TEST_F(AABBTest, Extents) {
    EXPECT_EQ(unit_box.extents(), glm::dvec3(1.0, 1.0, 1.0));
    EXPECT_EQ(offset_box.extents(), glm::dvec3(2.0, 2.0, 2.0));
}

TEST_F(AABBTest, HalfExtents) {
    EXPECT_EQ(unit_box.half_extents(), glm::dvec3(0.5, 0.5, 0.5));
    EXPECT_EQ(offset_box.half_extents(), glm::dvec3(1.0, 1.0, 1.0));
}

TEST_F(AABBTest, ContainsPoint) {
    EXPECT_TRUE(unit_box.contains(glm::dvec3(0.5, 0.5, 0.5)));
    EXPECT_TRUE(unit_box.contains(glm::dvec3(0.0, 0.0, 0.0)));
    EXPECT_TRUE(unit_box.contains(glm::dvec3(1.0, 1.0, 1.0)));
    EXPECT_FALSE(unit_box.contains(glm::dvec3(-0.1, 0.5, 0.5)));
    EXPECT_FALSE(unit_box.contains(glm::dvec3(1.1, 0.5, 0.5)));
}

TEST_F(AABBTest, Intersects) {
    AABB overlapping(glm::dvec3(0.5, 0.5, 0.5), glm::dvec3(1.5, 1.5, 1.5));
    AABB touching(glm::dvec3(1.0, 0.0, 0.0), glm::dvec3(2.0, 1.0, 1.0));
    AABB separate(glm::dvec3(10.0, 10.0, 10.0), glm::dvec3(11.0, 11.0, 11.0));

    EXPECT_TRUE(unit_box.intersects(overlapping));
    EXPECT_TRUE(unit_box.intersects(touching));
    EXPECT_FALSE(unit_box.intersects(separate));
}

TEST_F(AABBTest, Expanded) {
    AABB expanded = unit_box.expanded(0.5);
    EXPECT_EQ(expanded.min, glm::dvec3(-0.5, -0.5, -0.5));
    EXPECT_EQ(expanded.max, glm::dvec3(1.5, 1.5, 1.5));
}

TEST_F(AABBTest, FromCenterExtents) {
    AABB aabb = AABB::from_center_extents(glm::dvec3(5.0, 5.0, 5.0), glm::dvec3(1.0, 1.0, 1.0));
    EXPECT_EQ(aabb.min, glm::dvec3(4.0, 4.0, 4.0));
    EXPECT_EQ(aabb.max, glm::dvec3(6.0, 6.0, 6.0));
}

TEST_F(AABBTest, FromBlockPos) {
    world::WorldBlockPos pos(10, 20, 30);
    AABB aabb = AABB::from_block_pos(pos);
    EXPECT_EQ(aabb.min, glm::dvec3(10.0, 20.0, 30.0));
    EXPECT_EQ(aabb.max, glm::dvec3(11.0, 21.0, 31.0));
}

// ============================================================================
// Ray Tests
// ============================================================================

class RayTest : public ::testing::Test {};

TEST_F(RayTest, Construction) {
    Ray ray(glm::dvec3(0.0, 0.0, 0.0), glm::dvec3(1.0, 0.0, 0.0));
    EXPECT_EQ(ray.origin, glm::dvec3(0.0, 0.0, 0.0));
    EXPECT_NEAR(glm::length(ray.direction), 1.0, 1e-10);  // Should be normalized
}

TEST_F(RayTest, DirectionNormalization) {
    Ray ray(glm::dvec3(0.0), glm::dvec3(2.0, 0.0, 0.0));
    EXPECT_NEAR(ray.direction.x, 1.0, 1e-10);
    EXPECT_NEAR(ray.direction.y, 0.0, 1e-10);
    EXPECT_NEAR(ray.direction.z, 0.0, 1e-10);
}

TEST_F(RayTest, PointAt) {
    Ray ray(glm::dvec3(0.0, 0.0, 0.0), glm::dvec3(1.0, 0.0, 0.0));
    glm::dvec3 point = ray.point_at(5.0);
    EXPECT_NEAR(point.x, 5.0, 1e-10);
    EXPECT_NEAR(point.y, 0.0, 1e-10);
    EXPECT_NEAR(point.z, 0.0, 1e-10);
}

// ============================================================================
// Ray-AABB Intersection Tests
// ============================================================================

class RayAABBIntersectionTest : public ::testing::Test {
protected:
    AABB unit_box{glm::dvec3(0.0), glm::dvec3(1.0)};
};

TEST_F(RayAABBIntersectionTest, HitFromFront) {
    Ray ray(glm::dvec3(-1.0, 0.5, 0.5), glm::dvec3(1.0, 0.0, 0.0));
    double t = ray_aabb_intersection(ray, unit_box);
    EXPECT_GT(t, 0.0);
    EXPECT_NEAR(t, 1.0, 1e-10);
}

TEST_F(RayAABBIntersectionTest, HitFromSide) {
    Ray ray(glm::dvec3(0.5, -1.0, 0.5), glm::dvec3(0.0, 1.0, 0.0));
    double t = ray_aabb_intersection(ray, unit_box);
    EXPECT_GT(t, 0.0);
    EXPECT_NEAR(t, 1.0, 1e-10);
}

TEST_F(RayAABBIntersectionTest, Miss) {
    Ray ray(glm::dvec3(-1.0, 2.0, 0.5), glm::dvec3(1.0, 0.0, 0.0));
    double t = ray_aabb_intersection(ray, unit_box);
    EXPECT_LT(t, 0.0);  // Negative means no hit
}

TEST_F(RayAABBIntersectionTest, RayStartsInside) {
    Ray ray(glm::dvec3(0.5, 0.5, 0.5), glm::dvec3(1.0, 0.0, 0.0));
    double t = ray_aabb_intersection(ray, unit_box);
    // When starting inside, should return distance to exit
    EXPECT_GE(t, 0.0);
}

// ============================================================================
// CollisionLayer Tests
// ============================================================================

class CollisionLayerTest : public ::testing::Test {};

TEST_F(CollisionLayerTest, BitwiseOr) {
    CollisionLayer combined = CollisionLayer::Player | CollisionLayer::Entity;
    EXPECT_EQ(static_cast<uint32_t>(combined),
              static_cast<uint32_t>(CollisionLayer::Player) | static_cast<uint32_t>(CollisionLayer::Entity));
}

TEST_F(CollisionLayerTest, HasLayer) {
    uint32_t mask = static_cast<uint32_t>(CollisionLayer::Player | CollisionLayer::Entity);
    EXPECT_TRUE(has_layer(mask, CollisionLayer::Player));
    EXPECT_TRUE(has_layer(mask, CollisionLayer::Entity));
    EXPECT_FALSE(has_layer(mask, CollisionLayer::Debris));
}

// ============================================================================
// RayHit Tests
// ============================================================================

class RayHitTest : public ::testing::Test {};

TEST_F(RayHitTest, HitBlockCheck) {
    RayHit hit;
    EXPECT_FALSE(hit.hit_block());

    hit.block_position = world::WorldBlockPos(1, 2, 3);
    EXPECT_TRUE(hit.hit_block());
}

TEST_F(RayHitTest, HitColliderCheck) {
    RayHit hit;
    EXPECT_FALSE(hit.hit_collider());

    hit.collider = 42;
    EXPECT_TRUE(hit.hit_collider());
}

}  // namespace
}  // namespace realcraft::physics
