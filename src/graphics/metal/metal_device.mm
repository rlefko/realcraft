// RealCraft Graphics Abstraction Layer
// metal_device.mm - Metal graphics device implementation

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "metal_impl.hpp"
#include "metal_types.hpp"

#include <spdlog/spdlog.h>

#include <stdexcept>

namespace realcraft::graphics {

// Forward declaration
class MetalSwapChain;

// ============================================================================
// MetalDevice Implementation
// ============================================================================

class MetalDevice : public GraphicsDevice {
public:
    MetalDevice() = default;
    ~MetalDevice() override;

    bool initialize(const DeviceDesc& desc);

    // Resource creation
    std::unique_ptr<Buffer> create_buffer(const BufferDesc& desc) override;
    std::unique_ptr<Texture> create_texture(const TextureDesc& desc) override;
    std::unique_ptr<Sampler> create_sampler(const SamplerDesc& desc) override;
    std::unique_ptr<Shader> create_shader(const ShaderDesc& desc) override;
    std::unique_ptr<Pipeline> create_pipeline(const PipelineDesc& desc) override;
    std::unique_ptr<ComputePipeline> create_compute_pipeline(const ComputePipelineDesc& desc) override;
    std::unique_ptr<RenderPass> create_render_pass(const RenderPassDesc& desc) override;
    std::unique_ptr<Framebuffer> create_framebuffer(const FramebufferDesc& desc) override;

    // Command buffers
    std::unique_ptr<CommandBuffer> create_command_buffer() override;
    void submit(CommandBuffer* cmd, bool wait_for_completion) override;
    void wait_idle() override;

    // Swap chain
    SwapChain* get_swap_chain() override;
    void resize_swap_chain(uint32_t width, uint32_t height) override;

    // Frame management
    void begin_frame() override;
    void end_frame() override;

    // Device info
    DeviceCapabilities get_capabilities() const override;
    const char* get_backend_name() const override { return "Metal"; }

    // Internal accessors for other Metal classes
    id<MTLDevice> get_mtl_device() const { return device_; }
    id<MTLCommandQueue> get_command_queue() const { return command_queue_; }
    id<CAMetalDrawable> get_current_drawable() const { return current_drawable_; }

private:
    id<MTLDevice> device_ = nil;
    id<MTLCommandQueue> command_queue_ = nil;
    CAMetalLayer* metal_layer_ = nil;
    id<CAMetalDrawable> current_drawable_ = nil;
    std::unique_ptr<MetalSwapChain> swap_chain_;
    bool vsync_ = true;
    bool in_frame_ = false;
};

// ============================================================================
// MetalSwapChain Implementation
// ============================================================================

class MetalSwapChain : public SwapChain {
public:
    MetalSwapChain(MetalDevice* device, CAMetalLayer* layer);
    ~MetalSwapChain() override = default;

    uint32_t get_width() const override { return width_; }
    uint32_t get_height() const override { return height_; }
    TextureFormat get_format() const override { return format_; }
    uint32_t get_image_count() const override { return 3; }  // Triple buffering
    Texture* get_current_texture() override;
    uint32_t get_current_frame_index() const override { return frame_index_; }

    void resize(uint32_t width, uint32_t height);
    void set_current_texture(id<MTLTexture> texture);
    void clear_current_texture();

private:
    CAMetalLayer* layer_ = nil;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    TextureFormat format_ = TextureFormat::BGRA8Unorm;
    uint32_t frame_index_ = 0;

    // Wrapper texture for the current drawable
    class DrawableTexture;
    std::unique_ptr<DrawableTexture> current_texture_;
};

// Wrapper texture class for swap chain drawable
// Caches the MTLTexture directly to avoid repeated property access through drawable
class MetalSwapChain::DrawableTexture : public Texture {
public:
    DrawableTexture() = default;

    void set_current_texture(id<MTLTexture> texture, uint32_t width, uint32_t height) {
        cached_texture_ = texture;
        width_ = width;
        height_ = height;
    }

    void clear() {
        cached_texture_ = nil;
    }

    id<MTLTexture> get_mtl_texture() const {
        return cached_texture_;
    }

    void* get_native_handle() const override {
        return (__bridge void*)cached_texture_;
    }

    TextureType get_type() const override { return TextureType::Texture2D; }
    TextureFormat get_format() const override { return TextureFormat::BGRA8Unorm; }
    uint32_t get_width() const override { return width_; }
    uint32_t get_height() const override { return height_; }
    uint32_t get_depth() const override { return 1; }
    uint32_t get_mip_levels() const override { return 1; }
    uint32_t get_array_layers() const override { return 1; }
    TextureUsage get_usage() const override { return TextureUsage::RenderTarget; }

private:
    id<MTLTexture> cached_texture_ = nil;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
};

// ============================================================================
// MetalSwapChain Implementation Details
// ============================================================================

MetalSwapChain::MetalSwapChain(MetalDevice* /*device*/, CAMetalLayer* layer)
    : layer_(layer) {
    current_texture_ = std::make_unique<DrawableTexture>();

    CGSize size = layer_.drawableSize;
    width_ = static_cast<uint32_t>(size.width);
    height_ = static_cast<uint32_t>(size.height);
    format_ = metal::from_mtl_pixel_format(layer_.pixelFormat);

    spdlog::debug("MetalSwapChain created: {}x{}", width_, height_);
}

Texture* MetalSwapChain::get_current_texture() {
    return current_texture_.get();
}

void MetalSwapChain::resize(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;
    layer_.drawableSize = CGSizeMake(width, height);
    spdlog::debug("MetalSwapChain resized: {}x{}", width_, height_);
}

void MetalSwapChain::set_current_texture(id<MTLTexture> texture) {
    current_texture_->set_current_texture(texture, width_, height_);
    frame_index_ = (frame_index_ + 1) % 3;
}

void MetalSwapChain::clear_current_texture() {
    current_texture_->clear();
}

// ============================================================================
// MetalDevice Implementation Details
// ============================================================================

MetalDevice::~MetalDevice() {
    @autoreleasepool {
        wait_idle();
        swap_chain_.reset();
        command_queue_ = nil;
        device_ = nil;
    }
}

bool MetalDevice::initialize(const DeviceDesc& desc) {
    @autoreleasepool {
        // Get Metal layer from platform layer
        metal_layer_ = (__bridge CAMetalLayer*)desc.metal_layer;
        if (!metal_layer_) {
            spdlog::error("MetalDevice: No Metal layer provided");
            return false;
        }

        // Get device from layer (platform layer already created it)
        device_ = metal_layer_.device;
        if (!device_) {
            device_ = MTLCreateSystemDefaultDevice();
            if (!device_) {
                spdlog::error("MetalDevice: Failed to create Metal device");
                return false;
            }
            metal_layer_.device = device_;
        }

        // Create command queue
        command_queue_ = [device_ newCommandQueue];
        if (!command_queue_) {
            spdlog::error("MetalDevice: Failed to create command queue");
            return false;
        }

        // Configure vsync
        vsync_ = desc.vsync;
        metal_layer_.displaySyncEnabled = vsync_;

        // Create swap chain
        swap_chain_ = std::make_unique<MetalSwapChain>(this, metal_layer_);

        spdlog::info("MetalDevice initialized: {}", [[device_ name] UTF8String]);
        return true;
    }
}

void MetalDevice::begin_frame() {
    // Note: No @autoreleasepool here - drawable must persist until end_frame
    if (in_frame_) {
        spdlog::warn("MetalDevice::begin_frame called while already in frame");
        return;
    }

    // Acquire next drawable
    id<CAMetalDrawable> drawable = [metal_layer_ nextDrawable];
    if (!drawable) {
        spdlog::warn("MetalDevice: Failed to acquire drawable");
        return;
    }

    // Store the drawable and cache its texture immediately
    // The texture is captured here while the drawable is definitely valid
    current_drawable_ = drawable;
    id<MTLTexture> texture = drawable.texture;
    swap_chain_->set_current_texture(texture);
    in_frame_ = true;
}

void MetalDevice::end_frame() {
    @autoreleasepool {
        if (!in_frame_) {
            spdlog::warn("MetalDevice::end_frame called while not in frame");
            return;
        }

        swap_chain_->clear_current_texture();
        current_drawable_ = nil;
        in_frame_ = false;
    }
}

void MetalDevice::wait_idle() {
    @autoreleasepool {
        // Create a temporary command buffer and wait for it to complete
        // This ensures all previous work is done
        id<MTLCommandBuffer> cmd = [command_queue_ commandBuffer];
        [cmd commit];
        [cmd waitUntilCompleted];
    }
}

SwapChain* MetalDevice::get_swap_chain() {
    return swap_chain_.get();
}

void MetalDevice::resize_swap_chain(uint32_t width, uint32_t height) {
    swap_chain_->resize(width, height);
}

DeviceCapabilities MetalDevice::get_capabilities() const {
    DeviceCapabilities caps;

    @autoreleasepool {
        caps.device_name = [[device_ name] UTF8String];
        caps.api_name = "Metal";
        caps.dedicated_video_memory = 0;  // Not easily available on Metal
        caps.max_buffer_size = [device_ maxBufferLength];
        caps.max_texture_size_2d = 16384;
        caps.max_texture_size_3d = 2048;
        caps.max_texture_array_layers = 2048;
        caps.max_uniform_buffer_size = 65536;
        caps.max_storage_buffer_size = [device_ maxBufferLength];
        caps.max_compute_work_group_count[0] = UINT32_MAX;
        caps.max_compute_work_group_count[1] = UINT32_MAX;
        caps.max_compute_work_group_count[2] = UINT32_MAX;
        caps.max_compute_work_group_size[0] = 1024;
        caps.max_compute_work_group_size[1] = 1024;
        caps.max_compute_work_group_size[2] = 1024;
        caps.max_compute_work_group_invocations = 1024;
        caps.supports_ray_tracing = [device_ supportsRaytracing];
        caps.supports_compute = true;
        caps.supports_timestamp_query = true;
    }

    return caps;
}

// ============================================================================
// Resource Creation Implementations
// (Factory functions are declared in metal_impl.hpp)
// ============================================================================

std::unique_ptr<Buffer> MetalDevice::create_buffer(const BufferDesc& desc) {
    return create_metal_buffer(device_, desc);
}

std::unique_ptr<Texture> MetalDevice::create_texture(const TextureDesc& desc) {
    return create_metal_texture(device_, desc);
}

std::unique_ptr<Sampler> MetalDevice::create_sampler(const SamplerDesc& desc) {
    return create_metal_sampler(device_, desc);
}

std::unique_ptr<Shader> MetalDevice::create_shader(const ShaderDesc& desc) {
    return create_metal_shader(device_, desc);
}

std::unique_ptr<Pipeline> MetalDevice::create_pipeline(const PipelineDesc& desc) {
    return create_metal_pipeline(device_, desc);
}

std::unique_ptr<ComputePipeline> MetalDevice::create_compute_pipeline(const ComputePipelineDesc& desc) {
    return create_metal_compute_pipeline(device_, desc);
}

std::unique_ptr<RenderPass> MetalDevice::create_render_pass(const RenderPassDesc& desc) {
    return create_metal_render_pass(desc);
}

std::unique_ptr<Framebuffer> MetalDevice::create_framebuffer(const FramebufferDesc& desc) {
    return create_metal_framebuffer(desc);
}

std::unique_ptr<CommandBuffer> MetalDevice::create_command_buffer() {
    return create_metal_command_buffer(command_queue_);
}

void MetalDevice::submit(CommandBuffer* cmd, bool wait_for_completion) {
    @autoreleasepool {
        auto* metal_cmd = static_cast<MetalCommandBuffer*>(cmd);
        id<MTLCommandBuffer> buffer = metal_cmd->get_mtl_command_buffer();

        if (current_drawable_ && in_frame_) {
            [buffer presentDrawable:current_drawable_];
        }

        [buffer commit];

        if (wait_for_completion) {
            [buffer waitUntilCompleted];
        }
    }
}

// ============================================================================
// Factory Function
// ============================================================================

std::unique_ptr<GraphicsDevice> create_metal_device(const DeviceDesc& desc) {
    auto device = std::make_unique<MetalDevice>();
    if (!device->initialize(desc)) {
        return nullptr;
    }
    return device;
}

}  // namespace realcraft::graphics
