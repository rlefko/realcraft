// RealCraft Graphics Abstraction Layer
// metal_types.hpp - Metal enum conversions and type utilities

#pragma once

#import <Metal/Metal.h>
#include <realcraft/graphics/types.hpp>

namespace realcraft::graphics::metal {

// ============================================================================
// Texture Format Conversions
// ============================================================================

inline MTLPixelFormat to_mtl_pixel_format(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8Unorm:
            return MTLPixelFormatR8Unorm;
        case TextureFormat::R8Snorm:
            return MTLPixelFormatR8Snorm;
        case TextureFormat::R8Uint:
            return MTLPixelFormatR8Uint;
        case TextureFormat::R8Sint:
            return MTLPixelFormatR8Sint;
        case TextureFormat::RG8Unorm:
            return MTLPixelFormatRG8Unorm;
        case TextureFormat::RG8Snorm:
            return MTLPixelFormatRG8Snorm;
        case TextureFormat::RG8Uint:
            return MTLPixelFormatRG8Uint;
        case TextureFormat::RG8Sint:
            return MTLPixelFormatRG8Sint;
        case TextureFormat::RGBA8Unorm:
            return MTLPixelFormatRGBA8Unorm;
        case TextureFormat::RGBA8Snorm:
            return MTLPixelFormatRGBA8Snorm;
        case TextureFormat::RGBA8Uint:
            return MTLPixelFormatRGBA8Uint;
        case TextureFormat::RGBA8Sint:
            return MTLPixelFormatRGBA8Sint;
        case TextureFormat::RGBA8UnormSrgb:
            return MTLPixelFormatRGBA8Unorm_sRGB;
        case TextureFormat::BGRA8Unorm:
            return MTLPixelFormatBGRA8Unorm;
        case TextureFormat::BGRA8UnormSrgb:
            return MTLPixelFormatBGRA8Unorm_sRGB;
        case TextureFormat::R16Float:
            return MTLPixelFormatR16Float;
        case TextureFormat::R16Uint:
            return MTLPixelFormatR16Uint;
        case TextureFormat::R16Sint:
            return MTLPixelFormatR16Sint;
        case TextureFormat::RG16Float:
            return MTLPixelFormatRG16Float;
        case TextureFormat::RG16Uint:
            return MTLPixelFormatRG16Uint;
        case TextureFormat::RG16Sint:
            return MTLPixelFormatRG16Sint;
        case TextureFormat::RGBA16Float:
            return MTLPixelFormatRGBA16Float;
        case TextureFormat::RGBA16Uint:
            return MTLPixelFormatRGBA16Uint;
        case TextureFormat::RGBA16Sint:
            return MTLPixelFormatRGBA16Sint;
        case TextureFormat::R32Float:
            return MTLPixelFormatR32Float;
        case TextureFormat::R32Uint:
            return MTLPixelFormatR32Uint;
        case TextureFormat::R32Sint:
            return MTLPixelFormatR32Sint;
        case TextureFormat::RG32Float:
            return MTLPixelFormatRG32Float;
        case TextureFormat::RG32Uint:
            return MTLPixelFormatRG32Uint;
        case TextureFormat::RG32Sint:
            return MTLPixelFormatRG32Sint;
        case TextureFormat::RGBA32Float:
            return MTLPixelFormatRGBA32Float;
        case TextureFormat::RGBA32Uint:
            return MTLPixelFormatRGBA32Uint;
        case TextureFormat::RGBA32Sint:
            return MTLPixelFormatRGBA32Sint;
        case TextureFormat::Depth16Unorm:
            return MTLPixelFormatDepth16Unorm;
        case TextureFormat::Depth32Float:
            return MTLPixelFormatDepth32Float;
        case TextureFormat::Depth24UnormStencil8:
            return MTLPixelFormatDepth24Unorm_Stencil8;
        case TextureFormat::Depth32FloatStencil8:
            return MTLPixelFormatDepth32Float_Stencil8;
        default:
            return MTLPixelFormatInvalid;
    }
}

inline TextureFormat from_mtl_pixel_format(MTLPixelFormat format) {
    switch (format) {
        case MTLPixelFormatR8Unorm:
            return TextureFormat::R8Unorm;
        case MTLPixelFormatRGBA8Unorm:
            return TextureFormat::RGBA8Unorm;
        case MTLPixelFormatBGRA8Unorm:
            return TextureFormat::BGRA8Unorm;
        case MTLPixelFormatBGRA8Unorm_sRGB:
            return TextureFormat::BGRA8UnormSrgb;
        case MTLPixelFormatRGBA16Float:
            return TextureFormat::RGBA16Float;
        case MTLPixelFormatRGBA32Float:
            return TextureFormat::RGBA32Float;
        case MTLPixelFormatDepth32Float:
            return TextureFormat::Depth32Float;
        case MTLPixelFormatDepth32Float_Stencil8:
            return TextureFormat::Depth32FloatStencil8;
        default:
            return TextureFormat::Unknown;
    }
}

// ============================================================================
// Vertex Format Conversions (TextureFormat used for vertex attributes)
// ============================================================================

inline MTLVertexFormat to_mtl_vertex_format(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8Unorm:
            return MTLVertexFormatUCharNormalized;
        case TextureFormat::R8Uint:
            return MTLVertexFormatUChar;
        case TextureFormat::R8Sint:
            return MTLVertexFormatChar;
        case TextureFormat::RG8Unorm:
            return MTLVertexFormatUChar2Normalized;
        case TextureFormat::RGBA8Unorm:
            return MTLVertexFormatUChar4Normalized;
        case TextureFormat::R16Float:
            return MTLVertexFormatHalf;
        case TextureFormat::RG16Float:
            return MTLVertexFormatHalf2;
        case TextureFormat::RGBA16Float:
            return MTLVertexFormatHalf4;
        case TextureFormat::R32Float:
            return MTLVertexFormatFloat;
        case TextureFormat::RG32Float:
            return MTLVertexFormatFloat2;
        case TextureFormat::RGBA32Float:
            return MTLVertexFormatFloat4;
        case TextureFormat::R32Uint:
            return MTLVertexFormatUInt;
        case TextureFormat::RG32Uint:
            return MTLVertexFormatUInt2;
        case TextureFormat::RGBA32Uint:
            return MTLVertexFormatUInt4;
        case TextureFormat::R32Sint:
            return MTLVertexFormatInt;
        case TextureFormat::RG32Sint:
            return MTLVertexFormatInt2;
        case TextureFormat::RGBA32Sint:
            return MTLVertexFormatInt4;
        default:
            return MTLVertexFormatInvalid;
    }
}

// ============================================================================
// Primitive Topology Conversions
// ============================================================================

inline MTLPrimitiveType to_mtl_primitive_type(PrimitiveTopology topology) {
    switch (topology) {
        case PrimitiveTopology::PointList:
            return MTLPrimitiveTypePoint;
        case PrimitiveTopology::LineList:
            return MTLPrimitiveTypeLine;
        case PrimitiveTopology::LineStrip:
            return MTLPrimitiveTypeLineStrip;
        case PrimitiveTopology::TriangleList:
            return MTLPrimitiveTypeTriangle;
        case PrimitiveTopology::TriangleStrip:
            return MTLPrimitiveTypeTriangleStrip;
        default:
            return MTLPrimitiveTypeTriangle;
    }
}

// ============================================================================
// Cull Mode Conversions
// ============================================================================

inline MTLCullMode to_mtl_cull_mode(CullMode mode) {
    switch (mode) {
        case CullMode::None:
            return MTLCullModeNone;
        case CullMode::Front:
            return MTLCullModeFront;
        case CullMode::Back:
            return MTLCullModeBack;
        default:
            return MTLCullModeNone;
    }
}

// ============================================================================
// Winding Order Conversions
// ============================================================================

inline MTLWinding to_mtl_winding(FrontFace face) {
    switch (face) {
        case FrontFace::CounterClockwise:
            return MTLWindingCounterClockwise;
        case FrontFace::Clockwise:
            return MTLWindingClockwise;
        default:
            return MTLWindingCounterClockwise;
    }
}

// ============================================================================
// Compare Function Conversions
// ============================================================================

inline MTLCompareFunction to_mtl_compare_function(CompareOp op) {
    switch (op) {
        case CompareOp::Never:
            return MTLCompareFunctionNever;
        case CompareOp::Less:
            return MTLCompareFunctionLess;
        case CompareOp::Equal:
            return MTLCompareFunctionEqual;
        case CompareOp::LessOrEqual:
            return MTLCompareFunctionLessEqual;
        case CompareOp::Greater:
            return MTLCompareFunctionGreater;
        case CompareOp::NotEqual:
            return MTLCompareFunctionNotEqual;
        case CompareOp::GreaterOrEqual:
            return MTLCompareFunctionGreaterEqual;
        case CompareOp::Always:
            return MTLCompareFunctionAlways;
        default:
            return MTLCompareFunctionAlways;
    }
}

// ============================================================================
// Blend Factor Conversions
// ============================================================================

inline MTLBlendFactor to_mtl_blend_factor(BlendFactor factor) {
    switch (factor) {
        case BlendFactor::Zero:
            return MTLBlendFactorZero;
        case BlendFactor::One:
            return MTLBlendFactorOne;
        case BlendFactor::SrcColor:
            return MTLBlendFactorSourceColor;
        case BlendFactor::OneMinusSrcColor:
            return MTLBlendFactorOneMinusSourceColor;
        case BlendFactor::DstColor:
            return MTLBlendFactorDestinationColor;
        case BlendFactor::OneMinusDstColor:
            return MTLBlendFactorOneMinusDestinationColor;
        case BlendFactor::SrcAlpha:
            return MTLBlendFactorSourceAlpha;
        case BlendFactor::OneMinusSrcAlpha:
            return MTLBlendFactorOneMinusSourceAlpha;
        case BlendFactor::DstAlpha:
            return MTLBlendFactorDestinationAlpha;
        case BlendFactor::OneMinusDstAlpha:
            return MTLBlendFactorOneMinusDestinationAlpha;
        case BlendFactor::ConstantColor:
            return MTLBlendFactorBlendColor;
        case BlendFactor::OneMinusConstantColor:
            return MTLBlendFactorOneMinusBlendColor;
        case BlendFactor::SrcAlphaSaturate:
            return MTLBlendFactorSourceAlphaSaturated;
        default:
            return MTLBlendFactorOne;
    }
}

// ============================================================================
// Blend Operation Conversions
// ============================================================================

inline MTLBlendOperation to_mtl_blend_operation(BlendOp op) {
    switch (op) {
        case BlendOp::Add:
            return MTLBlendOperationAdd;
        case BlendOp::Subtract:
            return MTLBlendOperationSubtract;
        case BlendOp::ReverseSubtract:
            return MTLBlendOperationReverseSubtract;
        case BlendOp::Min:
            return MTLBlendOperationMin;
        case BlendOp::Max:
            return MTLBlendOperationMax;
        default:
            return MTLBlendOperationAdd;
    }
}

// ============================================================================
// Load/Store Action Conversions
// ============================================================================

inline MTLLoadAction to_mtl_load_action(LoadOp op) {
    switch (op) {
        case LoadOp::Load:
            return MTLLoadActionLoad;
        case LoadOp::Clear:
            return MTLLoadActionClear;
        case LoadOp::DontCare:
            return MTLLoadActionDontCare;
        default:
            return MTLLoadActionDontCare;
    }
}

inline MTLStoreAction to_mtl_store_action(StoreOp op) {
    switch (op) {
        case StoreOp::Store:
            return MTLStoreActionStore;
        case StoreOp::DontCare:
            return MTLStoreActionDontCare;
        default:
            return MTLStoreActionStore;
    }
}

// ============================================================================
// Index Type Conversions
// ============================================================================

inline MTLIndexType to_mtl_index_type(IndexType type) {
    switch (type) {
        case IndexType::Uint16:
            return MTLIndexTypeUInt16;
        case IndexType::Uint32:
            return MTLIndexTypeUInt32;
        default:
            return MTLIndexTypeUInt32;
    }
}

// ============================================================================
// Sampler State Conversions
// ============================================================================

inline MTLSamplerMinMagFilter to_mtl_filter(FilterMode filter) {
    switch (filter) {
        case FilterMode::Nearest:
            return MTLSamplerMinMagFilterNearest;
        case FilterMode::Linear:
            return MTLSamplerMinMagFilterLinear;
        default:
            return MTLSamplerMinMagFilterLinear;
    }
}

inline MTLSamplerMipFilter to_mtl_mip_filter(FilterMode filter) {
    switch (filter) {
        case FilterMode::Nearest:
            return MTLSamplerMipFilterNearest;
        case FilterMode::Linear:
            return MTLSamplerMipFilterLinear;
        default:
            return MTLSamplerMipFilterLinear;
    }
}

inline MTLSamplerAddressMode to_mtl_address_mode(AddressMode mode) {
    switch (mode) {
        case AddressMode::Repeat:
            return MTLSamplerAddressModeRepeat;
        case AddressMode::MirroredRepeat:
            return MTLSamplerAddressModeMirrorRepeat;
        case AddressMode::ClampToEdge:
            return MTLSamplerAddressModeClampToEdge;
        case AddressMode::ClampToBorder:
            return MTLSamplerAddressModeClampToBorderColor;
        default:
            return MTLSamplerAddressModeRepeat;
    }
}

// ============================================================================
// Texture Type Conversions
// ============================================================================

inline MTLTextureType to_mtl_texture_type(TextureType type, uint32_t array_layers) {
    switch (type) {
        case TextureType::Texture1D:
            return array_layers > 1 ? MTLTextureType1DArray : MTLTextureType1D;
        case TextureType::Texture2D:
            return array_layers > 1 ? MTLTextureType2DArray : MTLTextureType2D;
        case TextureType::Texture3D:
            return MTLTextureType3D;
        case TextureType::TextureCube:
            return array_layers > 6 ? MTLTextureTypeCubeArray : MTLTextureTypeCube;
        case TextureType::Texture2DArray:
            return MTLTextureType2DArray;
        default:
            return MTLTextureType2D;
    }
}

}  // namespace realcraft::graphics::metal
