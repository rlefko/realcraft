// RealCraft Rendering Tests
// camera_test.cpp - Unit tests for Camera class

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include <cmath>
#include <realcraft/rendering/camera.hpp>

namespace realcraft::rendering::test {

class CameraTest : public ::testing::Test {
protected:
    void SetUp() override { camera_ = std::make_unique<Camera>(); }

    std::unique_ptr<Camera> camera_;
};

TEST_F(CameraTest, DefaultConstruction) {
    auto pos = camera_->get_position();
    EXPECT_DOUBLE_EQ(pos.x, 0.0);
    EXPECT_DOUBLE_EQ(pos.y, 64.0);  // Default Y position
    EXPECT_DOUBLE_EQ(pos.z, 0.0);
}

TEST_F(CameraTest, SetPosition) {
    glm::dvec3 new_pos(100.5, 64.0, -200.25);
    camera_->set_position(new_pos);

    auto pos = camera_->get_position();
    EXPECT_DOUBLE_EQ(pos.x, 100.5);
    EXPECT_DOUBLE_EQ(pos.y, 64.0);
    EXPECT_DOUBLE_EQ(pos.z, -200.25);
}

TEST_F(CameraTest, SetRotation) {
    camera_->set_rotation(30.0f, 45.0f);

    EXPECT_FLOAT_EQ(camera_->get_pitch(), 30.0f);
    EXPECT_FLOAT_EQ(camera_->get_yaw(), 45.0f);
}

TEST_F(CameraTest, Rotate) {
    // Set sensitivity to 1.0 to get direct rotation values
    CameraConfig config;
    config.mouse_sensitivity = 1.0f;
    camera_->set_config(config);

    camera_->set_rotation(0.0f, 0.0f);
    camera_->rotate(10.0f, 20.0f);

    EXPECT_FLOAT_EQ(camera_->get_pitch(), 10.0f);
    EXPECT_FLOAT_EQ(camera_->get_yaw(), 20.0f);
}

TEST_F(CameraTest, ForwardVector) {
    // Looking down -Z (default yaw = -90)
    camera_->set_rotation(0.0f, -90.0f);
    auto forward = camera_->get_forward();

    EXPECT_NEAR(forward.x, 0.0f, 0.001f);
    EXPECT_NEAR(forward.y, 0.0f, 0.001f);
    EXPECT_NEAR(forward.z, -1.0f, 0.001f);
}

TEST_F(CameraTest, RightVector) {
    camera_->set_rotation(0.0f, -90.0f);
    auto right = camera_->get_right();

    EXPECT_NEAR(right.x, 1.0f, 0.001f);
    EXPECT_NEAR(right.y, 0.0f, 0.001f);
    EXPECT_NEAR(right.z, 0.0f, 0.001f);
}

TEST_F(CameraTest, UpVector) {
    camera_->set_rotation(0.0f, 0.0f);
    auto up = camera_->get_up();

    EXPECT_NEAR(up.x, 0.0f, 0.001f);
    EXPECT_NEAR(up.y, 1.0f, 0.001f);
    EXPECT_NEAR(up.z, 0.0f, 0.001f);
}

TEST_F(CameraTest, ViewMatrix) {
    camera_->set_position({0.0, 0.0, 0.0});
    camera_->set_rotation(0.0f, -90.0f);

    auto view = camera_->get_view_matrix();

    // View matrix should be valid (non-zero determinant)
    // Just check it's not all zeros
    bool has_nonzero = false;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (std::abs(view[i][j]) > 0.001f) {
                has_nonzero = true;
                break;
            }
        }
    }
    EXPECT_TRUE(has_nonzero);
}

TEST_F(CameraTest, ProjectionMatrix) {
    auto proj = camera_->get_projection_matrix(16.0f / 9.0f);

    // Check perspective projection properties
    // Element [2][3] should be -1 for perspective projection
    EXPECT_FLOAT_EQ(proj[2][3], -1.0f);
}

TEST_F(CameraTest, Interpolation) {
    camera_->set_position({0.0, 0.0, 0.0});
    camera_->store_previous_position();

    camera_->set_position({10.0, 0.0, 0.0});

    // At interpolation = 0.5, should be halfway
    auto interp_pos = camera_->get_interpolated_position(0.5);
    EXPECT_NEAR(interp_pos.x, 5.0, 0.001);
}

TEST_F(CameraTest, ProcessMovement) {
    camera_->set_position({0.0, 0.0, 0.0});
    camera_->set_rotation(0.0f, -90.0f);  // Looking down -Z

    glm::vec3 move_input(0.0f, 0.0f, 1.0f);  // Move forward
    camera_->process_movement(move_input, 1.0f);

    auto pos = camera_->get_position();
    EXPECT_LT(pos.z, 0.0);  // Should have moved in -Z direction
}

TEST_F(CameraTest, ConfigFovAffectsProjection) {
    CameraConfig config1;
    config1.fov_degrees = 60.0f;
    camera_->set_config(config1);
    auto proj60 = camera_->get_projection_matrix(1.0f);

    CameraConfig config2;
    config2.fov_degrees = 90.0f;
    camera_->set_config(config2);
    auto proj90 = camera_->get_projection_matrix(1.0f);

    // Different FOV should produce different projection matrices
    EXPECT_NE(proj60[0][0], proj90[0][0]);
}

TEST_F(CameraTest, RenderOffset) {
    camera_->set_position({1000.0, 64.0, 2000.0});
    camera_->set_render_offset({1000.0, 0.0, 2000.0});

    auto render_pos = camera_->get_render_position();

    // Render position should be relative to offset
    EXPECT_NEAR(render_pos.x, 0.0f, 0.001f);
    EXPECT_NEAR(render_pos.z, 0.0f, 0.001f);
}

}  // namespace realcraft::rendering::test
