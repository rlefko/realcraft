// RealCraft Graphics Abstraction Layer
// swap_chain.hpp - Swap chain interface

#pragma once

#include "types.hpp"

#include <memory>

namespace realcraft::graphics {

// Forward declarations
class Texture;

// Abstract swap chain class
// Manages presentation surfaces and frame synchronization
// Implementations: MetalSwapChain, VulkanSwapChain
class SwapChain {
public:
    virtual ~SwapChain() = default;

    // Non-copyable
    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;

    // Properties
    [[nodiscard]] virtual uint32_t get_width() const = 0;
    [[nodiscard]] virtual uint32_t get_height() const = 0;
    [[nodiscard]] virtual TextureFormat get_format() const = 0;
    [[nodiscard]] virtual uint32_t get_image_count() const = 0;

    // Get current drawable texture
    // Only valid between begin_frame() and end_frame() on the device
    [[nodiscard]] virtual Texture* get_current_texture() = 0;

    // Get the index of the current frame (for multi-buffering)
    [[nodiscard]] virtual uint32_t get_current_frame_index() const = 0;

protected:
    SwapChain() = default;
};

}  // namespace realcraft::graphics
