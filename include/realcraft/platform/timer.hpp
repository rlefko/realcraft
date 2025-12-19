// RealCraft Platform Abstraction Layer
// timer.hpp - High-resolution timer utilities

#pragma once

#include <array>
#include <chrono>
#include <cstdint>

namespace realcraft::platform {

// Simple stopwatch timer using steady_clock
class Timer {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::nanoseconds;

    Timer();

    // Reset the timer to zero
    void reset();

    // Pause timing (elapsed time stops accumulating)
    void pause();

    // Resume timing after a pause
    void resume();

    // Check if timer is paused
    [[nodiscard]] bool is_paused() const;

    // Get elapsed time since timer creation/reset
    [[nodiscard]] Duration elapsed() const;
    [[nodiscard]] double elapsed_seconds() const;
    [[nodiscard]] double elapsed_milliseconds() const;
    [[nodiscard]] double elapsed_microseconds() const;

private:
    TimePoint start_time_;
    Duration paused_duration_{0};
    TimePoint pause_start_;
    bool is_paused_ = false;
};

// Frame timing for game loop with delta time calculation
class FrameTimer {
public:
    FrameTimer();

    // Call at the start of each frame
    void begin_frame();

    // Call at the end of each frame
    void end_frame();

    // Delta time (time since last frame)
    [[nodiscard]] double get_delta_time() const;       // In seconds
    [[nodiscard]] double get_delta_time_ms() const;    // In milliseconds
    [[nodiscard]] float get_delta_time_f() const;      // In seconds (float)

    // Unscaled delta time (ignores time scale)
    [[nodiscard]] double get_unscaled_delta_time() const;

    // Time scaling (for slow-motion effects, pause, etc.)
    void set_time_scale(double scale);
    [[nodiscard]] double get_time_scale() const;

    // Frame rate
    [[nodiscard]] double get_fps() const;
    [[nodiscard]] double get_average_fps() const;         // Rolling average
    [[nodiscard]] double get_average_frame_time() const;  // In milliseconds

    // Frame limiting
    void set_target_fps(double fps);
    [[nodiscard]] double get_target_fps() const;
    void set_frame_limiting_enabled(bool enabled);
    [[nodiscard]] bool is_frame_limiting_enabled() const;

    // Call after end_frame() to wait for target frame time
    void wait_for_target_frame_time();

    // Total time since timer creation
    [[nodiscard]] double get_total_time() const;

    // Frame count
    [[nodiscard]] uint64_t get_frame_count() const;

private:
    static constexpr size_t HISTORY_SIZE = 120;

    Timer total_timer_;
    Timer::TimePoint frame_start_;
    Timer::TimePoint frame_end_;

    double delta_time_ = 0.0;
    double time_scale_ = 1.0;
    double target_frame_time_ = 1.0 / 60.0;
    bool frame_limiting_enabled_ = false;

    std::array<double, HISTORY_SIZE> frame_times_{};
    size_t frame_time_index_ = 0;
    uint64_t frame_count_ = 0;
};

// RAII scoped timer for profiling - logs elapsed time on destruction
class ScopedTimer {
public:
    explicit ScopedTimer(const char* name);
    ~ScopedTimer();

    // Non-copyable and non-movable
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
    ScopedTimer(ScopedTimer&&) = delete;
    ScopedTimer& operator=(ScopedTimer&&) = delete;

private:
    const char* name_;
    Timer timer_;
};

// Macro for easy profiling
#define REALCRAFT_SCOPED_TIMER(name) \
    ::realcraft::platform::ScopedTimer scoped_timer_##__LINE__(name)

}  // namespace realcraft::platform
