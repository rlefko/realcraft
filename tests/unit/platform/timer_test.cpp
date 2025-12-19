// RealCraft Platform Tests
// timer_test.cpp - Timer unit tests

#include <gtest/gtest.h>
#include <realcraft/platform/timer.hpp>

#include <chrono>
#include <thread>

using namespace realcraft::platform;

class TimerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TimerTest, StartsNearZero) {
    Timer timer;
    EXPECT_LT(timer.elapsed_milliseconds(), 5.0);
}

TEST_F(TimerTest, MeasuresElapsedTime) {
    Timer timer;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    double elapsed = timer.elapsed_milliseconds();
    EXPECT_GE(elapsed, 45.0);  // Allow some tolerance
    EXPECT_LT(elapsed, 150.0);
}

TEST_F(TimerTest, ResetWorks) {
    Timer timer;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    timer.reset();

    EXPECT_LT(timer.elapsed_milliseconds(), 5.0);
}

TEST_F(TimerTest, PauseAndResume) {
    Timer timer;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    timer.pause();
    EXPECT_TRUE(timer.is_paused());

    double paused_time = timer.elapsed_milliseconds();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Time should not advance while paused
    EXPECT_NEAR(timer.elapsed_milliseconds(), paused_time, 5.0);

    timer.resume();
    EXPECT_FALSE(timer.is_paused());

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // Time should have advanced after resume
    EXPECT_GT(timer.elapsed_milliseconds(), paused_time + 15.0);
}

TEST_F(TimerTest, ElapsedConversions) {
    Timer timer;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    double seconds = timer.elapsed_seconds();
    double milliseconds = timer.elapsed_milliseconds();
    double microseconds = timer.elapsed_microseconds();

    EXPECT_NEAR(milliseconds, seconds * 1000.0, 5.0);
    EXPECT_NEAR(microseconds, seconds * 1000000.0, 5000.0);
}

class FrameTimerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(FrameTimerTest, DeltaTimeCalculation) {
    FrameTimer ft;

    ft.begin_frame();
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    ft.end_frame();

    double delta = ft.get_delta_time();
    // CI runners can have significant timing variability, so use a wide tolerance
    EXPECT_GT(delta, 0.010);   // At least 10ms (sleep was 16ms)
    EXPECT_LT(delta, 0.200);   // Less than 200ms (reasonable upper bound)
}

TEST_F(FrameTimerTest, DeltaTimeConversions) {
    FrameTimer ft;

    ft.begin_frame();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ft.end_frame();

    double seconds = ft.get_delta_time();
    double milliseconds = ft.get_delta_time_ms();
    float seconds_f = ft.get_delta_time_f();

    EXPECT_NEAR(milliseconds, seconds * 1000.0, 1.0);
    EXPECT_NEAR(static_cast<double>(seconds_f), seconds, 0.001);
}

TEST_F(FrameTimerTest, FPSCalculation) {
    FrameTimer ft;

    // Simulate several frames at ~60 FPS
    for (int i = 0; i < 5; ++i) {
        ft.begin_frame();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        ft.end_frame();
    }

    double fps = ft.get_fps();
    // CI runners can be slow - just verify FPS is calculated and positive
    EXPECT_GT(fps, 5.0);    // At least 5 FPS (very lenient for slow CI)
    EXPECT_LT(fps, 200.0);  // Reasonable upper bound
}

TEST_F(FrameTimerTest, TimeScaling) {
    FrameTimer ft;
    ft.set_time_scale(2.0);
    EXPECT_DOUBLE_EQ(ft.get_time_scale(), 2.0);

    ft.begin_frame();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ft.end_frame();

    double scaled = ft.get_delta_time();
    double unscaled = ft.get_unscaled_delta_time();

    EXPECT_NEAR(scaled, unscaled * 2.0, 0.005);
}

TEST_F(FrameTimerTest, FrameCount) {
    FrameTimer ft;
    EXPECT_EQ(ft.get_frame_count(), 0u);

    for (int i = 0; i < 5; ++i) {
        ft.begin_frame();
        ft.end_frame();
    }

    EXPECT_EQ(ft.get_frame_count(), 5u);
}

TEST_F(FrameTimerTest, TotalTime) {
    FrameTimer ft;

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    double total = ft.get_total_time();
    EXPECT_GE(total, 0.045);
    EXPECT_LT(total, 0.150);
}

TEST_F(FrameTimerTest, FrameLimitingSettings) {
    FrameTimer ft;

    EXPECT_FALSE(ft.is_frame_limiting_enabled());
    ft.set_frame_limiting_enabled(true);
    EXPECT_TRUE(ft.is_frame_limiting_enabled());

    ft.set_target_fps(30.0);
    EXPECT_DOUBLE_EQ(ft.get_target_fps(), 30.0);
}
