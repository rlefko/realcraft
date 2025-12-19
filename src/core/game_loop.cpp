// RealCraft Engine Core
// game_loop.cpp - Fixed and variable timestep game loop implementation

#include <algorithm>
#include <realcraft/core/game_loop.hpp>
#include <realcraft/core/logger.hpp>
#include <realcraft/core/memory.hpp>
#include <realcraft/platform/timer.hpp>

namespace realcraft::core {

struct GameLoop::Impl {
    GameLoopConfig config;

    FixedUpdateCallback fixed_update_callback;
    UpdateCallback update_callback;
    RenderCallback render_callback;

    platform::FrameTimer frame_timer;
    double accumulator = 0.0;
    double interpolation = 0.0;
    double time_scale = 1.0;

    bool paused = false;
    bool exit_requested = false;
};

GameLoop::GameLoop() : impl_(std::make_unique<Impl>()) {
    // Configure frame timer with default target FPS
    impl_->frame_timer.set_frame_limiting_enabled(false);
}

GameLoop::~GameLoop() = default;

void GameLoop::configure(const GameLoopConfig& config) {
    impl_->config = config;

    if (config.target_fps > 0.0) {
        impl_->frame_timer.set_target_fps(config.target_fps);
        impl_->frame_timer.set_frame_limiting_enabled(true);
    } else {
        impl_->frame_timer.set_frame_limiting_enabled(false);
    }

    REALCRAFT_LOG_INFO(log_category::ENGINE, "Game loop configured: fixed_dt={:.4f}s, max_frame={:.4f}s, target_fps={}",
                       config.fixed_timestep, config.max_frame_time,
                       config.target_fps > 0 ? std::to_string(static_cast<int>(config.target_fps)) : "unlimited");
}

const GameLoopConfig& GameLoop::get_config() const {
    return impl_->config;
}

void GameLoop::set_fixed_update(FixedUpdateCallback callback) {
    impl_->fixed_update_callback = std::move(callback);
}

void GameLoop::set_update(UpdateCallback callback) {
    impl_->update_callback = std::move(callback);
}

void GameLoop::set_render(RenderCallback callback) {
    impl_->render_callback = std::move(callback);
}

void GameLoop::run_frame() {
    // Reset per-frame allocator
    reset_frame_allocator();

    // Begin frame timing
    impl_->frame_timer.begin_frame();

    // Calculate frame time, clamping to prevent spiral of death
    double frame_time = impl_->frame_timer.get_unscaled_delta_time();
    frame_time = std::min(frame_time, impl_->config.max_frame_time);

    // Variable update (runs every frame)
    double scaled_delta = frame_time * impl_->time_scale;
    if (impl_->update_callback) {
        impl_->update_callback(scaled_delta);
    }

    // Fixed timestep updates (only if not paused)
    if (!impl_->paused) {
        impl_->accumulator += frame_time;

        // Run fixed updates until we've caught up
        while (impl_->accumulator >= impl_->config.fixed_timestep) {
            if (impl_->fixed_update_callback) {
                impl_->fixed_update_callback(impl_->config.fixed_timestep);
            }
            impl_->accumulator -= impl_->config.fixed_timestep;
        }
    }

    // Calculate interpolation for smooth rendering
    impl_->interpolation = impl_->accumulator / impl_->config.fixed_timestep;

    // Render
    if (impl_->render_callback) {
        impl_->render_callback(impl_->interpolation);
    }

    // End frame timing
    impl_->frame_timer.end_frame();

    // Wait for target frame time if frame limiting is enabled
    if (impl_->frame_timer.is_frame_limiting_enabled()) {
        impl_->frame_timer.wait_for_target_frame_time();
    }
}

void GameLoop::request_exit() {
    impl_->exit_requested = true;
}

bool GameLoop::should_exit() const {
    return impl_->exit_requested;
}

double GameLoop::get_fixed_timestep() const {
    return impl_->config.fixed_timestep;
}

double GameLoop::get_delta_time() const {
    return impl_->frame_timer.get_delta_time() * impl_->time_scale;
}

double GameLoop::get_unscaled_delta_time() const {
    return impl_->frame_timer.get_unscaled_delta_time();
}

double GameLoop::get_total_time() const {
    return impl_->frame_timer.get_total_time();
}

double GameLoop::get_interpolation() const {
    return impl_->interpolation;
}

uint64_t GameLoop::get_frame_count() const {
    return impl_->frame_timer.get_frame_count();
}

double GameLoop::get_fps() const {
    return impl_->frame_timer.get_fps();
}

double GameLoop::get_average_fps() const {
    return impl_->frame_timer.get_average_fps();
}

void GameLoop::set_time_scale(double scale) {
    impl_->time_scale = scale;
    impl_->frame_timer.set_time_scale(scale);
}

double GameLoop::get_time_scale() const {
    return impl_->time_scale;
}

void GameLoop::set_paused(bool paused) {
    impl_->paused = paused;

    if (paused) {
        REALCRAFT_LOG_DEBUG(log_category::ENGINE, "Game loop paused");
    } else {
        REALCRAFT_LOG_DEBUG(log_category::ENGINE, "Game loop resumed");
    }
}

bool GameLoop::is_paused() const {
    return impl_->paused;
}

}  // namespace realcraft::core
