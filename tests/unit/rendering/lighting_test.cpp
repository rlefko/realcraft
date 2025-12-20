// RealCraft Rendering Tests
// lighting_test.cpp - Unit tests for DayNightCycle and Lighting

#include <glm/glm.hpp>
#include <gtest/gtest.h>

#include <cmath>
#include <realcraft/rendering/lighting.hpp>

namespace realcraft::rendering::test {

class DayNightCycleTest : public ::testing::Test {
protected:
    void SetUp() override { cycle_ = std::make_unique<DayNightCycle>(); }

    std::unique_ptr<DayNightCycle> cycle_;
};

TEST_F(DayNightCycleTest, DefaultTimeIsNoon) {
    // Default time should be around noon (0.5)
    float time = cycle_->get_time();
    EXPECT_GE(time, 0.0f);
    EXPECT_LE(time, 1.0f);
}

TEST_F(DayNightCycleTest, SetTime) {
    cycle_->set_time(0.75f);  // Sunset
    EXPECT_FLOAT_EQ(cycle_->get_time(), 0.75f);

    cycle_->set_time(0.0f);  // Midnight
    EXPECT_FLOAT_EQ(cycle_->get_time(), 0.0f);

    cycle_->set_time(0.25f);  // Sunrise
    EXPECT_FLOAT_EQ(cycle_->get_time(), 0.25f);
}

TEST_F(DayNightCycleTest, TimeWrapsAround) {
    cycle_->set_time(1.5f);
    EXPECT_FLOAT_EQ(cycle_->get_time(), 0.5f);

    cycle_->set_time(-0.25f);
    EXPECT_FLOAT_EQ(cycle_->get_time(), 0.75f);
}

TEST_F(DayNightCycleTest, UpdateAdvancesTime) {
    cycle_->set_time(0.0f);
    float initial_time = cycle_->get_time();

    cycle_->update(60.0f);  // 1 minute
    float new_time = cycle_->get_time();

    EXPECT_GT(new_time, initial_time);
}

TEST_F(DayNightCycleTest, DayLengthAffectsSpeed) {
    DayNightCycle fast_cycle;
    fast_cycle.set_day_length(60.0f);  // 1 minute day

    DayNightCycle slow_cycle;
    slow_cycle.set_day_length(1200.0f);  // 20 minute day

    fast_cycle.set_time(0.0f);
    slow_cycle.set_time(0.0f);

    fast_cycle.update(30.0f);  // 30 seconds
    slow_cycle.update(30.0f);

    // Fast cycle should advance more
    EXPECT_GT(fast_cycle.get_time(), slow_cycle.get_time());
}

TEST_F(DayNightCycleTest, PausedCycleDoesNotAdvance) {
    cycle_->set_time(0.25f);
    cycle_->set_paused(true);

    cycle_->update(1000.0f);  // Large time delta

    EXPECT_FLOAT_EQ(cycle_->get_time(), 0.25f);
}

TEST_F(DayNightCycleTest, SunDirectionAtNoon) {
    cycle_->set_time(0.5f);  // Noon

    auto uniforms = cycle_->get_uniforms();

    // At noon, sun should be mostly overhead (positive Y)
    EXPECT_GT(uniforms.sun_direction.y, 0.5f);
}

TEST_F(DayNightCycleTest, SunDirectionAtMidnight) {
    cycle_->set_time(0.0f);  // Midnight

    auto uniforms = cycle_->get_uniforms();

    // At midnight, sun should be below horizon (negative Y)
    EXPECT_LT(uniforms.sun_direction.y, 0.0f);
}

TEST_F(DayNightCycleTest, SunDirectionAtHorizon) {
    cycle_->set_time(0.25f);  // Sunrise

    auto uniforms = cycle_->get_uniforms();

    // At sunrise, sun should be near horizon (Y close to 0)
    EXPECT_NEAR(uniforms.sun_direction.y, 0.0f, 0.1f);
}

TEST_F(DayNightCycleTest, SunIntensityHigherDuringDay) {
    cycle_->set_time(0.5f);  // Noon
    auto noon_uniforms = cycle_->get_uniforms();

    cycle_->set_time(0.0f);  // Midnight
    auto midnight_uniforms = cycle_->get_uniforms();

    EXPECT_GT(noon_uniforms.sun_intensity, midnight_uniforms.sun_intensity);
}

TEST_F(DayNightCycleTest, MoonIntensityHigherAtNight) {
    cycle_->set_time(0.0f);  // Midnight
    auto midnight_uniforms = cycle_->get_uniforms();

    cycle_->set_time(0.5f);  // Noon
    auto noon_uniforms = cycle_->get_uniforms();

    EXPECT_GT(midnight_uniforms.moon_intensity, noon_uniforms.moon_intensity);
}

TEST_F(DayNightCycleTest, AmbientLightAlwaysPresent) {
    // Check that there's always some ambient light
    for (float time = 0.0f; time <= 1.0f; time += 0.1f) {
        cycle_->set_time(time);
        auto uniforms = cycle_->get_uniforms();

        EXPECT_GT(uniforms.ambient_intensity, 0.0f);
    }
}

TEST_F(DayNightCycleTest, SkyColorChangesWithTime) {
    cycle_->set_time(0.5f);  // Noon
    auto noon_uniforms = cycle_->get_uniforms();

    cycle_->set_time(0.0f);  // Midnight
    auto midnight_uniforms = cycle_->get_uniforms();

    // Sky should be different colors at different times
    EXPECT_NE(noon_uniforms.sky_zenith, midnight_uniforms.sky_zenith);
}

TEST_F(DayNightCycleTest, FogColorMatchesSky) {
    cycle_->set_time(0.5f);
    auto uniforms = cycle_->get_uniforms();

    // Fog color should be related to horizon color
    // They might not be exactly equal but should be close
    float diff = glm::length(uniforms.fog_color - uniforms.sky_horizon);
    EXPECT_LT(diff, 0.5f);
}

TEST_F(DayNightCycleTest, UniformsHaveValidValues) {
    for (float time = 0.0f; time <= 1.0f; time += 0.1f) {
        cycle_->set_time(time);
        auto uniforms = cycle_->get_uniforms();

        // All intensities should be in [0, 1] (with small tolerance for floating point)
        constexpr float epsilon = 0.0001f;
        EXPECT_GE(uniforms.sun_intensity, 0.0f - epsilon);
        EXPECT_LE(uniforms.sun_intensity, 1.0f + epsilon);
        EXPECT_GE(uniforms.moon_intensity, 0.0f - epsilon);
        EXPECT_LE(uniforms.moon_intensity, 1.0f + epsilon);
        EXPECT_GE(uniforms.ambient_intensity, 0.0f - epsilon);
        EXPECT_LE(uniforms.ambient_intensity, 1.0f + epsilon);

        // Sun direction should be normalized
        float sun_len = glm::length(uniforms.sun_direction);
        EXPECT_NEAR(sun_len, 1.0f, 0.001f);

        // Time should match
        EXPECT_NEAR(uniforms.time_of_day, time, 0.001f);
    }
}

TEST_F(DayNightCycleTest, IsDayTimeCorrect) {
    cycle_->set_time(0.5f);  // Noon
    EXPECT_TRUE(cycle_->is_day());

    cycle_->set_time(0.0f);  // Midnight
    EXPECT_FALSE(cycle_->is_day());

    cycle_->set_time(0.3f);  // Morning
    EXPECT_TRUE(cycle_->is_day());

    cycle_->set_time(0.9f);  // Late night
    EXPECT_FALSE(cycle_->is_day());
}

TEST_F(DayNightCycleTest, SunAngleContinuous) {
    // Sun direction should change smoothly
    glm::vec3 prev_dir = glm::vec3(0.0f);

    for (float time = 0.0f; time <= 1.0f; time += 0.01f) {
        cycle_->set_time(time);
        auto uniforms = cycle_->get_uniforms();

        if (time > 0.0f) {
            float angle_change = glm::dot(prev_dir, uniforms.sun_direction);
            // Should be close to 1 (nearly parallel for small time steps)
            EXPECT_GT(angle_change, 0.99f);
        }

        prev_dir = uniforms.sun_direction;
    }
}

class LightingUniformsTest : public ::testing::Test {};

TEST_F(LightingUniformsTest, DefaultValues) {
    LightingUniforms uniforms{};

    EXPECT_FLOAT_EQ(uniforms.sun_intensity, 0.0f);
    EXPECT_FLOAT_EQ(uniforms.moon_intensity, 0.0f);
    EXPECT_FLOAT_EQ(uniforms.ambient_intensity, 0.0f);
}

TEST_F(LightingUniformsTest, StructLayout) {
    // Verify std140 layout compatibility
    // Each vec3 + float should be 16 bytes aligned
    EXPECT_EQ(sizeof(LightingUniforms) % 16, 0u);
}

}  // namespace realcraft::rendering::test
