// RealCraft Rendering Tests
// frustum_test.cpp - Unit tests for Frustum culling

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include <realcraft/rendering/frustum.hpp>

namespace realcraft::rendering::test {

class FrustumTest : public ::testing::Test {
protected:
    void SetUp() override {
        frustum_ = std::make_unique<Frustum>();

        // Create a standard view-projection matrix
        // Camera at origin, looking down -Z
        view_ = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        // 90 degree FOV, 1:1 aspect, near=0.1, far=100
        proj_ = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
    }

    std::unique_ptr<Frustum> frustum_;
    glm::mat4 view_;
    glm::mat4 proj_;
};

TEST_F(FrustumTest, ExtractPlanes) {
    frustum_->extract_from_matrix(proj_ * view_);

    // After extraction, we should have valid planes
    // Test by checking visibility of a box in front of camera
    AABB box_in_front;
    box_in_front.min = glm::vec3(-1.0f, -1.0f, -10.0f);
    box_in_front.max = glm::vec3(1.0f, 1.0f, -5.0f);

    EXPECT_TRUE(frustum_->is_visible(box_in_front));
}

TEST_F(FrustumTest, BoxInFrontVisible) {
    frustum_->extract_from_matrix(proj_ * view_);

    // Box directly in front of camera
    AABB box;
    box.min = glm::vec3(-1.0f, -1.0f, -20.0f);
    box.max = glm::vec3(1.0f, 1.0f, -10.0f);

    EXPECT_TRUE(frustum_->is_visible(box));
}

TEST_F(FrustumTest, BoxBehindCameraNotVisible) {
    frustum_->extract_from_matrix(proj_ * view_);

    // Box behind the camera (positive Z since camera looks at -Z)
    AABB box;
    box.min = glm::vec3(-1.0f, -1.0f, 5.0f);
    box.max = glm::vec3(1.0f, 1.0f, 10.0f);

    EXPECT_FALSE(frustum_->is_visible(box));
}

TEST_F(FrustumTest, BoxFarBeyondFarPlaneNotVisible) {
    frustum_->extract_from_matrix(proj_ * view_);

    // Box beyond far plane (far = 100)
    AABB box;
    box.min = glm::vec3(-1.0f, -1.0f, -200.0f);
    box.max = glm::vec3(1.0f, 1.0f, -150.0f);

    EXPECT_FALSE(frustum_->is_visible(box));
}

TEST_F(FrustumTest, BoxOffToLeftNotVisible) {
    frustum_->extract_from_matrix(proj_ * view_);

    // Box far to the left of the frustum
    AABB box;
    box.min = glm::vec3(-100.0f, -1.0f, -20.0f);
    box.max = glm::vec3(-50.0f, 1.0f, -10.0f);

    EXPECT_FALSE(frustum_->is_visible(box));
}

TEST_F(FrustumTest, BoxPartiallyInFrustumVisible) {
    frustum_->extract_from_matrix(proj_ * view_);

    // Large box that intersects the frustum
    AABB box;
    box.min = glm::vec3(-10.0f, -10.0f, -50.0f);
    box.max = glm::vec3(10.0f, 10.0f, 5.0f);  // Extends behind camera

    EXPECT_TRUE(frustum_->is_visible(box));
}

TEST_F(FrustumTest, BoxEnclosingFrustumVisible) {
    frustum_->extract_from_matrix(proj_ * view_);

    // Very large box that encloses the entire frustum
    AABB box;
    box.min = glm::vec3(-1000.0f, -1000.0f, -1000.0f);
    box.max = glm::vec3(1000.0f, 1000.0f, 1000.0f);

    EXPECT_TRUE(frustum_->is_visible(box));
}

class ChunkCullerTest : public ::testing::Test {
protected:
    void SetUp() override { culler_ = std::make_unique<ChunkCuller>(); }

    std::unique_ptr<ChunkCuller> culler_;
};

TEST_F(ChunkCullerTest, UpdateFromViewProjection) {
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 1000.0f);

    culler_->update(proj * view);

    // A chunk in front should be visible
    bool visible = culler_->is_chunk_visible(glm::vec3(0.0f, 0.0f, -50.0f), 32, 256, 32);
    EXPECT_TRUE(visible);
}

TEST_F(ChunkCullerTest, CountsVisibleAndCulled) {
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 1000.0f);

    culler_->update(proj * view);
    culler_->reset_stats();

    // Check a visible chunk
    culler_->is_chunk_visible(glm::vec3(0.0f, 0.0f, -50.0f), 32, 256, 32);

    // Check a culled chunk (behind camera)
    culler_->is_chunk_visible(glm::vec3(0.0f, 0.0f, 100.0f), 32, 256, 32);

    EXPECT_EQ(culler_->get_visible_count(), 1u);
    EXPECT_EQ(culler_->get_culled_count(), 1u);
}

TEST_F(ChunkCullerTest, ResetStatsClearsCounters) {
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 1000.0f);

    culler_->update(proj * view);

    culler_->is_chunk_visible(glm::vec3(0.0f, 0.0f, -50.0f), 32, 256, 32);
    culler_->is_chunk_visible(glm::vec3(0.0f, 0.0f, -50.0f), 32, 256, 32);

    culler_->reset_stats();

    EXPECT_EQ(culler_->get_visible_count(), 0u);
    EXPECT_EQ(culler_->get_culled_count(), 0u);
}

}  // namespace realcraft::rendering::test
