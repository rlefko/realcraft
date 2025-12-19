// RealCraft Graphics Abstraction Layer
// metal_sampler.mm - Metal sampler implementation

#import <Metal/Metal.h>

#include "metal_impl.hpp"
#include "metal_types.hpp"

#include <spdlog/spdlog.h>

namespace realcraft::graphics {

// ============================================================================
// MetalSampler Implementation
// ============================================================================

MetalSampler::MetalSampler(id<MTLDevice> device, const SamplerDesc& desc)
    : min_filter_(desc.min_filter)
    , mag_filter_(desc.mag_filter)
    , mip_filter_(desc.mip_filter)
    , address_u_(desc.address_u)
    , address_v_(desc.address_v)
    , address_w_(desc.address_w)
    , max_anisotropy_(desc.max_anisotropy) {
    @autoreleasepool {
        MTLSamplerDescriptor* sampler_desc = [[MTLSamplerDescriptor alloc] init];

        sampler_desc.minFilter = metal::to_mtl_filter(min_filter_);
        sampler_desc.magFilter = metal::to_mtl_filter(mag_filter_);
        sampler_desc.mipFilter = metal::to_mtl_mip_filter(mip_filter_);

        sampler_desc.sAddressMode = metal::to_mtl_address_mode(address_u_);
        sampler_desc.tAddressMode = metal::to_mtl_address_mode(address_v_);
        sampler_desc.rAddressMode = metal::to_mtl_address_mode(address_w_);

        sampler_desc.maxAnisotropy = static_cast<NSUInteger>(desc.max_anisotropy);

        if (desc.compare_enable) {
            sampler_desc.compareFunction = metal::to_mtl_compare_function(desc.compare_op);
        }

        sampler_desc.lodMinClamp = desc.min_lod;
        sampler_desc.lodMaxClamp = desc.max_lod;

        // Set label on descriptor before creating sampler (sampler state label is readonly)
        if (!desc.debug_name.empty()) {
            sampler_desc.label = [NSString stringWithUTF8String:desc.debug_name.c_str()];
        }

        sampler_ = [device newSamplerStateWithDescriptor:sampler_desc];
    }
}

MetalSampler::~MetalSampler() {
    @autoreleasepool {
        sampler_ = nil;
    }
}

FilterMode MetalSampler::get_min_filter() const { return min_filter_; }
FilterMode MetalSampler::get_mag_filter() const { return mag_filter_; }
AddressMode MetalSampler::get_address_mode_u() const { return address_u_; }
AddressMode MetalSampler::get_address_mode_v() const { return address_v_; }
AddressMode MetalSampler::get_address_mode_w() const { return address_w_; }
float MetalSampler::get_max_anisotropy() const { return max_anisotropy_; }
id<MTLSamplerState> MetalSampler::get_mtl_sampler() const { return sampler_; }

// ============================================================================
// Factory Function
// ============================================================================

std::unique_ptr<Sampler> create_metal_sampler(id<MTLDevice> device, const SamplerDesc& desc) {
    return std::make_unique<MetalSampler>(device, desc);
}

}  // namespace realcraft::graphics
