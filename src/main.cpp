// RealCraft - Realistic Voxel Sandbox Game Engine
// main.cpp - Entry point

#include <realcraft/core/config.hpp>
#include <realcraft/core/engine.hpp>
#include <realcraft/core/logger.hpp>
#include <realcraft/graphics/command_buffer.hpp>
#include <realcraft/graphics/swap_chain.hpp>
#include <realcraft/graphics/types.hpp>
#include <realcraft/physics/physics_world.hpp>
#include <realcraft/platform/input.hpp>
#include <realcraft/rendering/render_system.hpp>
#include <realcraft/world/world_manager.hpp>

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

    // Initialize World Manager
    world::WorldConfig world_config;
    world_config.name = "demo_world";
    world_config.seed = 12345;
    world_config.view_distance = 4;
    world_config.generation_threads = 2;
    world_config.enable_saving = false;  // Demo mode, no persistence

    world::WorldManager world_manager;
    if (!world_manager.initialize(world_config)) {
        REALCRAFT_LOG_ERROR(core::log_category::ENGINE, "Failed to initialize World Manager");
        return 1;
    }
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "World Manager: OK");

    // Initialize Render System
    rendering::RenderSystemConfig render_config;
    render_config.camera.fov_degrees = 70.0f;
    render_config.camera.mouse_sensitivity = 0.1f;
    render_config.camera.move_speed = 10.0f;
    render_config.render_distance = 4.0f;
    render_config.mesh_manager.worker_threads = 2;

    rendering::RenderSystem render_system;
    if (!render_system.initialize(&engine, &world_manager, render_config)) {
        REALCRAFT_LOG_ERROR(core::log_category::ENGINE, "Failed to initialize Render System");
        world_manager.shutdown();
        return 1;
    }
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "Render System: OK");

    // Initialize Physics World
    physics::PhysicsConfig physics_config;
    physics_config.gravity = glm::dvec3(0.0, -9.81, 0.0);
    physics_config.fixed_timestep = 1.0 / 60.0;

    physics::PhysicsWorld physics_world;
    if (!physics_world.initialize(&world_manager, physics_config)) {
        REALCRAFT_LOG_ERROR(core::log_category::ENGINE, "Failed to initialize Physics World");
        render_system.shutdown();
        world_manager.shutdown();
        return 1;
    }
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "Physics World: OK");

    // Set initial camera position (above ground, looking around)
    render_system.get_camera().set_position({0.0, 80.0, 0.0});
    render_system.get_camera().set_rotation(-90.0f, -20.0f);

    // Set initial player position for chunk loading
    world_manager.set_player_position({0.0, 80.0, 0.0});

    // Log controls
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "Controls:");
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "  WASD - Move camera");
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "  Space/Shift - Fly up/down");
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "  Mouse - Look around");
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "  Click - Capture mouse");
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "  ESC - Release mouse / Exit");
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "  F11 - Toggle fullscreen");
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "  T - Toggle wireframe");
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "  P - Pause/resume day-night cycle");

    // Fixed update callback (60 Hz physics)
    engine.set_fixed_update_callback([&render_system, &physics_world](double dt) {
        physics_world.fixed_update(dt);
        render_system.fixed_update(dt);
    });

    // Variable update callback (every frame)
    engine.set_update_callback([&engine, &world_manager, &render_system](double dt) {
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

        if (input->is_key_just_pressed(platform::KeyCode::T)) {
            render_system.set_wireframe(!render_system.is_wireframe());
            REALCRAFT_LOG_INFO(core::log_category::GRAPHICS, "Wireframe: {}",
                               render_system.is_wireframe() ? "ON" : "OFF");
        }

        if (input->is_key_just_pressed(platform::KeyCode::P)) {
            auto& cycle = render_system.get_day_night_cycle();
            cycle.set_paused(!cycle.is_paused());
            REALCRAFT_LOG_INFO(core::log_category::GRAPHICS, "Day-night cycle: {}",
                               cycle.is_paused() ? "PAUSED" : "RUNNING");
        }

        // Capture mouse on left click
        if (input->is_mouse_button_just_pressed(platform::MouseButton::Left)) {
            if (!input->is_mouse_captured()) {
                input->set_mouse_captured(true);
                REALCRAFT_LOG_INFO(core::log_category::INPUT, "Mouse captured");
            }
        }

        // Build camera movement input
        glm::vec3 move_input(0.0f);
        bool sprinting = input->is_key_pressed(platform::KeyCode::LeftControl);

        // Movement
        if (input->is_key_pressed(platform::KeyCode::W)) {
            move_input.z += 1.0f;
        }
        if (input->is_key_pressed(platform::KeyCode::S)) {
            move_input.z -= 1.0f;
        }
        if (input->is_key_pressed(platform::KeyCode::D)) {
            move_input.x += 1.0f;
        }
        if (input->is_key_pressed(platform::KeyCode::A)) {
            move_input.x -= 1.0f;
        }
        if (input->is_key_pressed(platform::KeyCode::Space)) {
            move_input.y += 1.0f;
        }
        if (input->is_key_pressed(platform::KeyCode::LeftShift)) {
            move_input.y -= 1.0f;
        }

        // Mouse look (only when captured)
        if (input->is_mouse_captured()) {
            auto mouse_delta = input->get_mouse_delta();
            float sensitivity = render_system.get_camera().get_config().mouse_sensitivity;
            render_system.get_camera().rotate(-static_cast<float>(mouse_delta.y) * sensitivity,
                                              static_cast<float>(mouse_delta.x) * sensitivity);
        }

        // Apply camera movement
        render_system.get_camera().process_movement(move_input, static_cast<float>(dt), sprinting);

        // Update player position for chunk loading
        auto camera_pos = render_system.get_camera().get_position();
        world_manager.set_player_position(camera_pos);

        // Update world (chunk loading/unloading)
        world_manager.update(dt);

        // Update render system
        render_system.update(dt);
    });

    // Render callback
    engine.set_render_callback([&render_system](double interpolation) { render_system.render(interpolation); });

    // Run the engine (blocks until exit)
    engine.run();

    // Cleanup
    physics_world.shutdown();
    render_system.shutdown();
    world_manager.shutdown();

    return 0;
}
