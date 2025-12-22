// RealCraft Graphics Abstraction Layer
// metal_command_buffer.mm - Metal command buffer implementation

#import <Metal/Metal.h>

#include "metal_impl.hpp"
#include "metal_types.hpp"

#include <spdlog/spdlog.h>

namespace realcraft::graphics {

// Helper functions to get Metal objects
inline id<MTLBuffer> get_mtl_buffer(const Buffer* buffer) {
    return buffer ? static_cast<const MetalBuffer*>(buffer)->get_mtl_buffer() : nil;
}

inline id<MTLTexture> get_mtl_texture(const Texture* texture) {
    if (!texture) return nil;
    // Try to cast to MetalTexture first for the fast path
    if (auto* metal_texture = dynamic_cast<const MetalTexture*>(texture)) {
        return metal_texture->get_mtl_texture();
    }
    // Fall back to native handle for other texture types (e.g., DrawableTexture)
    void* handle = texture->get_native_handle();
    if (!handle) {
        spdlog::warn("get_mtl_texture: texture has null native handle");
        return nil;
    }
    // The handle is a retained MTLTexture*, safe to bridge
    return (__bridge id<MTLTexture>)handle;
}

inline id<MTLSamplerState> get_mtl_sampler(const Sampler* sampler) {
    return sampler ? static_cast<const MetalSampler*>(sampler)->get_mtl_sampler() : nil;
}

// ============================================================================
// MetalCommandBuffer Implementation
// ============================================================================

MetalCommandBuffer::MetalCommandBuffer(id<MTLCommandQueue> queue) : queue_(queue) {}

MetalCommandBuffer::~MetalCommandBuffer() {
    @autoreleasepool {
        render_encoder_ = nil;
        compute_encoder_ = nil;
        blit_encoder_ = nil;
        command_buffer_ = nil;
    }
}

void MetalCommandBuffer::begin() {
    // Note: No @autoreleasepool - command buffer must persist until end()/submit()
    command_buffer_ = [queue_ commandBuffer];
}

void MetalCommandBuffer::end() {
    // End any active blit encoder
    @autoreleasepool {
        if (blit_encoder_) {
            [blit_encoder_ endEncoding];
            blit_encoder_ = nil;
        }
    }
}

void MetalCommandBuffer::begin_render_pass(const RenderPassDesc& desc, Texture* color_attachment,
                                            Texture* depth_attachment, const ClearValue& color_clear,
                                            const ClearValue& depth_clear) {
    // Note: No @autoreleasepool - render_encoder_ must persist until end_render_pass()
    MTLRenderPassDescriptor* pass_desc = [MTLRenderPassDescriptor renderPassDescriptor];

    // Configure color attachment
    if (color_attachment) {
        id<MTLTexture> mtl_texture = get_mtl_texture(color_attachment);
        if (!mtl_texture) {
            spdlog::error("begin_render_pass: Failed to get MTLTexture from color_attachment");
            return;
        }
        pass_desc.colorAttachments[0].texture = mtl_texture;

        if (!desc.color_attachments.empty()) {
            pass_desc.colorAttachments[0].loadAction =
                metal::to_mtl_load_action(desc.color_attachments[0].load_op);
            pass_desc.colorAttachments[0].storeAction =
                metal::to_mtl_store_action(desc.color_attachments[0].store_op);
        } else {
            pass_desc.colorAttachments[0].loadAction = MTLLoadActionClear;
            pass_desc.colorAttachments[0].storeAction = MTLStoreActionStore;
        }

        pass_desc.colorAttachments[0].clearColor = MTLClearColorMake(
            color_clear.color.r, color_clear.color.g,
            color_clear.color.b, color_clear.color.a);
    }

    // Configure depth attachment
    if (depth_attachment) {
        id<MTLTexture> mtl_depth = get_mtl_texture(depth_attachment);
        pass_desc.depthAttachment.texture = mtl_depth;

        if (desc.depth_attachment) {
            pass_desc.depthAttachment.loadAction =
                metal::to_mtl_load_action(desc.depth_attachment->load_op);
            pass_desc.depthAttachment.storeAction =
                metal::to_mtl_store_action(desc.depth_attachment->store_op);
        } else {
            pass_desc.depthAttachment.loadAction = MTLLoadActionClear;
            pass_desc.depthAttachment.storeAction = MTLStoreActionDontCare;
        }

        pass_desc.depthAttachment.clearDepth = depth_clear.depth_stencil.depth;
    }

    render_encoder_ = [command_buffer_ renderCommandEncoderWithDescriptor:pass_desc];
}

void MetalCommandBuffer::end_render_pass() {
    if (render_encoder_) {
        [render_encoder_ endEncoding];
        render_encoder_ = nil;
    }
    current_pipeline_ = nullptr;
}

void MetalCommandBuffer::bind_pipeline(const Pipeline* pipeline) {
    if (!render_encoder_ || !pipeline) return;

    @autoreleasepool {
        current_pipeline_ = static_cast<const MetalPipeline*>(pipeline);
        [render_encoder_ setRenderPipelineState:current_pipeline_->get_pipeline_state()];
        [render_encoder_ setDepthStencilState:current_pipeline_->get_depth_stencil_state()];
        [render_encoder_ setCullMode:current_pipeline_->get_mtl_cull_mode()];
        [render_encoder_ setFrontFacingWinding:current_pipeline_->get_mtl_winding()];

        if (current_pipeline_->get_wireframe()) {
            [render_encoder_ setTriangleFillMode:MTLTriangleFillModeLines];
        } else {
            [render_encoder_ setTriangleFillMode:MTLTriangleFillModeFill];
        }
    }
}

void MetalCommandBuffer::bind_compute_pipeline(const ComputePipeline* pipeline) {
    if (!compute_encoder_ || !pipeline) return;

    @autoreleasepool {
        current_compute_pipeline_ = static_cast<const MetalComputePipeline*>(pipeline);
        [compute_encoder_ setComputePipelineState:current_compute_pipeline_->get_pipeline_state()];
    }
}

void MetalCommandBuffer::bind_vertex_buffer(uint32_t slot, const Buffer* buffer, size_t offset) {
    if (!render_encoder_ || !buffer) return;

    @autoreleasepool {
        [render_encoder_ setVertexBuffer:get_mtl_buffer(buffer) offset:offset atIndex:slot];
    }
}

void MetalCommandBuffer::bind_index_buffer(const Buffer* buffer, IndexType type, size_t offset) {
    if (!buffer) return;

    bound_index_buffer_ = get_mtl_buffer(buffer);
    bound_index_type_ = metal::to_mtl_index_type(type);
    bound_index_offset_ = offset;
}

void MetalCommandBuffer::bind_uniform_buffer(uint32_t binding, const Buffer* buffer,
                                              size_t offset, size_t /*size*/) {
    if (!buffer) return;

    // Uniform buffers are remapped to indices 10+ to avoid collision with vertex buffers
    // This must match the SPIRV-Cross MSL resource binding remapping in shader_compiler.cpp
    constexpr uint32_t UNIFORM_BUFFER_BASE_INDEX = 10;
    uint32_t metal_index = UNIFORM_BUFFER_BASE_INDEX + binding;

    @autoreleasepool {
        id<MTLBuffer> mtl_buffer = get_mtl_buffer(buffer);
        if (render_encoder_) {
            [render_encoder_ setVertexBuffer:mtl_buffer offset:offset atIndex:metal_index];
            [render_encoder_ setFragmentBuffer:mtl_buffer offset:offset atIndex:metal_index];
        }
        if (compute_encoder_) {
            [compute_encoder_ setBuffer:mtl_buffer offset:offset atIndex:metal_index];
        }
    }
}

void MetalCommandBuffer::bind_texture(uint32_t binding, const Texture* texture,
                                       const Sampler* sampler) {
    @autoreleasepool {
        id<MTLTexture> mtl_texture = get_mtl_texture(texture);
        id<MTLSamplerState> mtl_sampler = get_mtl_sampler(sampler);

        if (render_encoder_) {
            [render_encoder_ setFragmentTexture:mtl_texture atIndex:binding];
            if (mtl_sampler) {
                [render_encoder_ setFragmentSamplerState:mtl_sampler atIndex:binding];
            }
        }
        if (compute_encoder_) {
            [compute_encoder_ setTexture:mtl_texture atIndex:binding];
            if (mtl_sampler) {
                [compute_encoder_ setSamplerState:mtl_sampler atIndex:binding];
            }
        }
    }
}

void MetalCommandBuffer::bind_storage_buffer(uint32_t binding, const Buffer* buffer,
                                              size_t offset, size_t /*size*/) {
    if (!buffer) return;

    @autoreleasepool {
        id<MTLBuffer> mtl_buffer = get_mtl_buffer(buffer);
        if (render_encoder_) {
            [render_encoder_ setFragmentBuffer:mtl_buffer offset:offset atIndex:binding];
        }
        if (compute_encoder_) {
            [compute_encoder_ setBuffer:mtl_buffer offset:offset atIndex:binding];
        }
    }
}

void MetalCommandBuffer::set_viewport(const Viewport& viewport) {
    if (!render_encoder_) return;

    @autoreleasepool {
        MTLViewport mtl_viewport;
        mtl_viewport.originX = viewport.x;
        mtl_viewport.originY = viewport.y;
        mtl_viewport.width = viewport.width;
        mtl_viewport.height = viewport.height;
        mtl_viewport.znear = viewport.min_depth;
        mtl_viewport.zfar = viewport.max_depth;
        [render_encoder_ setViewport:mtl_viewport];
    }
}

void MetalCommandBuffer::set_scissor(const Rect& scissor) {
    if (!render_encoder_) return;

    @autoreleasepool {
        MTLScissorRect mtl_scissor;
        mtl_scissor.x = scissor.x;
        mtl_scissor.y = scissor.y;
        mtl_scissor.width = scissor.width;
        mtl_scissor.height = scissor.height;
        [render_encoder_ setScissorRect:mtl_scissor];
    }
}

void MetalCommandBuffer::set_blend_constant(float r, float g, float b, float a) {
    if (!render_encoder_) return;

    @autoreleasepool {
        [render_encoder_ setBlendColorRed:r green:g blue:b alpha:a];
    }
}

void MetalCommandBuffer::push_constants(ShaderStage stage, uint32_t /*offset*/, uint32_t size,
                                         const void* data) {
    if (!render_encoder_ || !data) return;

    @autoreleasepool {
        // Metal uses setVertexBytes/setFragmentBytes for small constant data
        // Using buffer index 30 as convention for push constants
        const uint32_t push_constant_index = 30;

        if (stage == ShaderStage::Vertex) {
            [render_encoder_ setVertexBytes:data length:size atIndex:push_constant_index];
        } else if (stage == ShaderStage::Fragment) {
            [render_encoder_ setFragmentBytes:data length:size atIndex:push_constant_index];
        }
    }
}

void MetalCommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count,
                               uint32_t first_vertex, uint32_t first_instance) {
    if (!render_encoder_ || !current_pipeline_) return;

    @autoreleasepool {
        [render_encoder_ drawPrimitives:current_pipeline_->get_mtl_primitive_type()
                            vertexStart:first_vertex
                            vertexCount:vertex_count
                          instanceCount:instance_count
                           baseInstance:first_instance];
    }
}

void MetalCommandBuffer::draw_indexed(uint32_t index_count, uint32_t instance_count,
                                       uint32_t first_index, int32_t vertex_offset,
                                       uint32_t first_instance) {
    if (!render_encoder_ || !current_pipeline_ || !bound_index_buffer_) return;

    @autoreleasepool {
        size_t index_size = (bound_index_type_ == MTLIndexTypeUInt16) ? 2 : 4;
        size_t offset = bound_index_offset_ + first_index * index_size;

        [render_encoder_
            drawIndexedPrimitives:current_pipeline_->get_mtl_primitive_type()
                       indexCount:index_count
                        indexType:bound_index_type_
                      indexBuffer:bound_index_buffer_
                indexBufferOffset:offset
                    instanceCount:instance_count
                       baseVertex:vertex_offset
                     baseInstance:first_instance];
    }
}

void MetalCommandBuffer::begin_compute_pass() {
    // No @autoreleasepool - encoder must persist until end_compute_pass()
    compute_encoder_ = [command_buffer_ computeCommandEncoder];
}

void MetalCommandBuffer::end_compute_pass() {
    if (compute_encoder_) {
        [compute_encoder_ endEncoding];
        compute_encoder_ = nil;
    }
    current_compute_pipeline_ = nullptr;
}

void MetalCommandBuffer::dispatch(uint32_t group_count_x, uint32_t group_count_y,
                                   uint32_t group_count_z) {
    if (!compute_encoder_ || !current_compute_pipeline_) return;

    @autoreleasepool {
        MTLSize threadgroups = MTLSizeMake(group_count_x, group_count_y, group_count_z);

        // Get thread execution width from pipeline
        NSUInteger thread_width = current_compute_pipeline_->get_pipeline_state()
                                      .threadExecutionWidth;
        NSUInteger max_threads = current_compute_pipeline_->get_pipeline_state()
                                     .maxTotalThreadsPerThreadgroup;

        // Simple default: use thread execution width
        MTLSize threads_per_group = MTLSizeMake(thread_width, 1, 1);
        if (max_threads >= 256) {
            threads_per_group = MTLSizeMake(16, 16, 1);
        }

        [compute_encoder_ dispatchThreadgroups:threadgroups
                         threadsPerThreadgroup:threads_per_group];
    }
}

void MetalCommandBuffer::copy_buffer(const Buffer* src, Buffer* dst, size_t src_offset,
                                      size_t dst_offset, size_t size) {
    if (!src || !dst) return;

    // No @autoreleasepool - blit_encoder_ must persist until end()
    if (!blit_encoder_) {
        blit_encoder_ = [command_buffer_ blitCommandEncoder];
    }

    [blit_encoder_ copyFromBuffer:get_mtl_buffer(src)
                     sourceOffset:src_offset
                         toBuffer:get_mtl_buffer(dst)
                destinationOffset:dst_offset
                             size:size];
}

void MetalCommandBuffer::copy_buffer_to_texture(const Buffer* src, Texture* dst,
                                                 const BufferImageCopy& region) {
    if (!src || !dst) return;

    // No @autoreleasepool - blit_encoder_ must persist until end()
    if (!blit_encoder_) {
        blit_encoder_ = [command_buffer_ blitCommandEncoder];
    }

    MTLSize size = MTLSizeMake(region.texture_width, region.texture_height, region.texture_depth);
    MTLOrigin origin = MTLOriginMake(region.texture_offset_x, region.texture_offset_y,
                                      region.texture_offset_z);

    uint32_t bytes_per_row = region.buffer_row_length > 0
                                 ? region.buffer_row_length
                                 : region.texture_width * 4;  // Assume 4 bytes per pixel
    uint32_t bytes_per_image = region.buffer_image_height > 0
                                   ? bytes_per_row * region.buffer_image_height
                                   : bytes_per_row * region.texture_height;

    [blit_encoder_ copyFromBuffer:get_mtl_buffer(src)
                     sourceOffset:region.buffer_offset
                sourceBytesPerRow:bytes_per_row
              sourceBytesPerImage:bytes_per_image
                       sourceSize:size
                        toTexture:get_mtl_texture(dst)
                 destinationSlice:region.array_layer
                 destinationLevel:region.mip_level
                destinationOrigin:origin];
}

void MetalCommandBuffer::copy_texture_to_buffer(const Texture* src, Buffer* dst,
                                                 const BufferImageCopy& region) {
    if (!src || !dst) return;

    // No @autoreleasepool - blit_encoder_ must persist until end()
    if (!blit_encoder_) {
        blit_encoder_ = [command_buffer_ blitCommandEncoder];
    }

    MTLSize size = MTLSizeMake(region.texture_width, region.texture_height, region.texture_depth);
    MTLOrigin origin = MTLOriginMake(region.texture_offset_x, region.texture_offset_y,
                                      region.texture_offset_z);

    uint32_t bytes_per_row = region.buffer_row_length > 0
                                 ? region.buffer_row_length
                                 : region.texture_width * 4;
    uint32_t bytes_per_image = region.buffer_image_height > 0
                                   ? bytes_per_row * region.buffer_image_height
                                   : bytes_per_row * region.texture_height;

    [blit_encoder_ copyFromTexture:get_mtl_texture(src)
                       sourceSlice:region.array_layer
                       sourceLevel:region.mip_level
                      sourceOrigin:origin
                        sourceSize:size
                          toBuffer:get_mtl_buffer(dst)
                 destinationOffset:region.buffer_offset
            destinationBytesPerRow:bytes_per_row
          destinationBytesPerImage:bytes_per_image];
}

id<MTLCommandBuffer> MetalCommandBuffer::get_mtl_command_buffer() const {
    return command_buffer_;
}

// ============================================================================
// Factory Function
// ============================================================================

std::unique_ptr<CommandBuffer> create_metal_command_buffer(id<MTLCommandQueue> queue) {
    return std::make_unique<MetalCommandBuffer>(queue);
}

}  // namespace realcraft::graphics
