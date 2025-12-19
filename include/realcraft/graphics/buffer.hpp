// RealCraft Graphics Abstraction Layer
// buffer.hpp - GPU buffer interface

#pragma once

#include "types.hpp"

#include <cstddef>
#include <memory>

namespace realcraft::graphics {

// Abstract GPU buffer class
// Implementations: MetalBuffer, VulkanBuffer
class Buffer {
public:
    virtual ~Buffer() = default;

    // Non-copyable
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    // Properties
    [[nodiscard]] virtual size_t get_size() const = 0;
    [[nodiscard]] virtual BufferUsage get_usage() const = 0;
    [[nodiscard]] virtual bool is_host_visible() const = 0;

    // Host-visible buffer operations
    // map() returns nullptr if buffer is not host-visible
    [[nodiscard]] virtual void* map() = 0;
    virtual void unmap() = 0;

    // Write data to buffer (for host-visible buffers)
    // For non-host-visible buffers, this may perform a staging copy
    virtual void write(const void* data, size_t size, size_t offset = 0) = 0;

    // Read data from buffer (for host-visible buffers)
    virtual void read(void* data, size_t size, size_t offset = 0) const = 0;

protected:
    Buffer() = default;
};

}  // namespace realcraft::graphics
