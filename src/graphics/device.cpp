// RealCraft Graphics Abstraction Layer
// device.cpp - Graphics device factory function

#include <spdlog/spdlog.h>

#include <realcraft/graphics/device.hpp>
#include <stdexcept>

// Platform-specific device declarations
#if defined(REALCRAFT_PLATFORM_MACOS)
namespace realcraft::graphics {
std::unique_ptr<GraphicsDevice> create_metal_device(const DeviceDesc& desc);
}
#elif defined(REALCRAFT_PLATFORM_LINUX) || defined(REALCRAFT_PLATFORM_WINDOWS)
namespace realcraft::graphics {
std::unique_ptr<GraphicsDevice> create_vulkan_device(const DeviceDesc& desc);
}
#endif

namespace realcraft::graphics {

std::unique_ptr<GraphicsDevice> create_graphics_device(const DeviceDesc& desc) {
#if defined(REALCRAFT_PLATFORM_MACOS)
    spdlog::info("Creating Metal graphics device");
    return create_metal_device(desc);
#elif defined(REALCRAFT_PLATFORM_LINUX)
    spdlog::info("Creating Vulkan graphics device (Linux)");
    return create_vulkan_device(desc);
#elif defined(REALCRAFT_PLATFORM_WINDOWS)
    spdlog::info("Creating Vulkan graphics device (Windows)");
    return create_vulkan_device(desc);
#else
    spdlog::error("No graphics backend available for this platform");
    throw std::runtime_error("No graphics backend available for this platform");
#endif
}

}  // namespace realcraft::graphics
