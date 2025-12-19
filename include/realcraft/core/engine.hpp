// RealCraft Engine Core
// engine.hpp - Main engine class that orchestrates all subsystems

#pragma once

#include <realcraft/core/config.hpp>
#include <realcraft/core/game_loop.hpp>
#include <realcraft/core/logger.hpp>
#include <realcraft/platform/window.hpp>
#include <realcraft/graphics/device.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace realcraft::core {

// Engine configuration
struct EngineConfig {
    // Application info
    std::string app_name = "RealCraft";
    std::string version = "0.1.0";

    // Window configuration
    platform::Window::Config window;

    // Graphics
    bool enable_graphics_validation = false;

    // Logging configuration
    LoggerConfig logging;

    // Config file path (empty = use default: <user_config_dir>/config.json)
    std::filesystem::path config_path;

    // Game loop settings
    GameLoopConfig game_loop;
};

// Main engine class - orchestrates all subsystems
class Engine {
public:
    Engine();
    ~Engine();

    // Non-copyable, non-movable
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    // Lifecycle
    bool initialize(const EngineConfig& config = {});
    void shutdown();
    [[nodiscard]] bool is_initialized() const;

    // Main loop (blocks until exit)
    void run();
    void request_exit();

    // Subsystem access
    [[nodiscard]] platform::Window* get_window();
    [[nodiscard]] const platform::Window* get_window() const;
    [[nodiscard]] graphics::GraphicsDevice* get_graphics_device();
    [[nodiscard]] const graphics::GraphicsDevice* get_graphics_device() const;
    [[nodiscard]] Config* get_config();
    [[nodiscard]] const Config* get_config() const;
    [[nodiscard]] GameLoop* get_game_loop();
    [[nodiscard]] const GameLoop* get_game_loop() const;

    // Input access (convenience)
    [[nodiscard]] platform::Input* get_input();
    [[nodiscard]] const platform::Input* get_input() const;

    // Game callbacks (set before calling run())
    using InitCallback = std::function<bool()>;
    using ShutdownCallback = std::function<void()>;
    using FixedUpdateCallback = std::function<void(double fixed_delta)>;
    using UpdateCallback = std::function<void(double delta_time)>;
    using RenderCallback = std::function<void(double interpolation)>;

    void set_init_callback(InitCallback callback);
    void set_shutdown_callback(ShutdownCallback callback);
    void set_fixed_update_callback(FixedUpdateCallback callback);
    void set_update_callback(UpdateCallback callback);
    void set_render_callback(RenderCallback callback);

    // Singleton access (for convenience, optional usage)
    [[nodiscard]] static Engine* get_instance();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    static Engine* s_instance;
};

}  // namespace realcraft::core
