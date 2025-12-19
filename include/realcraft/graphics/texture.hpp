// RealCraft Graphics Abstraction Layer
// texture.hpp - GPU texture interface

#pragma once

#include "types.hpp"

#include <memory>

namespace realcraft::graphics {

// Abstract GPU texture class
// Implementations: MetalTexture, VulkanTexture
class Texture {
public:
    virtual ~Texture() = default;

    // Non-copyable
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // Properties
    [[nodiscard]] virtual TextureType get_type() const = 0;
    [[nodiscard]] virtual TextureFormat get_format() const = 0;
    [[nodiscard]] virtual uint32_t get_width() const = 0;
    [[nodiscard]] virtual uint32_t get_height() const = 0;
    [[nodiscard]] virtual uint32_t get_depth() const = 0;
    [[nodiscard]] virtual uint32_t get_mip_levels() const = 0;
    [[nodiscard]] virtual uint32_t get_array_layers() const = 0;
    [[nodiscard]] virtual TextureUsage get_usage() const = 0;

    // Get native texture handle (MTLTexture*, VkImage, etc.)
    // Returns nullptr for textures that don't have a native handle
    [[nodiscard]] virtual void* get_native_handle() const { return nullptr; }

protected:
    Texture() = default;
};

}  // namespace realcraft::graphics
