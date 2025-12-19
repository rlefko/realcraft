// RealCraft - Realistic Voxel Sandbox Game Engine
// main.cpp - Entry point

#include <realcraft/core/config.hpp>
#include <realcraft/core/engine.hpp>
#include <realcraft/core/logger.hpp>
#include <realcraft/graphics/command_buffer.hpp>
#include <realcraft/graphics/swap_chain.hpp>
#include <realcraft/graphics/types.hpp>
#include <realcraft/platform/input.hpp>

// Bullet Physics
#include <btBulletDynamicsCommon.h>

// FastNoise2
#include <FastNoise/FastNoise.h>
#include <cmath>

namespace {

constexpr const char* VERSION = "0.1.0";

// Verify Bullet Physics integration
bool verify_bullet_physics() {
    btDefaultCollisionConfiguration collision_config;
    btCollisionDispatcher dispatcher(&collision_config);
    btDbvtBroadphase broadphase;
    btSequentialImpulseConstraintSolver solver;
    btDiscreteDynamicsWorld dynamics_world(&dispatcher, &broadphase, &solver, &collision_config);
    dynamics_world.setGravity(btVector3(0, -9.81f, 0));
    btVector3 gravity = dynamics_world.getGravity();
    return (std::abs(gravity.getY() + 9.81f) < 0.001f);
}

// Verify FastNoise2 integration
bool verify_fastnoise2() {
    auto fn_simplex = FastNoise::New<FastNoise::Simplex>();
    auto fn_fractal = FastNoise::New<FastNoise::FractalFBm>();
    fn_fractal->SetSource(fn_simplex);
    fn_fractal->SetOctaveCount(4);
    float noise_value = fn_fractal->GenSingle2D(0.5f, 0.5f, 1337);
    return (noise_value >= -2.0f && noise_value <= 2.0f);
}

}  // namespace

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    using namespace realcraft;

    // Configure engine
    core::EngineConfig config;
    config.app_name = "RealCraft";
    config.version = VERSION;

    // Window settings
    config.window.title = "RealCraft";
    config.window.width = 1280;
    config.window.height = 720;
    config.window.fullscreen = false;
    config.window.vsync = true;

    // Enable graphics validation in debug builds
#ifdef REALCRAFT_DEBUG
    config.enable_graphics_validation = true;
#endif

    // Create engine
    core::Engine engine;

    if (!engine.initialize(config)) {
        return 1;
    }

    // Verify dependencies after logger is initialized
    if (!verify_bullet_physics()) {
        REALCRAFT_LOG_ERROR(core::log_category::ENGINE, "Bullet Physics verification failed");
        return 1;
    }
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "Bullet Physics: OK");

    if (!verify_fastnoise2()) {
        REALCRAFT_LOG_ERROR(core::log_category::ENGINE, "FastNoise2 verification failed");
        return 1;
    }
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "FastNoise2: OK");

    // Demo state
    double total_time = 0.0;

    // Log controls
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "Controls:");
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "  ESC - Exit");
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "  F11 - Toggle fullscreen");
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "  Click - Capture mouse (ESC to release)");

    // Fixed update callback (60 Hz physics)
    engine.set_fixed_update_callback([](double /*dt*/) {
        // Physics updates will go here
    });

    // Variable update callback (every frame)
    engine.set_update_callback([&engine, &total_time](double dt) {
        total_time += dt;

        auto* input = engine.get_input();
        auto* window = engine.get_window();

        // Handle input
        if (input->is_key_just_pressed(platform::KeyCode::Escape)) {
            if (input->is_mouse_captured()) {
                input->set_mouse_captured(false);
                REALCRAFT_LOG_INFO(core::log_category::INPUT, "Mouse released");
            } else {
                engine.request_exit();
            }
        }

        if (input->is_key_just_pressed(platform::KeyCode::F11)) {
            window->set_fullscreen(!window->is_fullscreen());
            REALCRAFT_LOG_INFO(core::log_category::ENGINE, "Fullscreen: {}", window->is_fullscreen() ? "ON" : "OFF");
        }

        // Capture mouse on left click
        if (input->is_mouse_button_just_pressed(platform::MouseButton::Left)) {
            if (!input->is_mouse_captured()) {
                input->set_mouse_captured(true);
                REALCRAFT_LOG_INFO(core::log_category::INPUT, "Mouse captured");
            }
        }
    });

    // Render callback
    engine.set_render_callback([&engine, &total_time](double /*interpolation*/) {
        auto* device = engine.get_graphics_device();

        device->begin_frame();

        auto cmd = device->create_command_buffer();
        cmd->begin();

        auto* swap_chain = device->get_swap_chain();
        if (swap_chain && swap_chain->get_current_texture()) {
            // Animated clear color (cycles through rainbow)
            float r = 0.5f + 0.5f * std::sin(static_cast<float>(total_time));
            float g = 0.5f + 0.5f * std::sin(static_cast<float>(total_time) + 2.094f);
            float b = 0.5f + 0.5f * std::sin(static_cast<float>(total_time) + 4.189f);

            graphics::RenderPassDesc pass_desc;
            pass_desc.color_attachments.push_back(
                {graphics::TextureFormat::BGRA8Unorm, graphics::LoadOp::Clear, graphics::StoreOp::Store});

            graphics::ClearValue color_clear;
            color_clear.color = {r, g, b, 1.0f};
            graphics::ClearValue depth_clear;

            cmd->begin_render_pass(pass_desc, swap_chain->get_current_texture(), nullptr, color_clear, depth_clear);
            cmd->end_render_pass();
        }

        cmd->end();
        device->submit(cmd.get(), false);

        device->end_frame();
    });

    // Run the engine (blocks until exit)
    engine.run();

    return 0;
}
