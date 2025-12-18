// RealCraft - Realistic Voxel Sandbox Game Engine
// main.cpp - Entry point

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

// Bullet Physics
#include <btBulletDynamicsCommon.h>

// FastNoise2
#include <FastNoise/FastNoise.h>

#include <cmath>
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
    btDiscreteDynamicsWorld dynamics_world(
        &dispatcher, &broadphase, &solver, &collision_config);

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

    // Verify GLM is working
    glm::vec3 test_vector(1.0f, 2.0f, 3.0f);
    spdlog::info("GLM test: vec3({}, {}, {})", test_vector.x, test_vector.y, test_vector.z);

    // Verify nlohmann_json is working
    nlohmann::json test_json = {{"engine", "RealCraft"}, {"version", VERSION}};
    spdlog::info("JSON test: {}", test_json.dump());

    // Verify Bullet Physics is working
    if (verify_bullet_physics()) {
        spdlog::info("Bullet Physics test: PASSED (dynamics world created, gravity set)");
    } else {
        spdlog::error("Bullet Physics test: FAILED");
        return 1;
    }

    // Verify FastNoise2 is working
    if (verify_fastnoise2()) {
        spdlog::info("FastNoise2 test: PASSED (fractal noise generated)");
    } else {
        spdlog::error("FastNoise2 test: FAILED");
        return 1;
    }

    spdlog::info("Build system verification complete!");

    return 0;
}
