// RealCraft - Realistic Voxel Sandbox Game Engine
// main.cpp - Entry point

#include <realcraft/platform/input_action.hpp>
#include <realcraft/platform/platform.hpp>
#include <realcraft/platform/timer.hpp>
#include <realcraft/platform/window.hpp>

// Graphics
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <realcraft/graphics/command_buffer.hpp>
#include <realcraft/graphics/device.hpp>
#include <realcraft/graphics/swap_chain.hpp>
#include <realcraft/graphics/types.hpp>

// Bullet Physics
#include <btBulletDynamicsCommon.h>

// FastNoise2
#include <FastNoise/FastNoise.h>
#include <cmath>
#include <format>
#include <iostream>
#include <string>

namespace {

constexpr const char* VERSION = "0.1.0";

std::string get_platform_name() {
#if defined(REALCRAFT_PLATFORM_MACOS)
    return "macOS";
#elif defined(REALCRAFT_PLATFORM_WINDOWS)
    return "Windows";
#elif defined(REALCRAFT_PLATFORM_LINUX)
    return "Linux";
#else
    return "Unknown";
#endif
}

std::string get_architecture_name() {
#if defined(REALCRAFT_ARCH_ARM64)
    return "arm64";
#elif defined(REALCRAFT_ARCH_X64)
    return "x64";
#else
    return "unknown";
#endif
}

std::string get_graphics_api_name() {
#if defined(REALCRAFT_GRAPHICS_METAL)
    return "Metal";
#elif defined(REALCRAFT_GRAPHICS_VULKAN)
    return "Vulkan";
#elif defined(REALCRAFT_GRAPHICS_DX12)
    return "DirectX 12";
#else
    return "Unknown";
#endif
}

// Verify Bullet Physics integration
bool verify_bullet_physics() {
    // Create a collision configuration
    btDefaultCollisionConfiguration collision_config;

    // Create a dispatcher
    btCollisionDispatcher dispatcher(&collision_config);

    // Create a broadphase
    btDbvtBroadphase broadphase;

    // Create a solver
    btSequentialImpulseConstraintSolver solver;

    // Create the dynamics world
    btDiscreteDynamicsWorld dynamics_world(&dispatcher, &broadphase, &solver, &collision_config);

    // Set gravity
    dynamics_world.setGravity(btVector3(0, -9.81f, 0));

    // Verify gravity was set correctly
    btVector3 gravity = dynamics_world.getGravity();
    return (std::abs(gravity.getY() + 9.81f) < 0.001f);
}

// Verify FastNoise2 integration
bool verify_fastnoise2() {
    // Create a simple Perlin noise generator
    auto fn_simplex = FastNoise::New<FastNoise::Simplex>();
    auto fn_fractal = FastNoise::New<FastNoise::FractalFBm>();
    fn_fractal->SetSource(fn_simplex);
    fn_fractal->SetOctaveCount(4);

    // Generate a single noise value to verify it works
    float noise_value = fn_fractal->GenSingle2D(0.5f, 0.5f, 1337);

    // Noise should be in range [-1, 1] approximately (fractal can exceed slightly)
    return (noise_value >= -2.0f && noise_value <= 2.0f);
}

}  // namespace

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    // Initialize logging
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");

    // Print startup information
    spdlog::info("RealCraft v{}", VERSION);
    spdlog::info("Platform: {} ({})", get_platform_name(), get_architecture_name());
    spdlog::info("Graphics API: {}", get_graphics_api_name());

    // Verify dependencies
    if (!verify_bullet_physics()) {
        spdlog::error("Bullet Physics verification failed");
        return 1;
    }
    spdlog::info("Bullet Physics: OK");

    if (!verify_fastnoise2()) {
        spdlog::error("FastNoise2 verification failed");
        return 1;
    }
    spdlog::info("FastNoise2: OK");

    // Initialize platform layer
    if (!realcraft::platform::initialize()) {
        spdlog::error("Failed to initialize platform layer");
        return 1;
    }

    // Create window
    auto window = realcraft::platform::create_window();
    realcraft::platform::Window::Config config;
    config.title = "RealCraft";
    config.width = 1280;
    config.height = 720;
    config.fullscreen = false;
    config.vsync = true;

    if (!window->initialize(config)) {
        spdlog::error("Failed to create window");
        realcraft::platform::shutdown();
        return 1;
    }

    // Get input and set up input mapper
    auto* input = window->get_input();
    realcraft::platform::InputMapper input_mapper(input);
    input_mapper.load_defaults();

    // Set up frame timer
    realcraft::platform::FrameTimer frame_timer;

    // Track FPS for title update
    double fps_update_timer = 0.0;
    constexpr double FPS_UPDATE_INTERVAL = 0.5;  // Update title every 0.5 seconds

    spdlog::info("Window created successfully");
    spdlog::info("Controls:");
    spdlog::info("  ESC - Exit");
    spdlog::info("  F11 - Toggle fullscreen");
    spdlog::info("  Click - Capture mouse (ESC to release)");

    // Initialize graphics device
    realcraft::graphics::DeviceDesc device_desc;
    device_desc.metal_layer = window->get_metal_layer();
    device_desc.native_window = window->get_native_window();
    device_desc.enable_validation = true;
    device_desc.vsync = true;

    auto graphics_device = realcraft::graphics::create_graphics_device(device_desc);
    if (!graphics_device) {
        spdlog::error("Failed to create graphics device");
        window->shutdown();
        realcraft::platform::shutdown();
        return 1;
    }

    spdlog::info("Graphics device: {}", graphics_device->get_backend_name());
    auto caps = graphics_device->get_capabilities();
    spdlog::info("  Device: {}", caps.device_name);
    spdlog::info("  API: {}", caps.api_name);

    // Set up window resize callback to resize swap chain
    window->set_framebuffer_resize_callback([&graphics_device](uint32_t width, uint32_t height) {
        spdlog::info("Framebuffer resized: {}x{}", width, height);
        graphics_device->resize_swap_chain(width, height);
    });

    // Track time for animated clear color
    double total_time = 0.0;

    // Main game loop
    while (!window->should_close()) {
        frame_timer.begin_frame();

        // Poll input events
        window->poll_events();

        // Handle input
        if (input_mapper.is_action_just_pressed("toggle_pause")) {
            if (input->is_mouse_captured()) {
                input->set_mouse_captured(false);
                spdlog::info("Mouse released");
            } else {
                window->set_should_close(true);
            }
        }

        if (input_mapper.is_action_just_pressed("toggle_fullscreen")) {
            window->set_fullscreen(!window->is_fullscreen());
            spdlog::info("Fullscreen: {}", window->is_fullscreen() ? "ON" : "OFF");
        }

        // Capture mouse on left click
        if (input->is_mouse_button_just_pressed(realcraft::platform::MouseButton::Left)) {
            if (!input->is_mouse_captured()) {
                input->set_mouse_captured(true);
                spdlog::info("Mouse captured");
            }
        }

        // Show mouse delta when captured (for FPS camera control demo)
        if (input->is_mouse_captured()) {
            auto delta = input->get_mouse_delta();
            if (std::abs(delta.x) > 0.1 || std::abs(delta.y) > 0.1) {
                // In a real game, this would rotate the camera
                // spdlog::trace("Mouse delta: ({:.1f}, {:.1f})", delta.x, delta.y);
            }
        }

        // Clear per-frame input state
        input->end_frame();

        // Begin graphics frame
        graphics_device->begin_frame();

        // Create command buffer and render
        auto cmd = graphics_device->create_command_buffer();
        cmd->begin();

        // Get current swap chain texture and render to it
        auto* swap_chain = graphics_device->get_swap_chain();
        if (swap_chain && swap_chain->get_current_texture()) {
            // Animated clear color (cycles through rainbow)
            float r = 0.5f + 0.5f * std::sin(static_cast<float>(total_time));
            float g = 0.5f + 0.5f * std::sin(static_cast<float>(total_time) + 2.094f);
            float b = 0.5f + 0.5f * std::sin(static_cast<float>(total_time) + 4.189f);

            realcraft::graphics::RenderPassDesc pass_desc;
            pass_desc.color_attachments.push_back({realcraft::graphics::TextureFormat::BGRA8Unorm,
                                                   realcraft::graphics::LoadOp::Clear,
                                                   realcraft::graphics::StoreOp::Store});

            realcraft::graphics::ClearValue color_clear;
            color_clear.color = {r, g, b, 1.0f};
            realcraft::graphics::ClearValue depth_clear;

            cmd->begin_render_pass(pass_desc, swap_chain->get_current_texture(), nullptr, color_clear, depth_clear);
            cmd->end_render_pass();
        }

        cmd->end();
        graphics_device->submit(cmd.get(), false);

        // End graphics frame (present)
        graphics_device->end_frame();

        frame_timer.end_frame();
        total_time += frame_timer.get_unscaled_delta_time();

        // Update window title with FPS periodically
        fps_update_timer += frame_timer.get_unscaled_delta_time();
        if (fps_update_timer >= FPS_UPDATE_INTERVAL) {
            fps_update_timer = 0.0;
            auto size = window->get_framebuffer_size();
            std::string title = std::format("RealCraft | {}x{} | {:.1f} FPS | Frame: {}", size.x, size.y,
                                            frame_timer.get_average_fps(), frame_timer.get_frame_count());
            window->set_title(title);
        }
    }

    // Wait for GPU to finish before cleanup
    graphics_device->wait_idle();

    // Cleanup
    spdlog::info("Shutting down...");
    window->shutdown();
    realcraft::platform::shutdown();

    spdlog::info("Goodbye!");
    return 0;
}
