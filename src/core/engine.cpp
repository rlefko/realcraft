// RealCraft Engine Core
// engine.cpp - Main engine class implementation

#include <realcraft/core/engine.hpp>
#include <realcraft/core/memory.hpp>
#include <realcraft/platform/platform.hpp>
#include <realcraft/platform/input_action.hpp>
#include <realcraft/platform/file_io.hpp>

#include <format>

namespace realcraft::core {

Engine* Engine::s_instance = nullptr;

struct Engine::Impl {
    EngineConfig config;
    bool initialized = false;

    // Subsystems
    std::unique_ptr<platform::Window> window;
    std::unique_ptr<graphics::GraphicsDevice> graphics_device;
    std::unique_ptr<Config> app_config;
    std::unique_ptr<GameLoop> game_loop;
    std::unique_ptr<platform::InputMapper> input_mapper;

    // User callbacks
    InitCallback init_callback;
    ShutdownCallback shutdown_callback;
    FixedUpdateCallback fixed_update_callback;
    UpdateCallback update_callback;
    RenderCallback render_callback;

    // State
    double fps_update_timer = 0.0;
    static constexpr double FPS_UPDATE_INTERVAL = 0.5;
};

Engine::Engine()
    : impl_(std::make_unique<Impl>()) {
    if (s_instance == nullptr) {
        s_instance = this;
    }
}

Engine::~Engine() {
    shutdown();

    if (s_instance == this) {
        s_instance = nullptr;
    }
}

bool Engine::initialize(const EngineConfig& config) {
    if (impl_->initialized) {
        REALCRAFT_LOG_WARN(log_category::ENGINE, "Engine already initialized");
        return true;
    }

    impl_->config = config;

    // Initialize logging first
    LoggerConfig log_config = config.logging;
    if (log_config.log_directory.empty()) {
        log_config.log_directory = platform::FileSystem::get_user_data_directory() / "logs";
    }
    Logger::initialize(log_config);

    REALCRAFT_LOG_INFO(log_category::ENGINE, "{} v{}", config.app_name, config.version);

    // Initialize memory tracking
    MemoryTracker::initialize();
    initialize_frame_allocator();

    // Initialize platform layer
    if (!platform::initialize()) {
        REALCRAFT_LOG_ERROR(log_category::ENGINE, "Failed to initialize platform layer");
        return false;
    }

    // Load or create configuration
    impl_->app_config = std::make_unique<Config>();
    std::filesystem::path config_path = config.config_path;
    if (config_path.empty()) {
        config_path = platform::FileSystem::get_user_config_directory() / "config.json";
    }
    impl_->app_config->load_or_create_default(config_path);

    // Create window with config settings
    impl_->window = platform::create_window();
    platform::Window::Config window_config = config.window;

    // Override window config with saved settings
    window_config.fullscreen = impl_->app_config->get_bool(
        config_section::GRAPHICS, config_key::FULLSCREEN, window_config.fullscreen);
    window_config.vsync = impl_->app_config->get_bool(
        config_section::GRAPHICS, config_key::VSYNC, window_config.vsync);
    window_config.width = static_cast<uint32_t>(impl_->app_config->get_int(
        config_section::GRAPHICS, config_key::RESOLUTION_WIDTH,
        static_cast<int>(window_config.width)));
    window_config.height = static_cast<uint32_t>(impl_->app_config->get_int(
        config_section::GRAPHICS, config_key::RESOLUTION_HEIGHT,
        static_cast<int>(window_config.height)));

    if (!impl_->window->initialize(window_config)) {
        REALCRAFT_LOG_ERROR(log_category::ENGINE, "Failed to create window");
        platform::shutdown();
        return false;
    }

    // Set up input mapper
    impl_->input_mapper = std::make_unique<platform::InputMapper>(impl_->window->get_input());
    impl_->input_mapper->load_defaults();

    // Create graphics device
    graphics::DeviceDesc device_desc;
    device_desc.metal_layer = impl_->window->get_metal_layer();
    device_desc.native_window = impl_->window->get_native_window();
    device_desc.enable_validation = config.enable_graphics_validation ||
        impl_->app_config->get_bool(config_section::GRAPHICS, config_key::VALIDATION_ENABLED, false);
    device_desc.vsync = window_config.vsync;

    impl_->graphics_device = graphics::create_graphics_device(device_desc);
    if (!impl_->graphics_device) {
        REALCRAFT_LOG_ERROR(log_category::ENGINE, "Failed to create graphics device");
        impl_->window->shutdown();
        platform::shutdown();
        return false;
    }

    REALCRAFT_LOG_INFO(log_category::ENGINE, "Graphics device: {}",
                       impl_->graphics_device->get_backend_name());
    auto caps = impl_->graphics_device->get_capabilities();
    REALCRAFT_LOG_INFO(log_category::ENGINE, "  Device: {}", caps.device_name);
    REALCRAFT_LOG_INFO(log_category::ENGINE, "  API: {}", caps.api_name);

    // Set up window resize callback
    impl_->window->set_framebuffer_resize_callback(
        [this](uint32_t width, uint32_t height) {
            REALCRAFT_LOG_INFO(log_category::ENGINE, "Framebuffer resized: {}x{}",
                              width, height);
            impl_->graphics_device->resize_swap_chain(width, height);
        });

    // Create and configure game loop
    impl_->game_loop = std::make_unique<GameLoop>();

    GameLoopConfig loop_config = config.game_loop;
    double target_fps = impl_->app_config->get_double(
        config_section::GRAPHICS, config_key::TARGET_FPS, loop_config.target_fps);
    loop_config.target_fps = target_fps;
    loop_config.vsync = window_config.vsync;

    impl_->game_loop->configure(loop_config);

    impl_->initialized = true;

    REALCRAFT_LOG_INFO(log_category::ENGINE, "Engine initialized successfully");

    return true;
}

void Engine::shutdown() {
    if (!impl_->initialized) {
        return;
    }

    REALCRAFT_LOG_INFO(log_category::ENGINE, "Shutting down...");

    // Call user shutdown callback
    if (impl_->shutdown_callback) {
        impl_->shutdown_callback();
    }

    // Save config if dirty
    if (impl_->app_config && impl_->app_config->is_dirty()) {
        impl_->app_config->save();
    }

    // Wait for GPU to finish
    if (impl_->graphics_device) {
        impl_->graphics_device->wait_idle();
    }

    // Cleanup in reverse order of initialization
    impl_->game_loop.reset();
    impl_->graphics_device.reset();
    impl_->input_mapper.reset();

    if (impl_->window) {
        impl_->window->shutdown();
        impl_->window.reset();
    }

    impl_->app_config.reset();

    platform::shutdown();

    // Memory tracking report
    MemoryTracker::log_stats();
    MemoryTracker::report_leaks();
    shutdown_frame_allocator();
    MemoryTracker::shutdown();

    REALCRAFT_LOG_INFO(log_category::ENGINE, "Goodbye!");
    Logger::shutdown();

    impl_->initialized = false;
}

bool Engine::is_initialized() const {
    return impl_->initialized;
}

void Engine::run() {
    if (!impl_->initialized) {
        REALCRAFT_LOG_ERROR(log_category::ENGINE, "Cannot run: engine not initialized");
        return;
    }

    // Call user init callback
    if (impl_->init_callback) {
        if (!impl_->init_callback()) {
            REALCRAFT_LOG_ERROR(log_category::ENGINE, "User init callback failed");
            return;
        }
    }

    // Set up game loop callbacks
    impl_->game_loop->set_fixed_update([this](double dt) {
        if (impl_->fixed_update_callback) {
            impl_->fixed_update_callback(dt);
        }
    });

    impl_->game_loop->set_update([this](double dt) {
        // Poll window events
        impl_->window->poll_events();

        // Check for window close
        if (impl_->window->should_close()) {
            impl_->game_loop->request_exit();
        }

        // Call user update callback
        if (impl_->update_callback) {
            impl_->update_callback(dt);
        }

        // Clear per-frame input state
        impl_->window->get_input()->end_frame();

        // Update FPS in window title
        impl_->fps_update_timer += impl_->game_loop->get_unscaled_delta_time();
        if (impl_->fps_update_timer >= Impl::FPS_UPDATE_INTERVAL) {
            impl_->fps_update_timer = 0.0;
            auto size = impl_->window->get_framebuffer_size();
            bool show_fps = impl_->app_config->get_bool(
                config_section::DEBUG, config_key::SHOW_FPS, true);
            if (show_fps) {
                std::string title = std::format("{} | {}x{} | {:.1f} FPS",
                    impl_->config.app_name,
                    size.x, size.y,
                    impl_->game_loop->get_average_fps());
                impl_->window->set_title(title);
            }
        }
    });

    impl_->game_loop->set_render([this](double interpolation) {
        if (impl_->render_callback) {
            impl_->render_callback(interpolation);
        }
    });

    REALCRAFT_LOG_INFO(log_category::ENGINE, "Entering main loop");

    // Main loop
    while (!impl_->game_loop->should_exit()) {
        impl_->game_loop->run_frame();
    }

    REALCRAFT_LOG_INFO(log_category::ENGINE, "Exiting main loop");
}

void Engine::request_exit() {
    if (impl_->game_loop) {
        impl_->game_loop->request_exit();
    }
    if (impl_->window) {
        impl_->window->set_should_close(true);
    }
}

platform::Window* Engine::get_window() {
    return impl_->window.get();
}

const platform::Window* Engine::get_window() const {
    return impl_->window.get();
}

graphics::GraphicsDevice* Engine::get_graphics_device() {
    return impl_->graphics_device.get();
}

const graphics::GraphicsDevice* Engine::get_graphics_device() const {
    return impl_->graphics_device.get();
}

Config* Engine::get_config() {
    return impl_->app_config.get();
}

const Config* Engine::get_config() const {
    return impl_->app_config.get();
}

GameLoop* Engine::get_game_loop() {
    return impl_->game_loop.get();
}

const GameLoop* Engine::get_game_loop() const {
    return impl_->game_loop.get();
}

platform::Input* Engine::get_input() {
    if (impl_->window) {
        return impl_->window->get_input();
    }
    return nullptr;
}

const platform::Input* Engine::get_input() const {
    if (impl_->window) {
        return impl_->window->get_input();
    }
    return nullptr;
}

void Engine::set_init_callback(InitCallback callback) {
    impl_->init_callback = std::move(callback);
}

void Engine::set_shutdown_callback(ShutdownCallback callback) {
    impl_->shutdown_callback = std::move(callback);
}

void Engine::set_fixed_update_callback(FixedUpdateCallback callback) {
    impl_->fixed_update_callback = std::move(callback);
}

void Engine::set_update_callback(UpdateCallback callback) {
    impl_->update_callback = std::move(callback);
}

void Engine::set_render_callback(RenderCallback callback) {
    impl_->render_callback = std::move(callback);
}

Engine* Engine::get_instance() {
    return s_instance;
}

}  // namespace realcraft::core
