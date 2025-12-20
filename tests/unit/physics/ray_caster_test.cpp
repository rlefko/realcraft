// RealCraft Physics Engine Tests
// ray_caster_test.cpp - Tests for voxel ray casting

#include <gtest/gtest.h>

#include <realcraft/physics/ray_caster.hpp>

namespace realcraft::physics {
namespace {

// ============================================================================
// VoxelRayCaster Tests
// ============================================================================

class VoxelRayCasterTest : public ::testing::Test {
protected:
    void SetUp() override {
        ray_caster = std::make_unique<VoxelRayCaster>();
        // Note: Without a WorldManager, casts will return no hits
    }

    std::unique_ptr<VoxelRayCaster> ray_caster;
};

TEST_F(VoxelRayCasterTest, Construction) {
    EXPECT_NE(ray_caster, nullptr);
}

TEST_F(VoxelRayCasterTest, CastWithoutWorldManager) {
    Ray ray(glm::dvec3(0.0, 10.0, 0.0), glm::dvec3(0.0, -1.0, 0.0));
    auto hit = ray_caster->cast(ray, 100.0);

    // Without a world manager, should return no hit
    EXPECT_FALSE(hit.has_value());
}

TEST_F(VoxelRayCasterTest, CastAllWithoutWorldManager) {
    Ray ray(glm::dvec3(0.0, 10.0, 0.0), glm::dvec3(0.0, -1.0, 0.0));
    auto hits = ray_caster->cast_all(ray, 100.0, 10);

    EXPECT_TRUE(hits.empty());
}

TEST_F(VoxelRayCasterTest, HasLineOfSightWithoutWorldManager) {
    // Without solid blocks, should always have line of sight
    bool los = ray_caster->has_line_of_sight(glm::dvec3(0.0, 0.0, 0.0), glm::dvec3(100.0, 100.0, 100.0));

    EXPECT_TRUE(los);
}

TEST_F(VoxelRayCasterTest, CastFilteredWithoutWorldManager) {
    Ray ray(glm::dvec3(0.0, 10.0, 0.0), glm::dvec3(0.0, -1.0, 0.0));
    auto filter = [](world::BlockId, const world::WorldBlockPos&) { return true; };

    auto hit = ray_caster->cast_filtered(ray, 100.0, filter);
    EXPECT_FALSE(hit.has_value());
}

// ============================================================================
// Ray Direction Tests
// ============================================================================

TEST_F(VoxelRayCasterTest, RayNormalization) {
    Ray ray(glm::dvec3(0.0), glm::dvec3(10.0, 0.0, 0.0));

    // Direction should be normalized
    EXPECT_NEAR(glm::length(ray.direction), 1.0, 1e-10);
    EXPECT_NEAR(ray.direction.x, 1.0, 1e-10);
}

TEST_F(VoxelRayCasterTest, RayPointAt) {
    Ray ray(glm::dvec3(5.0, 10.0, 15.0), glm::dvec3(1.0, 0.0, 0.0));

    glm::dvec3 point = ray.point_at(3.0);
    EXPECT_NEAR(point.x, 8.0, 1e-10);
    EXPECT_NEAR(point.y, 10.0, 1e-10);
    EXPECT_NEAR(point.z, 15.0, 1e-10);
}

// ============================================================================
// Same Point Line of Sight Test
// ============================================================================

TEST_F(VoxelRayCasterTest, SamePointLineOfSight) {
    glm::dvec3 point(5.0, 10.0, 15.0);
    bool los = ray_caster->has_line_of_sight(point, point);

    // Same point should always have line of sight
    EXPECT_TRUE(los);
}

}  // namespace
}  // namespace realcraft::physics
