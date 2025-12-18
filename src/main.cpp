// RealCraft - Realistic Voxel Sandbox Game Engine
// main.cpp - Entry point

#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

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

} // namespace

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
    nlohmann::json test_json = {
        {"engine", "RealCraft"},
        {"version", VERSION}
    };
    spdlog::info("JSON test: {}", test_json.dump());

    spdlog::info("Build system verification complete!");

    return 0;
}
