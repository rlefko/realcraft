// RealCraft Graphics Abstraction Layer
// command_buffer.hpp - Command buffer interface

#pragma once

#include "types.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace realcraft::graphics {

// Forward declarations
class Buffer;
class ComputePipeline;
class Framebuffer;
class Pipeline;
class RenderPass;
class Sampler;
class Texture;

// Abstract command buffer class
// Records GPU commands for later submission
// Implementations: MetalCommandBuffer, VulkanCommandBuffer
class CommandBuffer {
public:
    virtual ~CommandBuffer() = default;

    // Non-copyable
    CommandBuffer(const CommandBuffer&) = delete;
    CommandBuffer& operator=(const CommandBuffer&) = delete;

    // ========================================================================
    // Recording Lifecycle
    // ========================================================================

    virtual void begin() = 0;
    virtual void end() = 0;

    // ========================================================================
    // Render Pass Scope
    // ========================================================================

    // Begin a render pass with specified framebuffer and clear values
    virtual void begin_render_pass(const RenderPassDesc& desc, Texture* color_attachment, Texture* depth_attachment,
                                   const ClearValue& color_clear, const ClearValue& depth_clear) = 0;

    virtual void end_render_pass() = 0;

    // ========================================================================
    // Pipeline Binding
    // ========================================================================

    virtual void bind_pipeline(const Pipeline* pipeline) = 0;
    virtual void bind_compute_pipeline(const ComputePipeline* pipeline) = 0;

    // ========================================================================
    // Resource Binding
    // ========================================================================

    virtual void bind_vertex_buffer(uint32_t slot, const Buffer* buffer, size_t offset = 0) = 0;
    virtual void bind_index_buffer(const Buffer* buffer, IndexType type, size_t offset = 0) = 0;

    // Bind uniform buffer to a binding slot
    virtual void bind_uniform_buffer(uint32_t binding, const Buffer* buffer, size_t offset = 0, size_t size = 0) = 0;

    // Bind texture with sampler to a binding slot
    virtual void bind_texture(uint32_t binding, const Texture* texture, const Sampler* sampler) = 0;

    // Bind storage buffer to a binding slot
    virtual void bind_storage_buffer(uint32_t binding, const Buffer* buffer, size_t offset = 0, size_t size = 0) = 0;

    // ========================================================================
    // Dynamic State
    // ========================================================================

    virtual void set_viewport(const Viewport& viewport) = 0;
    virtual void set_scissor(const Rect& scissor) = 0;
    virtual void set_blend_constant(float r, float g, float b, float a) = 0;

    // Push constants (small, fast uniform data)
    virtual void push_constants(ShaderStage stage, uint32_t offset, uint32_t size, const void* data) = 0;

    // ========================================================================
    // Draw Commands
    // ========================================================================

    virtual void draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0,
                      uint32_t first_instance = 0) = 0;

    virtual void draw_indexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t first_index = 0,
                              int32_t vertex_offset = 0, uint32_t first_instance = 0) = 0;

    // ========================================================================
    // Compute Commands
    // ========================================================================

    virtual void begin_compute_pass() = 0;
    virtual void end_compute_pass() = 0;

    virtual void dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) = 0;

    // ========================================================================
    // Transfer Commands
    // ========================================================================

    virtual void copy_buffer(const Buffer* src, Buffer* dst, size_t src_offset, size_t dst_offset, size_t size) = 0;

    virtual void copy_buffer_to_texture(const Buffer* src, Texture* dst, const BufferImageCopy& region) = 0;

    virtual void copy_texture_to_buffer(const Texture* src, Buffer* dst, const BufferImageCopy& region) = 0;

protected:
    CommandBuffer() = default;
};

}  // namespace realcraft::graphics
