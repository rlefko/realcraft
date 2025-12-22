// RealCraft - Realistic Voxel Sandbox Game Engine
// main.cpp - Entry point

#include <realcraft/core/config.hpp>
#include <realcraft/core/engine.hpp>
#include <realcraft/core/logger.hpp>
#include <realcraft/graphics/command_buffer.hpp>
#include <realcraft/graphics/swap_chain.hpp>
#include <realcraft/graphics/types.hpp>
#include <realcraft/physics/physics_world.hpp>
#include <realcraft/physics/player_controller.hpp>
#include <realcraft/platform/input.hpp>
#include <realcraft/platform/input_action.hpp>
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

    // Initialize Player Controller
    physics::PlayerControllerConfig player_config;
    player_config.capsule_radius = 0.4;
    player_config.capsule_height = 1.8;
    player_config.eye_height = 1.6;
    player_config.walk_speed = 4.3;
    player_config.sprint_speed = 5.6;
    player_config.jump_height = 1.25;
    player_config.gravity = 32.0;

    physics::PlayerController player_controller;
    if (!player_controller.initialize(&physics_world, &world_manager, player_config)) {
        REALCRAFT_LOG_ERROR(core::log_category::ENGINE, "Failed to initialize Player Controller");
        physics_world.shutdown();
        render_system.shutdown();
        world_manager.shutdown();
        return 1;
    }
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "Player Controller: OK");

    // Set initial player position (above ground)
    player_controller.set_position({0.0, 80.0, 0.0});

    // Set initial camera rotation (looking forward)
    render_system.get_camera().set_rotation(-90.0f, -20.0f);

    // Set initial player position for chunk loading
    world_manager.set_player_position({0.0, 80.0, 0.0});

    // Pre-load central chunk synchronously so there's something to see immediately
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "Pre-loading central chunk...");
    (void)world_manager.load_chunk_sync({0, 0});
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "Central chunk loaded");

    // Shared player input state (updated in update callback, used in fixed_update)
    physics::PlayerInput player_input;

    // Create input mapper for rebindable controls
    platform::InputMapper input_mapper(engine.get_input());
    input_mapper.load_defaults();

    // Log controls
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "Controls:");
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "  WASD - Move player");
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "  Space - Jump");
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "  Shift - Sprint");
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "  Ctrl - Crouch");
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "  Mouse - Look around");
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "  Click - Capture mouse");
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "  ESC - Release mouse / Exit");
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "  F11 - Toggle fullscreen");
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "  T - Toggle wireframe");
    REALCRAFT_LOG_INFO(core::log_category::ENGINE, "  P - Pause/resume day-night cycle");

    // Fixed update callback (60 Hz physics)
    engine.set_fixed_update_callback([&render_system, &physics_world, &player_controller, &player_input](double dt) {
        // Store previous state for interpolation
        player_controller.store_previous_state();

        // Update player physics
        player_controller.fixed_update(dt, player_input);

        // Reset jump just pressed (consumed by physics)
        player_input.jump_just_pressed = false;

        // Update physics world
        physics_world.fixed_update(dt);
        render_system.fixed_update(dt);
    });

    // Variable update callback (every frame)
    engine.set_update_callback([&engine, &world_manager, &render_system, &player_controller, &player_input,
                                &input_mapper](double dt) {
        auto* input = engine.get_input();
        auto* window = engine.get_window();

        // Handle UI/system input (not rebindable)
        if (input_mapper.is_action_just_pressed("toggle_pause")) {
            if (input->is_mouse_captured()) {
                input->set_mouse_captured(false);
                REALCRAFT_LOG_INFO(core::log_category::INPUT, "Mouse released");
            } else {
                engine.request_exit();
            }
        }

        if (input_mapper.is_action_just_pressed("toggle_fullscreen")) {
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
        if (input_mapper.is_action_just_pressed("primary_action")) {
            if (!input->is_mouse_captured()) {
                input->set_mouse_captured(true);
                REALCRAFT_LOG_INFO(core::log_category::INPUT, "Mouse captured");
            }
        }

        // Mouse look (only when captured)
        if (input->is_mouse_captured()) {
            auto mouse_delta = input->get_mouse_delta();
            float sensitivity = render_system.get_camera().get_config().mouse_sensitivity;
            render_system.get_camera().rotate(-static_cast<float>(mouse_delta.y) * sensitivity,
                                              static_cast<float>(mouse_delta.x) * sensitivity);
        }

        // Build player movement input from action mappings
        glm::vec3 local_move(0.0f);
        if (input_mapper.is_action_pressed("move_forward")) {
            local_move.z += 1.0f;
        }
        if (input_mapper.is_action_pressed("move_backward")) {
            local_move.z -= 1.0f;
        }
        if (input_mapper.is_action_pressed("move_right")) {
            local_move.x += 1.0f;
        }
        if (input_mapper.is_action_pressed("move_left")) {
            local_move.x -= 1.0f;
        }

        // Transform local movement to world-space based on camera facing
        glm::vec3 forward = render_system.get_camera().get_forward_horizontal();
        glm::vec3 right = render_system.get_camera().get_right();

        glm::vec3 world_move = forward * local_move.z + right * local_move.x;
        if (glm::length(world_move) > 0.01f) {
            world_move = glm::normalize(world_move);
        }

        // Update player input for physics using action mappings
        player_input.move_direction = world_move;
        player_input.sprint_pressed = input_mapper.is_action_pressed("sprint");
        player_input.crouch_pressed = input_mapper.is_action_pressed("crouch");
        player_input.jump_pressed = input_mapper.is_action_pressed("jump");
        if (input_mapper.is_action_just_pressed("jump")) {
            player_input.jump_just_pressed = true;
        }

        // Sync camera position to interpolated player eye position
        double interpolation = engine.get_game_loop()->get_interpolation();
        glm::dvec3 eye_pos = player_controller.get_interpolated_eye_position(interpolation);
        render_system.get_camera().set_position(eye_pos);

        // Update player position for chunk loading
        world_manager.set_player_position(player_controller.get_position());

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
    player_controller.shutdown();
    physics_world.shutdown();
    render_system.shutdown();
    world_manager.shutdown();

    return 0;
}
