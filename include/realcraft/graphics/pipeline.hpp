// RealCraft Graphics Abstraction Layer
// pipeline.hpp - GPU pipeline interfaces

#pragma once

#include "types.hpp"

#include <memory>

namespace realcraft::graphics {

// Abstract render pipeline class
// Implementations: MetalPipeline, VulkanPipeline
class Pipeline {
public:
    virtual ~Pipeline() = default;

    // Non-copyable
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    // Properties
    [[nodiscard]] virtual PrimitiveTopology get_topology() const = 0;

protected:
    Pipeline() = default;
};

// Abstract compute pipeline class
// Implementations: MetalComputePipeline, VulkanComputePipeline
class ComputePipeline {
public:
    virtual ~ComputePipeline() = default;

    // Non-copyable
    ComputePipeline(const ComputePipeline&) = delete;
    ComputePipeline& operator=(const ComputePipeline&) = delete;

protected:
    ComputePipeline() = default;
};

}  // namespace realcraft::graphics
