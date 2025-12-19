// RealCraft Graphics Abstraction Layer
// shader.hpp - GPU shader interface

#pragma once

#include "types.hpp"

#include <memory>
#include <optional>
#include <string>

namespace realcraft::graphics {

// Abstract GPU shader class
// Implementations: MetalShader, VulkanShader
class Shader {
public:
    virtual ~Shader() = default;

    // Non-copyable
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // Properties
    [[nodiscard]] virtual ShaderStage get_stage() const = 0;
    [[nodiscard]] virtual const std::string& get_entry_point() const = 0;

    // Reflection data (may be null if reflection was not requested)
    [[nodiscard]] virtual const ShaderReflection* get_reflection() const = 0;

protected:
    Shader() = default;
};

}  // namespace realcraft::graphics
