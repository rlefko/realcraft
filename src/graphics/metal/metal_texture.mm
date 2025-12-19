// RealCraft Graphics Abstraction Layer
// metal_texture.mm - Metal texture implementation

#import <Metal/Metal.h>

#include "metal_impl.hpp"
#include "metal_types.hpp"

#include <spdlog/spdlog.h>

namespace realcraft::graphics {

// ============================================================================
// MetalTexture Implementation
// ============================================================================

MetalTexture::MetalTexture(id<MTLDevice> device, const TextureDesc& desc)
    : width_(desc.width)
    , height_(desc.height)
    , depth_(desc.depth)
    , mip_levels_(desc.mip_levels)
    , array_layers_(desc.array_layers)
    , format_(desc.format)
    , type_(desc.type)
    , usage_(desc.usage)
    , owns_texture_(true) {
    @autoreleasepool {
        MTLTextureDescriptor* tex_desc = [[MTLTextureDescriptor alloc] init];

        tex_desc.textureType = metal::to_mtl_texture_type(type_, array_layers_);
        tex_desc.pixelFormat = metal::to_mtl_pixel_format(format_);
        tex_desc.width = width_;
        tex_desc.height = height_;
        tex_desc.depth = depth_;
        tex_desc.mipmapLevelCount = mip_levels_;
        tex_desc.arrayLength = (type_ == TextureType::TextureCube) ? 1 : array_layers_;

        // Set usage flags
        MTLTextureUsage mtl_usage = MTLTextureUsageUnknown;
        if (has_flag(usage_, TextureUsage::Sampled)) {
            mtl_usage |= MTLTextureUsageShaderRead;
        }
        if (has_flag(usage_, TextureUsage::Storage)) {
            mtl_usage |= MTLTextureUsageShaderWrite;
        }
        if (has_flag(usage_, TextureUsage::RenderTarget) ||
            has_flag(usage_, TextureUsage::DepthStencil)) {
            mtl_usage |= MTLTextureUsageRenderTarget;
        }
        tex_desc.usage = mtl_usage;

        // Storage mode
        if (has_flag(usage_, TextureUsage::RenderTarget) ||
            has_flag(usage_, TextureUsage::DepthStencil)) {
            tex_desc.storageMode = MTLStorageModePrivate;
        } else {
            tex_desc.storageMode = MTLStorageModeShared;
        }

        texture_ = [device newTextureWithDescriptor:tex_desc];

        if (!desc.debug_name.empty()) {
            texture_.label = [NSString stringWithUTF8String:desc.debug_name.c_str()];
        }
    }
}

MetalTexture::MetalTexture(id<MTLTexture> texture, bool owns)
    : texture_(texture), owns_texture_(owns) {
    if (texture_) {
        width_ = static_cast<uint32_t>(texture_.width);
        height_ = static_cast<uint32_t>(texture_.height);
        depth_ = static_cast<uint32_t>(texture_.depth);
        mip_levels_ = static_cast<uint32_t>(texture_.mipmapLevelCount);
        array_layers_ = static_cast<uint32_t>(texture_.arrayLength);
        format_ = metal::from_mtl_pixel_format(texture_.pixelFormat);

        switch (texture_.textureType) {
            case MTLTextureType1D:
            case MTLTextureType1DArray:
                type_ = TextureType::Texture1D;
                break;
            case MTLTextureType2D:
            case MTLTextureType2DArray:
                type_ = TextureType::Texture2D;
                break;
            case MTLTextureType3D:
                type_ = TextureType::Texture3D;
                break;
            case MTLTextureTypeCube:
            case MTLTextureTypeCubeArray:
                type_ = TextureType::TextureCube;
                break;
            default:
                type_ = TextureType::Texture2D;
                break;
        }
    }
}

MetalTexture::~MetalTexture() {
    @autoreleasepool {
        if (owns_texture_) {
            texture_ = nil;
        }
    }
}

uint32_t MetalTexture::get_width() const { return width_; }
uint32_t MetalTexture::get_height() const { return height_; }
uint32_t MetalTexture::get_depth() const { return depth_; }
uint32_t MetalTexture::get_mip_levels() const { return mip_levels_; }
uint32_t MetalTexture::get_array_layers() const { return array_layers_; }
TextureFormat MetalTexture::get_format() const { return format_; }
TextureType MetalTexture::get_type() const { return type_; }
TextureUsage MetalTexture::get_usage() const { return usage_; }
void* MetalTexture::get_native_handle() const { return (__bridge void*)texture_; }
id<MTLTexture> MetalTexture::get_mtl_texture() const { return texture_; }

// ============================================================================
// Factory Function
// ============================================================================

std::unique_ptr<Texture> create_metal_texture(id<MTLDevice> device, const TextureDesc& desc) {
    return std::make_unique<MetalTexture>(device, desc);
}

}  // namespace realcraft::graphics
