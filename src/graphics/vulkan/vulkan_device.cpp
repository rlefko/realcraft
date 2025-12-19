// RealCraft Graphics Abstraction Layer
// vulkan_device.cpp - Vulkan graphics device stub (not yet implemented)

#include <realcraft/graphics/device.hpp>

#include <spdlog/spdlog.h>

#include <stdexcept>

#if defined(REALCRAFT_PLATFORM_LINUX) || defined(REALCRAFT_PLATFORM_WINDOWS)

namespace realcraft::graphics {

// ============================================================================
// VulkanDevice Stub Implementation
// ============================================================================

class VulkanDevice : public GraphicsDevice {
public:
    VulkanDevice() = default;
    ~VulkanDevice() override = default;

    std::unique_ptr<Buffer> create_buffer(const BufferDesc& /*desc*/) override {
        throw std::runtime_error("VulkanDevice::create_buffer not implemented");
    }

    std::unique_ptr<Texture> create_texture(const TextureDesc& /*desc*/) override {
        throw std::runtime_error("VulkanDevice::create_texture not implemented");
    }

    std::unique_ptr<Sampler> create_sampler(const SamplerDesc& /*desc*/) override {
        throw std::runtime_error("VulkanDevice::create_sampler not implemented");
    }

    std::unique_ptr<Shader> create_shader(const ShaderDesc& /*desc*/) override {
        throw std::runtime_error("VulkanDevice::create_shader not implemented");
    }

    std::unique_ptr<Pipeline> create_pipeline(const PipelineDesc& /*desc*/) override {
        throw std::runtime_error("VulkanDevice::create_pipeline not implemented");
    }

    std::unique_ptr<ComputePipeline> create_compute_pipeline(
        const ComputePipelineDesc& /*desc*/) override {
        throw std::runtime_error("VulkanDevice::create_compute_pipeline not implemented");
    }

    std::unique_ptr<RenderPass> create_render_pass(const RenderPassDesc& /*desc*/) override {
        throw std::runtime_error("VulkanDevice::create_render_pass not implemented");
    }

    std::unique_ptr<Framebuffer> create_framebuffer(const FramebufferDesc& /*desc*/) override {
        throw std::runtime_error("VulkanDevice::create_framebuffer not implemented");
    }

    std::unique_ptr<CommandBuffer> create_command_buffer() override {
        throw std::runtime_error("VulkanDevice::create_command_buffer not implemented");
    }

    void submit(CommandBuffer* /*cmd*/, bool /*wait_for_completion*/) override {
        throw std::runtime_error("VulkanDevice::submit not implemented");
    }

    void wait_idle() override {
        throw std::runtime_error("VulkanDevice::wait_idle not implemented");
    }

    SwapChain* get_swap_chain() override {
        throw std::runtime_error("VulkanDevice::get_swap_chain not implemented");
    }

    void resize_swap_chain(uint32_t /*width*/, uint32_t /*height*/) override {
        throw std::runtime_error("VulkanDevice::resize_swap_chain not implemented");
    }

    void begin_frame() override {
        throw std::runtime_error("VulkanDevice::begin_frame not implemented");
    }

    void end_frame() override {
        throw std::runtime_error("VulkanDevice::end_frame not implemented");
    }

    DeviceCapabilities get_capabilities() const override {
        DeviceCapabilities caps;
        caps.device_name = "Vulkan (Not Implemented)";
        caps.api_name = "Vulkan";
        return caps;
    }

    const char* get_backend_name() const override { return "Vulkan (Stub)"; }
};

// Factory function
std::unique_ptr<GraphicsDevice> create_vulkan_device(const DeviceDesc& /*desc*/) {
    spdlog::warn("Vulkan backend is not yet implemented. Returning stub device.");
    return std::make_unique<VulkanDevice>();
}

}  // namespace realcraft::graphics

#endif  // REALCRAFT_PLATFORM_LINUX || REALCRAFT_PLATFORM_WINDOWS
