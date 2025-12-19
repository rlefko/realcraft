// RealCraft Graphics Abstraction Layer
// device.hpp - Graphics device interface

#pragma once

#include "types.hpp"

#include <memory>

namespace realcraft::graphics {

// Forward declarations
class Buffer;
class CommandBuffer;
class ComputePipeline;
class Framebuffer;
class Pipeline;
class RenderPass;
class Sampler;
class Shader;
class SwapChain;
class Texture;

// Device creation configuration
struct DeviceDesc {
    void* window_handle = nullptr;   // GLFW window from Window::get_native_handle()
    void* metal_layer = nullptr;     // CAMetalLayer* from Window::get_metal_layer() (macOS)
    void* native_window = nullptr;   // NSWindow*/HWND from Window::get_native_window()
    bool enable_validation = false;  // Enable debug/validation layers
    bool vsync = true;               // Enable vertical sync
};

// Abstract graphics device class
// Implementations: MetalDevice, VulkanDevice
class GraphicsDevice {
public:
    virtual ~GraphicsDevice() = default;

    // Non-copyable, non-movable
    GraphicsDevice(const GraphicsDevice&) = delete;
    GraphicsDevice& operator=(const GraphicsDevice&) = delete;
    GraphicsDevice(GraphicsDevice&&) = delete;
    GraphicsDevice& operator=(GraphicsDevice&&) = delete;

    // ========================================================================
    // Resource Creation
    // ========================================================================

    [[nodiscard]] virtual std::unique_ptr<Buffer> create_buffer(const BufferDesc& desc) = 0;
    [[nodiscard]] virtual std::unique_ptr<Texture> create_texture(const TextureDesc& desc) = 0;
    [[nodiscard]] virtual std::unique_ptr<Sampler> create_sampler(const SamplerDesc& desc) = 0;
    [[nodiscard]] virtual std::unique_ptr<Shader> create_shader(const ShaderDesc& desc) = 0;
    [[nodiscard]] virtual std::unique_ptr<Pipeline> create_pipeline(const PipelineDesc& desc) = 0;
    [[nodiscard]] virtual std::unique_ptr<ComputePipeline> create_compute_pipeline(const ComputePipelineDesc& desc) = 0;
    [[nodiscard]] virtual std::unique_ptr<RenderPass> create_render_pass(const RenderPassDesc& desc) = 0;
    [[nodiscard]] virtual std::unique_ptr<Framebuffer> create_framebuffer(const FramebufferDesc& desc) = 0;

    // ========================================================================
    // Command Buffers
    // ========================================================================

    [[nodiscard]] virtual std::unique_ptr<CommandBuffer> create_command_buffer() = 0;
    virtual void submit(CommandBuffer* cmd, bool wait_for_completion = false) = 0;
    virtual void wait_idle() = 0;

    // ========================================================================
    // Swap Chain
    // ========================================================================

    [[nodiscard]] virtual SwapChain* get_swap_chain() = 0;
    virtual void resize_swap_chain(uint32_t width, uint32_t height) = 0;

    // ========================================================================
    // Frame Management
    // ========================================================================

    // Call at start of frame to acquire next drawable
    virtual void begin_frame() = 0;

    // Call at end of frame to present
    virtual void end_frame() = 0;

    // ========================================================================
    // Device Info
    // ========================================================================

    [[nodiscard]] virtual DeviceCapabilities get_capabilities() const = 0;
    [[nodiscard]] virtual const char* get_backend_name() const = 0;

protected:
    GraphicsDevice() = default;
};

// Factory function - creates appropriate backend based on platform
// On macOS: creates MetalDevice
// On Linux: creates VulkanDevice
// On Windows: creates VulkanDevice or DX12Device based on configuration
[[nodiscard]] std::unique_ptr<GraphicsDevice> create_graphics_device(const DeviceDesc& desc);

}  // namespace realcraft::graphics
