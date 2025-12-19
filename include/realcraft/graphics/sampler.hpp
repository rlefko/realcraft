// RealCraft Graphics Abstraction Layer
// sampler.hpp - GPU sampler interface

#pragma once

#include "types.hpp"

#include <memory>

namespace realcraft::graphics {

// Abstract GPU sampler class
// Implementations: MetalSampler, VulkanSampler
class Sampler {
public:
    virtual ~Sampler() = default;

    // Non-copyable
    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;

    // Properties
    [[nodiscard]] virtual FilterMode get_min_filter() const = 0;
    [[nodiscard]] virtual FilterMode get_mag_filter() const = 0;
    [[nodiscard]] virtual AddressMode get_address_mode_u() const = 0;
    [[nodiscard]] virtual AddressMode get_address_mode_v() const = 0;
    [[nodiscard]] virtual AddressMode get_address_mode_w() const = 0;
    [[nodiscard]] virtual float get_max_anisotropy() const = 0;

protected:
    Sampler() = default;
};

}  // namespace realcraft::graphics
