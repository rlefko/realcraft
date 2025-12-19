// RealCraft Graphics Abstraction Layer
// render_pass.hpp - Render pass and framebuffer interfaces

#pragma once

#include "types.hpp"

#include <memory>

namespace realcraft::graphics {

// Abstract render pass class
// Describes the attachments and their load/store operations
// Implementations: MetalRenderPass, VulkanRenderPass
class RenderPass {
public:
    virtual ~RenderPass() = default;

    // Non-copyable
    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;

    // Properties
    [[nodiscard]] virtual size_t get_color_attachment_count() const = 0;
    [[nodiscard]] virtual bool has_depth_attachment() const = 0;
    [[nodiscard]] virtual TextureFormat get_color_format(size_t index) const = 0;
    [[nodiscard]] virtual TextureFormat get_depth_format() const = 0;

protected:
    RenderPass() = default;
};

// Abstract framebuffer class
// A collection of textures to render into
// Implementations: MetalFramebuffer, VulkanFramebuffer
class Framebuffer {
public:
    virtual ~Framebuffer() = default;

    // Non-copyable
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    // Properties
    [[nodiscard]] virtual uint32_t get_width() const = 0;
    [[nodiscard]] virtual uint32_t get_height() const = 0;
    [[nodiscard]] virtual size_t get_color_attachment_count() const = 0;
    [[nodiscard]] virtual Texture* get_color_attachment(size_t index) const = 0;
    [[nodiscard]] virtual Texture* get_depth_attachment() const = 0;

protected:
    Framebuffer() = default;
};

}  // namespace realcraft::graphics
