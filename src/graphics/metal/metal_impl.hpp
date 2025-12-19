// RealCraft Graphics Abstraction Layer
// metal_impl.hpp - Shared Metal implementation declarations for cross-file access
//
// This header provides the minimal declarations needed for Metal implementation
// files to access each other's classes via static_cast.

#pragma once

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <realcraft/graphics/buffer.hpp>
#include <realcraft/graphics/command_buffer.hpp>
#include <realcraft/graphics/device.hpp>
#include <realcraft/graphics/pipeline.hpp>
#include <realcraft/graphics/render_pass.hpp>
#include <realcraft/graphics/sampler.hpp>
#include <realcraft/graphics/shader.hpp>
#include <realcraft/graphics/swap_chain.hpp>
#include <realcraft/graphics/texture.hpp>

namespace realcraft::graphics {

// ============================================================================
// MetalBuffer - Defined in metal_buffer.mm
// ============================================================================

class MetalBuffer : public Buffer {
public:
    MetalBuffer(id<MTLDevice> device, const BufferDesc& desc);
    ~MetalBuffer() override;

    void* map() override;
    void unmap() override;
    void write(const void* data, size_t size, size_t offset) override;
    void read(void* data, size_t size, size_t offset) const override;

    size_t get_size() const override;
    BufferUsage get_usage() const override;
    bool is_host_visible() const override;

    id<MTLBuffer> get_mtl_buffer() const;

private:
    id<MTLBuffer> buffer_ = nil;
    size_t size_ = 0;
    BufferUsage usage_ = BufferUsage::None;
    bool host_visible_ = false;
    bool mapped_ = false;
};

// ============================================================================
// MetalTexture - Defined in metal_texture.mm
// ============================================================================

class MetalTexture : public Texture {
public:
    MetalTexture(id<MTLDevice> device, const TextureDesc& desc);
    // Constructor for wrapping existing MTLTexture
    explicit MetalTexture(id<MTLTexture> texture, bool owns = false);
    ~MetalTexture() override;

    uint32_t get_width() const override;
    uint32_t get_height() const override;
    uint32_t get_depth() const override;
    uint32_t get_mip_levels() const override;
    uint32_t get_array_layers() const override;
    TextureFormat get_format() const override;
    TextureType get_type() const override;
    TextureUsage get_usage() const override;
    void* get_native_handle() const override;

    id<MTLTexture> get_mtl_texture() const;

private:
    id<MTLTexture> texture_ = nil;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    uint32_t depth_ = 1;
    uint32_t mip_levels_ = 1;
    uint32_t array_layers_ = 1;
    TextureFormat format_ = TextureFormat::RGBA8Unorm;
    TextureType type_ = TextureType::Texture2D;
    TextureUsage usage_ = TextureUsage::Sampled;
    bool owns_texture_ = true;
};

// ============================================================================
// MetalSampler - Defined in metal_sampler.mm
// ============================================================================

class MetalSampler : public Sampler {
public:
    MetalSampler(id<MTLDevice> device, const SamplerDesc& desc);
    ~MetalSampler() override;

    FilterMode get_min_filter() const override;
    FilterMode get_mag_filter() const override;
    AddressMode get_address_mode_u() const override;
    AddressMode get_address_mode_v() const override;
    AddressMode get_address_mode_w() const override;
    float get_max_anisotropy() const override;

    id<MTLSamplerState> get_mtl_sampler() const;

private:
    id<MTLSamplerState> sampler_ = nil;
    FilterMode min_filter_ = FilterMode::Linear;
    FilterMode mag_filter_ = FilterMode::Linear;
    FilterMode mip_filter_ = FilterMode::Linear;
    AddressMode address_u_ = AddressMode::Repeat;
    AddressMode address_v_ = AddressMode::Repeat;
    AddressMode address_w_ = AddressMode::Repeat;
    float max_anisotropy_ = 1.0f;
};

// ============================================================================
// MetalShader - Defined in metal_shader.mm
// ============================================================================

class MetalShader : public Shader {
public:
    MetalShader(id<MTLDevice> device, const ShaderDesc& desc);
    ~MetalShader() override;

    ShaderStage get_stage() const override;
    const std::string& get_entry_point() const override;
    const ShaderReflection* get_reflection() const override;

    id<MTLFunction> get_mtl_function() const;

private:
    id<MTLLibrary> library_ = nil;
    id<MTLFunction> function_ = nil;
    ShaderStage stage_ = ShaderStage::Vertex;
    std::string entry_point_;
    std::optional<ShaderReflection> reflection_;
};

// ============================================================================
// MetalPipeline - Defined in metal_pipeline.mm
// ============================================================================

class MetalPipeline : public Pipeline {
public:
    MetalPipeline(id<MTLDevice> device, const PipelineDesc& desc);
    ~MetalPipeline() override;

    PrimitiveTopology get_topology() const override;

    id<MTLRenderPipelineState> get_pipeline_state() const;
    id<MTLDepthStencilState> get_depth_stencil_state() const;
    MTLPrimitiveType get_mtl_primitive_type() const;
    MTLCullMode get_mtl_cull_mode() const;
    MTLWinding get_mtl_winding() const;
    bool get_wireframe() const;

private:
    id<MTLRenderPipelineState> pipeline_state_ = nil;
    id<MTLDepthStencilState> depth_stencil_state_ = nil;
    PrimitiveTopology topology_ = PrimitiveTopology::TriangleList;
    MTLPrimitiveType primitive_type_ = MTLPrimitiveTypeTriangle;
    MTLCullMode cull_mode_ = MTLCullModeBack;
    MTLWinding winding_ = MTLWindingCounterClockwise;
    bool wireframe_ = false;
};

// ============================================================================
// MetalComputePipeline - Defined in metal_pipeline.mm
// ============================================================================

class MetalComputePipeline : public ComputePipeline {
public:
    MetalComputePipeline(id<MTLDevice> device, const ComputePipelineDesc& desc);
    ~MetalComputePipeline() override;

    id<MTLComputePipelineState> get_pipeline_state() const;

private:
    id<MTLComputePipelineState> pipeline_state_ = nil;
};

// ============================================================================
// MetalCommandBuffer - Defined in metal_command_buffer.mm
// ============================================================================

class MetalCommandBuffer : public CommandBuffer {
public:
    explicit MetalCommandBuffer(id<MTLCommandQueue> queue);
    ~MetalCommandBuffer() override;

    void begin() override;
    void end() override;

    void begin_render_pass(const RenderPassDesc& desc, Texture* color_attachment, Texture* depth_attachment,
                           const ClearValue& color_clear, const ClearValue& depth_clear) override;
    void end_render_pass() override;

    void bind_pipeline(const Pipeline* pipeline) override;
    void bind_compute_pipeline(const ComputePipeline* pipeline) override;

    void bind_vertex_buffer(uint32_t slot, const Buffer* buffer, size_t offset) override;
    void bind_index_buffer(const Buffer* buffer, IndexType type, size_t offset) override;
    void bind_uniform_buffer(uint32_t binding, const Buffer* buffer, size_t offset, size_t size) override;
    void bind_texture(uint32_t binding, const Texture* texture, const Sampler* sampler) override;
    void bind_storage_buffer(uint32_t binding, const Buffer* buffer, size_t offset, size_t size) override;

    void set_viewport(const Viewport& viewport) override;
    void set_scissor(const Rect& scissor) override;
    void set_blend_constant(float r, float g, float b, float a) override;
    void push_constants(ShaderStage stage, uint32_t offset, uint32_t size, const void* data) override;

    void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) override;
    void draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset,
                      uint32_t first_instance) override;

    void begin_compute_pass() override;
    void end_compute_pass() override;
    void dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) override;

    void copy_buffer(const Buffer* src, Buffer* dst, size_t src_offset, size_t dst_offset, size_t size) override;
    void copy_buffer_to_texture(const Buffer* src, Texture* dst, const BufferImageCopy& region) override;
    void copy_texture_to_buffer(const Texture* src, Buffer* dst, const BufferImageCopy& region) override;

    id<MTLCommandBuffer> get_mtl_command_buffer() const;

private:
    id<MTLCommandQueue> queue_ = nil;
    id<MTLCommandBuffer> command_buffer_ = nil;
    id<MTLRenderCommandEncoder> render_encoder_ = nil;
    id<MTLComputeCommandEncoder> compute_encoder_ = nil;
    id<MTLBlitCommandEncoder> blit_encoder_ = nil;

    const MetalPipeline* current_pipeline_ = nullptr;
    const MetalComputePipeline* current_compute_pipeline_ = nullptr;
    id<MTLBuffer> bound_index_buffer_ = nil;
    MTLIndexType bound_index_type_ = MTLIndexTypeUInt32;
    size_t bound_index_offset_ = 0;
};

// ============================================================================
// Factory Functions (defined in respective .mm files)
// ============================================================================

std::unique_ptr<Buffer> create_metal_buffer(id<MTLDevice> device, const BufferDesc& desc);
std::unique_ptr<Texture> create_metal_texture(id<MTLDevice> device, const TextureDesc& desc);
std::unique_ptr<Sampler> create_metal_sampler(id<MTLDevice> device, const SamplerDesc& desc);
std::unique_ptr<Shader> create_metal_shader(id<MTLDevice> device, const ShaderDesc& desc);
std::unique_ptr<Pipeline> create_metal_pipeline(id<MTLDevice> device, const PipelineDesc& desc);
std::unique_ptr<ComputePipeline> create_metal_compute_pipeline(id<MTLDevice> device, const ComputePipelineDesc& desc);
std::unique_ptr<RenderPass> create_metal_render_pass(const RenderPassDesc& desc);
std::unique_ptr<Framebuffer> create_metal_framebuffer(const FramebufferDesc& desc);
std::unique_ptr<CommandBuffer> create_metal_command_buffer(id<MTLCommandQueue> queue);

}  // namespace realcraft::graphics
