// RealCraft Platform Abstraction Layer
// timer.cpp - Timer implementation

#include <realcraft/platform/timer.hpp>

#include <spdlog/spdlog.h>
#include <algorithm>
#include <numeric>
#include <thread>

namespace realcraft::platform {

// Timer implementation
Timer::Timer() : start_time_(Clock::now()) {}

void Timer::reset() {
    start_time_ = Clock::now();
    paused_duration_ = Duration{0};
    is_paused_ = false;
}

void Timer::pause() {
    if (!is_paused_) {
        is_paused_ = true;
        pause_start_ = Clock::now();
    }
}

void Timer::resume() {
    if (is_paused_) {
        is_paused_ = false;
        paused_duration_ += std::chrono::duration_cast<Duration>(Clock::now() - pause_start_);
    }
}

bool Timer::is_paused() const {
    return is_paused_;
}

Timer::Duration Timer::elapsed() const {
    auto now = Clock::now();
    auto total = std::chrono::duration_cast<Duration>(now - start_time_);

    if (is_paused_) {
        total -= paused_duration_;
        total -= std::chrono::duration_cast<Duration>(now - pause_start_);
    } else {
        total -= paused_duration_;
    }

    return total;
}

double Timer::elapsed_seconds() const {
    return std::chrono::duration<double>(elapsed()).count();
}

double Timer::elapsed_milliseconds() const {
    return std::chrono::duration<double, std::milli>(elapsed()).count();
}

double Timer::elapsed_microseconds() const {
    return std::chrono::duration<double, std::micro>(elapsed()).count();
}

// FrameTimer implementation
FrameTimer::FrameTimer() : frame_start_(Timer::Clock::now()), frame_end_(frame_start_) {
    frame_times_.fill(0.0);
}

void FrameTimer::begin_frame() {
    frame_start_ = Timer::Clock::now();
}

void FrameTimer::end_frame() {
    frame_end_ = Timer::Clock::now();

    auto duration = std::chrono::duration<double>(frame_end_ - frame_start_);
    delta_time_ = duration.count();

    // Store frame time in history for averaging
    frame_times_[frame_time_index_] = delta_time_;
    frame_time_index_ = (frame_time_index_ + 1) % HISTORY_SIZE;

    ++frame_count_;
}

double FrameTimer::get_delta_time() const {
    return delta_time_ * time_scale_;
}

double FrameTimer::get_delta_time_ms() const {
    return get_delta_time() * 1000.0;
}

float FrameTimer::get_delta_time_f() const {
    return static_cast<float>(get_delta_time());
}

double FrameTimer::get_unscaled_delta_time() const {
    return delta_time_;
}

void FrameTimer::set_time_scale(double scale) {
    time_scale_ = std::max(0.0, scale);
}

double FrameTimer::get_time_scale() const {
    return time_scale_;
}

double FrameTimer::get_fps() const {
    if (delta_time_ > 0.0) {
        return 1.0 / delta_time_;
    }
    return 0.0;
}

double FrameTimer::get_average_fps() const {
    double avg_frame_time = get_average_frame_time();
    if (avg_frame_time > 0.0) {
        return 1000.0 / avg_frame_time;
    }
    return 0.0;
}

double FrameTimer::get_average_frame_time() const {
    size_t count = std::min(frame_count_, static_cast<uint64_t>(HISTORY_SIZE));
    if (count == 0) {
        return 0.0;
    }

    double sum = 0.0;
    for (size_t i = 0; i < count; ++i) {
        sum += frame_times_[i];
    }

    return (sum / static_cast<double>(count)) * 1000.0;  // Convert to ms
}

void FrameTimer::set_target_fps(double fps) {
    if (fps > 0.0) {
        target_frame_time_ = 1.0 / fps;
    }
}

double FrameTimer::get_target_fps() const {
    if (target_frame_time_ > 0.0) {
        return 1.0 / target_frame_time_;
    }
    return 0.0;
}

void FrameTimer::set_frame_limiting_enabled(bool enabled) {
    frame_limiting_enabled_ = enabled;
}

bool FrameTimer::is_frame_limiting_enabled() const {
    return frame_limiting_enabled_;
}

void FrameTimer::wait_for_target_frame_time() {
    if (!frame_limiting_enabled_) {
        return;
    }

    auto now = Timer::Clock::now();
    auto elapsed = std::chrono::duration<double>(now - frame_start_).count();
    double remaining = target_frame_time_ - elapsed;

    if (remaining > 0.0) {
        // Sleep for most of the remaining time, then spin-wait for precision
        if (remaining > 0.002) {  // 2ms threshold for sleep
            std::this_thread::sleep_for(
                std::chrono::duration<double>(remaining - 0.001));  // Leave 1ms for spin
        }

        // Spin-wait for remaining time
        while (std::chrono::duration<double>(Timer::Clock::now() - frame_start_).count() <
               target_frame_time_) {
            // Busy wait
        }
    }
}

double FrameTimer::get_total_time() const {
    return total_timer_.elapsed_seconds();
}

uint64_t FrameTimer::get_frame_count() const {
    return frame_count_;
}

// ScopedTimer implementation
ScopedTimer::ScopedTimer(const char* name) : name_(name) {}

ScopedTimer::~ScopedTimer() {
    double elapsed = timer_.elapsed_milliseconds();
    spdlog::trace("[Timer] {}: {:.3f}ms", name_, elapsed);
}

}  // namespace realcraft::platform
