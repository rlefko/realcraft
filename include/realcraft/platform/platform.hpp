// RealCraft Platform Abstraction Layer
// platform.hpp - Common platform types and forward declarations

#pragma once

#include <glm/vec2.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace realcraft::platform {

// Forward declarations
class Window;
class Input;
class InputMapper;

// Monitor information for multi-display support
struct MonitorInfo {
    std::string name;
    int32_t index = 0;
    int32_t width = 0;
    int32_t height = 0;
    int32_t refresh_rate = 0;
    int32_t position_x = 0;
    int32_t position_y = 0;
    bool is_primary = false;
};

// Video mode for fullscreen
struct VideoMode {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t refresh_rate = 0;
};

// Initialize the platform layer (GLFW). Call once at startup.
bool initialize();

// Shutdown the platform layer. Call once at program exit.
void shutdown();

// Check if platform is initialized
bool is_initialized();

}  // namespace realcraft::platform
