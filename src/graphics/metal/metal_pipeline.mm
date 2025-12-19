// RealCraft Graphics Abstraction Layer
// metal_pipeline.mm - Metal pipeline implementation

#import <Metal/Metal.h>

#include "metal_impl.hpp"
#include "metal_types.hpp"

#include <spdlog/spdlog.h>

namespace realcraft::graphics {

// ============================================================================
// MetalPipeline Implementation
// ============================================================================

MetalPipeline::MetalPipeline(id<MTLDevice> device, const PipelineDesc& desc)
    : topology_(desc.topology)
    , primitive_type_(metal::to_mtl_primitive_type(desc.topology))
    , cull_mode_(metal::to_mtl_cull_mode(desc.rasterizer.cull_mode))
    , winding_(metal::to_mtl_winding(desc.rasterizer.front_face))
    , wireframe_(desc.rasterizer.wireframe) {
    @autoreleasepool {
        NSError* error = nil;

        // Create render pipeline descriptor
        MTLRenderPipelineDescriptor* pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];

        // Set label on descriptor (pipeline state label is readonly)
        if (!desc.debug_name.empty()) {
            pipeline_desc.label = [NSString stringWithUTF8String:desc.debug_name.c_str()];
        }

        // Set shaders
        if (desc.vertex_shader) {
            pipeline_desc.vertexFunction =
                static_cast<const MetalShader*>(desc.vertex_shader)->get_mtl_function();
        }
        if (desc.fragment_shader) {
            pipeline_desc.fragmentFunction =
                static_cast<const MetalShader*>(desc.fragment_shader)->get_mtl_function();
        }

        // Configure vertex descriptor
        if (!desc.vertex_attributes.empty()) {
            MTLVertexDescriptor* vertex_desc = [[MTLVertexDescriptor alloc] init];

            for (const auto& attr : desc.vertex_attributes) {
                vertex_desc.attributes[attr.location].format =
                    metal::to_mtl_vertex_format(attr.format);
                vertex_desc.attributes[attr.location].offset = attr.offset;
                vertex_desc.attributes[attr.location].bufferIndex = attr.binding;
            }

            for (const auto& binding : desc.vertex_bindings) {
                vertex_desc.layouts[binding.binding].stride = binding.stride;
                vertex_desc.layouts[binding.binding].stepRate = 1;
                vertex_desc.layouts[binding.binding].stepFunction =
                    binding.per_instance ? MTLVertexStepFunctionPerInstance
                                         : MTLVertexStepFunctionPerVertex;
            }

            pipeline_desc.vertexDescriptor = vertex_desc;
        }

        // Configure color attachments
        size_t color_count = desc.color_formats.size();
        if (color_count == 0 && !desc.color_blend.empty()) {
            color_count = desc.color_blend.size();
        }

        for (size_t i = 0; i < color_count; ++i) {
            MTLRenderPipelineColorAttachmentDescriptor* color_attachment =
                pipeline_desc.colorAttachments[i];

            // Set format
            if (i < desc.color_formats.size()) {
                color_attachment.pixelFormat = metal::to_mtl_pixel_format(desc.color_formats[i]);
            } else {
                color_attachment.pixelFormat = MTLPixelFormatBGRA8Unorm;
            }

            // Set blending
            if (i < desc.color_blend.size()) {
                const auto& blend = desc.color_blend[i];
                color_attachment.blendingEnabled = blend.enable;
                color_attachment.sourceRGBBlendFactor =
                    metal::to_mtl_blend_factor(blend.src_color);
                color_attachment.destinationRGBBlendFactor =
                    metal::to_mtl_blend_factor(blend.dst_color);
                color_attachment.rgbBlendOperation =
                    metal::to_mtl_blend_operation(blend.color_op);
                color_attachment.sourceAlphaBlendFactor =
                    metal::to_mtl_blend_factor(blend.src_alpha);
                color_attachment.destinationAlphaBlendFactor =
                    metal::to_mtl_blend_factor(blend.dst_alpha);
                color_attachment.alphaBlendOperation =
                    metal::to_mtl_blend_operation(blend.alpha_op);
                color_attachment.writeMask = blend.color_write_mask;
            }
        }

        // Set depth format
        if (desc.depth_format != TextureFormat::Unknown) {
            pipeline_desc.depthAttachmentPixelFormat =
                metal::to_mtl_pixel_format(desc.depth_format);
        }

        // Create pipeline state
        pipeline_state_ = [device newRenderPipelineStateWithDescriptor:pipeline_desc error:&error];

        if (!pipeline_state_) {
            spdlog::error("MetalPipeline: Failed to create pipeline state: {}",
                          error ? [[error localizedDescription] UTF8String] : "unknown error");
            return;
        }

        // Create depth stencil state
        MTLDepthStencilDescriptor* depth_desc = [[MTLDepthStencilDescriptor alloc] init];
        depth_desc.depthCompareFunction =
            desc.depth_stencil.depth_test_enable
                ? metal::to_mtl_compare_function(desc.depth_stencil.depth_compare)
                : MTLCompareFunctionAlways;
        depth_desc.depthWriteEnabled = desc.depth_stencil.depth_write_enable;

        depth_stencil_state_ = [device newDepthStencilStateWithDescriptor:depth_desc];

        spdlog::debug("MetalPipeline created");
    }
}

MetalPipeline::~MetalPipeline() {
    @autoreleasepool {
        pipeline_state_ = nil;
        depth_stencil_state_ = nil;
    }
}

PrimitiveTopology MetalPipeline::get_topology() const { return topology_; }
id<MTLRenderPipelineState> MetalPipeline::get_pipeline_state() const { return pipeline_state_; }
id<MTLDepthStencilState> MetalPipeline::get_depth_stencil_state() const { return depth_stencil_state_; }
MTLPrimitiveType MetalPipeline::get_mtl_primitive_type() const { return primitive_type_; }
MTLCullMode MetalPipeline::get_mtl_cull_mode() const { return cull_mode_; }
MTLWinding MetalPipeline::get_mtl_winding() const { return winding_; }
bool MetalPipeline::get_wireframe() const { return wireframe_; }

// ============================================================================
// MetalComputePipeline Implementation
// ============================================================================

MetalComputePipeline::MetalComputePipeline(id<MTLDevice> device, const ComputePipelineDesc& desc) {
    @autoreleasepool {
        NSError* error = nil;

        if (!desc.compute_shader) {
            spdlog::error("MetalComputePipeline: No compute shader provided");
            return;
        }

        id<MTLFunction> function =
            static_cast<const MetalShader*>(desc.compute_shader)->get_mtl_function();
        if (!function) {
            spdlog::error("MetalComputePipeline: Invalid compute shader");
            return;
        }

        pipeline_state_ = [device newComputePipelineStateWithFunction:function error:&error];

        if (!pipeline_state_) {
            spdlog::error("MetalComputePipeline: Failed to create pipeline state: {}",
                          error ? [[error localizedDescription] UTF8String] : "unknown error");
            return;
        }

        spdlog::debug("MetalComputePipeline created");
    }
}

MetalComputePipeline::~MetalComputePipeline() {
    @autoreleasepool {
        pipeline_state_ = nil;
    }
}

id<MTLComputePipelineState> MetalComputePipeline::get_pipeline_state() const {
    return pipeline_state_;
}

// ============================================================================
// Factory Functions
// ============================================================================

std::unique_ptr<Pipeline> create_metal_pipeline(id<MTLDevice> device, const PipelineDesc& desc) {
    return std::make_unique<MetalPipeline>(device, desc);
}

std::unique_ptr<ComputePipeline> create_metal_compute_pipeline(id<MTLDevice> device,
                                                                const ComputePipelineDesc& desc) {
    return std::make_unique<MetalComputePipeline>(device, desc);
}

}  // namespace realcraft::graphics
