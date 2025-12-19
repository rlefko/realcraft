// RealCraft Graphics Abstraction Layer
// metal_buffer.mm - Metal buffer implementation

#import <Metal/Metal.h>

#include "metal_impl.hpp"
#include "metal_types.hpp"

#include <spdlog/spdlog.h>

#include <cstring>

namespace realcraft::graphics {

// ============================================================================
// MetalBuffer Implementation
// ============================================================================

MetalBuffer::MetalBuffer(id<MTLDevice> device, const BufferDesc& desc)
    : size_(desc.size), usage_(desc.usage), host_visible_(desc.host_visible) {
    @autoreleasepool {
        MTLResourceOptions options = MTLResourceStorageModeShared;

        if (!host_visible_) {
            // Use private storage for GPU-only buffers (faster on discrete GPUs)
            options = MTLResourceStorageModePrivate;
        }

        if (desc.initial_data && host_visible_) {
            // Create buffer with initial data
            buffer_ = [device newBufferWithBytes:desc.initial_data
                                          length:size_
                                         options:options];
        } else {
            buffer_ = [device newBufferWithLength:size_ options:options];

            // If we have initial data but buffer is not host visible,
            // we need to use a staging buffer (simplified: just skip for now)
            if (desc.initial_data && !host_visible_) {
                spdlog::warn("MetalBuffer: Initial data for non-host-visible buffer "
                             "requires staging buffer (not yet implemented)");
            }
        }

        if (!desc.debug_name.empty()) {
            buffer_.label = [NSString stringWithUTF8String:desc.debug_name.c_str()];
        }
    }
}

MetalBuffer::~MetalBuffer() {
    @autoreleasepool {
        buffer_ = nil;
    }
}

void* MetalBuffer::map() {
    if (!host_visible_) {
        spdlog::error("MetalBuffer::map called on non-host-visible buffer");
        return nullptr;
    }
    if (mapped_) {
        spdlog::warn("MetalBuffer::map called on already mapped buffer");
    }
    mapped_ = true;
    return [buffer_ contents];
}

void MetalBuffer::unmap() {
    if (!mapped_) {
        spdlog::warn("MetalBuffer::unmap called on non-mapped buffer");
    }
    mapped_ = false;
    // Metal shared buffers don't need explicit unmap/flush
}

void MetalBuffer::write(const void* data, size_t size, size_t offset) {
    if (!host_visible_) {
        spdlog::error("MetalBuffer::write called on non-host-visible buffer");
        return;
    }
    if (offset + size > size_) {
        spdlog::error("MetalBuffer::write out of bounds: offset={} size={} buffer_size={}",
                      offset, size, size_);
        return;
    }

    void* dest = static_cast<uint8_t*>([buffer_ contents]) + offset;
    std::memcpy(dest, data, size);
}

void MetalBuffer::read(void* data, size_t size, size_t offset) const {
    if (!host_visible_) {
        spdlog::error("MetalBuffer::read called on non-host-visible buffer");
        return;
    }
    if (offset + size > size_) {
        spdlog::error("MetalBuffer::read out of bounds: offset={} size={} buffer_size={}",
                      offset, size, size_);
        return;
    }

    const void* src = static_cast<const uint8_t*>([buffer_ contents]) + offset;
    std::memcpy(data, src, size);
}

size_t MetalBuffer::get_size() const { return size_; }
BufferUsage MetalBuffer::get_usage() const { return usage_; }
bool MetalBuffer::is_host_visible() const { return host_visible_; }
id<MTLBuffer> MetalBuffer::get_mtl_buffer() const { return buffer_; }

// ============================================================================
// Factory Function
// ============================================================================

std::unique_ptr<Buffer> create_metal_buffer(id<MTLDevice> device, const BufferDesc& desc) {
    return std::make_unique<MetalBuffer>(device, desc);
}

}  // namespace realcraft::graphics
