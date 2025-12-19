// RealCraft Graphics Abstraction Layer
// shader_compiler.hpp - Shader compilation and cross-compilation

#pragma once

#include "types.hpp"

#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace realcraft::graphics {

// ============================================================================
// Shader Compilation Options
// ============================================================================

struct ShaderCompileOptions {
    ShaderStage stage = ShaderStage::Vertex;
    std::string entry_point = "main";
    std::vector<std::string> defines;        // Preprocessor definitions
    std::vector<std::string> include_paths;  // Include search paths
    bool generate_debug_info = false;
    bool optimize = true;
    bool generate_reflection = true;
};

// ============================================================================
// Compiled Shader Result
// ============================================================================

struct CompiledShader {
    bool success = false;
    std::string error_message;
    std::vector<uint8_t> spirv_bytecode;  // SPIR-V bytecode (Vulkan)
    std::vector<uint8_t> msl_bytecode;    // MSL bytecode or source (Metal)
    std::string msl_source;               // MSL source code (for debugging)
    ShaderReflection reflection;
};

// ============================================================================
// Shader Compiler
// ============================================================================

// Compiles GLSL shaders to SPIR-V and cross-compiles to MSL for Metal
class ShaderCompiler {
public:
    ShaderCompiler();
    ~ShaderCompiler();

    // Non-copyable
    ShaderCompiler(const ShaderCompiler&) = delete;
    ShaderCompiler& operator=(const ShaderCompiler&) = delete;

    // Move-only
    ShaderCompiler(ShaderCompiler&&) noexcept;
    ShaderCompiler& operator=(ShaderCompiler&&) noexcept;

    // ========================================================================
    // Compilation
    // ========================================================================

    // Compile GLSL source to SPIR-V
    [[nodiscard]] CompiledShader compile_glsl(std::string_view source, const ShaderCompileOptions& options);

    // Compile GLSL file to SPIR-V
    [[nodiscard]] CompiledShader compile_glsl_file(const std::filesystem::path& path,
                                                   const ShaderCompileOptions& options);

    // Async compilation
    [[nodiscard]] std::future<CompiledShader> compile_glsl_async(std::string source, ShaderCompileOptions options);

    // ========================================================================
    // Cross-Compilation
    // ========================================================================

    // Cross-compile SPIR-V to MSL (Metal Shading Language)
    [[nodiscard]] std::optional<std::string> spirv_to_msl(std::span<const uint8_t> spirv);

    // ========================================================================
    // Reflection
    // ========================================================================

    // Extract reflection data from SPIR-V bytecode
    [[nodiscard]] std::optional<ShaderReflection> reflect_spirv(std::span<const uint8_t> spirv);

    // ========================================================================
    // Hot-Reload Support
    // ========================================================================

    using ReloadCallback = std::function<void(const std::filesystem::path& path, const CompiledShader& shader)>;
    using ErrorCallback = std::function<void(const std::filesystem::path& path, const std::string& error)>;

    // Watch a shader file for changes
    void watch_file(const std::filesystem::path& path, const ShaderCompileOptions& options, ReloadCallback on_reload,
                    ErrorCallback on_error = nullptr);

    // Stop watching a file
    void stop_watching(const std::filesystem::path& path);

    // Stop watching all files
    void stop_watching_all();

    // Poll for file changes (call each frame in debug builds)
    void poll_file_changes();

    // Check if hot-reload is enabled
    [[nodiscard]] bool is_hot_reload_enabled() const;

    // Enable/disable hot-reload
    void set_hot_reload_enabled(bool enabled);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Utility Functions
// ============================================================================

// Determine shader stage from file extension
[[nodiscard]] std::optional<ShaderStage> shader_stage_from_extension(std::string_view extension);

// Get file extension for shader stage
[[nodiscard]] const char* shader_stage_extension(ShaderStage stage);

// Get human-readable name for shader stage
[[nodiscard]] const char* shader_stage_name(ShaderStage stage);

}  // namespace realcraft::graphics
