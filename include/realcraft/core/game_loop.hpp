// RealCraft Engine Core
// game_loop.hpp - Fixed and variable timestep game loop

#pragma once

#include <cstdint>
#include <functional>
#include <memory>

namespace realcraft::core {

// Callbacks for game loop phases
using FixedUpdateCallback = std::function<void(double fixed_delta)>;
using UpdateCallback = std::function<void(double delta_time)>;
using RenderCallback = std::function<void(double interpolation)>;

// Game loop configuration
struct GameLoopConfig {
    double fixed_timestep = 1.0 / 60.0;   // 60 Hz physics
    double max_frame_time = 0.25;          // Prevent spiral of death
    double target_fps = 0.0;               // 0 = unlimited (vsync controls)
    bool vsync = true;
};

// Fixed timestep game loop with interpolated rendering
class GameLoop {
public:
    GameLoop();
    ~GameLoop();

    // Non-copyable, non-movable
    GameLoop(const GameLoop&) = delete;
    GameLoop& operator=(const GameLoop&) = delete;
    GameLoop(GameLoop&&) = delete;
    GameLoop& operator=(GameLoop&&) = delete;

    // Configuration
    void configure(const GameLoopConfig& config);
    [[nodiscard]] const GameLoopConfig& get_config() const;

    // Set callbacks
    void set_fixed_update(FixedUpdateCallback callback);
    void set_update(UpdateCallback callback);
    void set_render(RenderCallback callback);

    // Run single frame - call this in your main loop
    void run_frame();

    // Exit control
    void request_exit();
    [[nodiscard]] bool should_exit() const;

    // Timing access
    [[nodiscard]] double get_fixed_timestep() const;
    [[nodiscard]] double get_delta_time() const;
    [[nodiscard]] double get_unscaled_delta_time() const;
    [[nodiscard]] double get_total_time() const;
    [[nodiscard]] double get_interpolation() const;  // Alpha for render interpolation
    [[nodiscard]] uint64_t get_frame_count() const;
    [[nodiscard]] double get_fps() const;
    [[nodiscard]] double get_average_fps() const;

    // Time scale (affects delta_time, NOT fixed updates)
    void set_time_scale(double scale);
    [[nodiscard]] double get_time_scale() const;

    // Pause state (stops fixed updates, render continues)
    void set_paused(bool paused);
    [[nodiscard]] bool is_paused() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::core
