// RealCraft Graphics Abstraction Layer
// shader_compiler.cpp - Shader compilation and cross-compilation

#include <spdlog/spdlog.h>

#include <chrono>
#include <fstream>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <mutex>
#include <realcraft/graphics/shader_compiler.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>
#include <spirv_cross/spirv_msl.hpp>
#include <spirv_cross/spirv_reflect.hpp>
#include <unordered_map>

namespace realcraft::graphics {

namespace {

// Initialize glslang once
std::once_flag glslang_init_flag;

void initialize_glslang() {
    std::call_once(glslang_init_flag, []() { glslang::InitializeProcess(); });
}

// Convert ShaderStage to glslang stage
EShLanguage to_glslang_stage(ShaderStage stage) {
    switch (stage) {
        case ShaderStage::Vertex:
            return EShLangVertex;
        case ShaderStage::Fragment:
            return EShLangFragment;
        case ShaderStage::Compute:
            return EShLangCompute;
        case ShaderStage::RayGeneration:
            return EShLangRayGen;
        case ShaderStage::RayMiss:
            return EShLangMiss;
        case ShaderStage::RayClosestHit:
            return EShLangClosestHit;
        case ShaderStage::RayAnyHit:
            return EShLangAnyHit;
        case ShaderStage::RayIntersection:
            return EShLangIntersect;
        default:
            return EShLangVertex;
    }
}

// Extract reflection data from SPIRV-Cross compiler
ShaderReflection extract_reflection(spirv_cross::Compiler& compiler, ShaderStage stage) {
    ShaderReflection reflection;
    reflection.stage = stage;

    // Get shader resources
    spirv_cross::ShaderResources resources = compiler.get_shader_resources();

    // Uniform buffers
    for (const auto& ubo : resources.uniform_buffers) {
        ShaderUniformBuffer uniform_buffer;
        uniform_buffer.name = ubo.name;
        uniform_buffer.set = compiler.get_decoration(ubo.id, spv::DecorationDescriptorSet);
        uniform_buffer.binding = compiler.get_decoration(ubo.id, spv::DecorationBinding);

        const auto& type = compiler.get_type(ubo.base_type_id);
        uniform_buffer.size = compiler.get_declared_struct_size(type);

        // Get struct members
        for (uint32_t i = 0; i < type.member_types.size(); ++i) {
            ShaderUniformMember member;
            member.name = compiler.get_member_name(ubo.base_type_id, i);
            member.offset = compiler.type_struct_member_offset(type, i);
            member.size = compiler.get_declared_struct_member_size(type, i);

            const auto& member_type = compiler.get_type(type.member_types[i]);
            if (member_type.basetype == spirv_cross::SPIRType::Float) {
                if (member_type.columns == 4 && member_type.vecsize == 4) {
                    member.type_name = "mat4";
                } else if (member_type.columns == 3 && member_type.vecsize == 3) {
                    member.type_name = "mat3";
                } else if (member_type.vecsize == 4) {
                    member.type_name = "vec4";
                } else if (member_type.vecsize == 3) {
                    member.type_name = "vec3";
                } else if (member_type.vecsize == 2) {
                    member.type_name = "vec2";
                } else {
                    member.type_name = "float";
                }
            } else if (member_type.basetype == spirv_cross::SPIRType::Int) {
                if (member_type.vecsize == 4) {
                    member.type_name = "ivec4";
                } else if (member_type.vecsize == 3) {
                    member.type_name = "ivec3";
                } else if (member_type.vecsize == 2) {
                    member.type_name = "ivec2";
                } else {
                    member.type_name = "int";
                }
            }

            uniform_buffer.members.push_back(member);
        }

        reflection.uniform_buffers.push_back(uniform_buffer);
    }

    // Sampled images
    for (const auto& sampler : resources.sampled_images) {
        ShaderSampledImage sampled_image;
        sampled_image.name = sampler.name;
        sampled_image.set = compiler.get_decoration(sampler.id, spv::DecorationDescriptorSet);
        sampled_image.binding = compiler.get_decoration(sampler.id, spv::DecorationBinding);

        const auto& type = compiler.get_type(sampler.type_id);
        switch (type.image.dim) {
            case spv::Dim1D:
                sampled_image.dimension = TextureType::Texture1D;
                break;
            case spv::Dim2D:
                sampled_image.dimension = TextureType::Texture2D;
                break;
            case spv::Dim3D:
                sampled_image.dimension = TextureType::Texture3D;
                break;
            case spv::DimCube:
                sampled_image.dimension = TextureType::TextureCube;
                break;
            default:
                sampled_image.dimension = TextureType::Texture2D;
                break;
        }

        reflection.sampled_images.push_back(sampled_image);
    }

    // Storage buffers
    for (const auto& ssbo : resources.storage_buffers) {
        ShaderStorageBuffer storage_buffer;
        storage_buffer.name = ssbo.name;
        storage_buffer.set = compiler.get_decoration(ssbo.id, spv::DecorationDescriptorSet);
        storage_buffer.binding = compiler.get_decoration(ssbo.id, spv::DecorationBinding);

        const auto& type = compiler.get_type(ssbo.base_type_id);
        storage_buffer.size = compiler.get_declared_struct_size(type);

        reflection.storage_buffers.push_back(storage_buffer);
    }

    // Push constants
    for (const auto& pc : resources.push_constant_buffers) {
        ShaderPushConstant push_constant;
        const auto& type = compiler.get_type(pc.base_type_id);
        push_constant.size = compiler.get_declared_struct_size(type);
        push_constant.stages = stage;
        reflection.push_constants.push_back(push_constant);
    }

    // Stage inputs
    for (const auto& input : resources.stage_inputs) {
        ShaderStageInput stage_input;
        stage_input.name = input.name;
        stage_input.location = compiler.get_decoration(input.id, spv::DecorationLocation);
        reflection.inputs.push_back(stage_input);
    }

    // Stage outputs
    for (const auto& output : resources.stage_outputs) {
        ShaderStageInput stage_output;
        stage_output.name = output.name;
        stage_output.location = compiler.get_decoration(output.id, spv::DecorationLocation);
        reflection.outputs.push_back(stage_output);
    }

    return reflection;
}

}  // namespace

// ============================================================================
// ShaderCompiler Implementation
// ============================================================================

struct ShaderCompiler::Impl {
    bool hot_reload_enabled = false;

    struct WatchedFile {
        std::filesystem::path path;
        ShaderCompileOptions options;
        ReloadCallback on_reload;
        ErrorCallback on_error;
        std::filesystem::file_time_type last_modified;
    };

    std::unordered_map<std::string, WatchedFile> watched_files;
    std::mutex watched_files_mutex;
};

ShaderCompiler::ShaderCompiler() : impl_(std::make_unique<Impl>()) {
    initialize_glslang();
}

ShaderCompiler::~ShaderCompiler() {
    stop_watching_all();
}

ShaderCompiler::ShaderCompiler(ShaderCompiler&&) noexcept = default;
ShaderCompiler& ShaderCompiler::operator=(ShaderCompiler&&) noexcept = default;

CompiledShader ShaderCompiler::compile_glsl(std::string_view source, const ShaderCompileOptions& options) {
    CompiledShader result;
    result.reflection.stage = options.stage;
    result.reflection.entry_point = options.entry_point;

    EShLanguage glslang_stage = to_glslang_stage(options.stage);
    glslang::TShader shader(glslang_stage);

    const char* source_str = source.data();
    int source_len = static_cast<int>(source.size());
    shader.setStringsWithLengths(&source_str, &source_len, 1);
    shader.setEntryPoint(options.entry_point.c_str());
    shader.setSourceEntryPoint(options.entry_point.c_str());

    // Set environment
    shader.setEnvInput(glslang::EShSourceGlsl, glslang_stage, glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);

    // Add defines
    std::string preamble;
    for (const auto& define : options.defines) {
        preamble += "#define " + define + "\n";
    }
    if (!preamble.empty()) {
        shader.setPreamble(preamble.c_str());
    }

    // Parse
    EShMessages messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
    if (options.generate_debug_info) {
        messages = static_cast<EShMessages>(messages | EShMsgDebugInfo);
    }

    const TBuiltInResource* resources = GetDefaultResources();
    if (!shader.parse(resources, 450, false, messages)) {
        result.success = false;
        result.error_message = shader.getInfoLog();
        spdlog::error("GLSL compilation failed: {}", result.error_message);
        return result;
    }

    // Link
    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(messages)) {
        result.success = false;
        result.error_message = program.getInfoLog();
        spdlog::error("GLSL linking failed: {}", result.error_message);
        return result;
    }

    // Generate SPIR-V
    std::vector<uint32_t> spirv;
    spv::SpvBuildLogger logger;
    glslang::SpvOptions spv_options;
    spv_options.generateDebugInfo = options.generate_debug_info;
    spv_options.stripDebugInfo = false;
    spv_options.disableOptimizer = !options.optimize;
    spv_options.optimizeSize = false;

    glslang::GlslangToSpv(*program.getIntermediate(glslang_stage), spirv, &logger, &spv_options);

    if (spirv.empty()) {
        result.success = false;
        result.error_message = "SPIR-V generation failed: " + logger.getAllMessages();
        spdlog::error("SPIR-V generation failed");
        return result;
    }

    // Copy SPIR-V to result
    result.spirv_bytecode.resize(spirv.size() * sizeof(uint32_t));
    std::memcpy(result.spirv_bytecode.data(), spirv.data(), result.spirv_bytecode.size());

    // Generate reflection if requested
    if (options.generate_reflection) {
        auto reflection_result = reflect_spirv(result.spirv_bytecode);
        if (reflection_result) {
            result.reflection = *reflection_result;
        }
    }

    // Cross-compile to MSL for Metal
    auto msl_result = spirv_to_msl(result.spirv_bytecode);
    if (msl_result) {
        result.msl_source = *msl_result;
        result.msl_bytecode.assign(result.msl_source.begin(), result.msl_source.end());
    }

    result.success = true;
    spdlog::debug("Shader compiled successfully: stage={}", static_cast<int>(options.stage));
    return result;
}

CompiledShader ShaderCompiler::compile_glsl_file(const std::filesystem::path& path,
                                                 const ShaderCompileOptions& options) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        CompiledShader result;
        result.success = false;
        result.error_message = "Failed to open file: " + path.string();
        return result;
    }

    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return compile_glsl(source, options);
}

std::future<CompiledShader> ShaderCompiler::compile_glsl_async(std::string source, ShaderCompileOptions options) {
    return std::async(std::launch::async, [this, source = std::move(source), options = std::move(options)]() mutable {
        return compile_glsl(source, options);
    });
}

std::optional<std::string> ShaderCompiler::spirv_to_msl(std::span<const uint8_t> spirv) {
    if (spirv.empty() || spirv.size() % 4 != 0) {
        spdlog::error("Invalid SPIR-V bytecode");
        return std::nullopt;
    }

    std::vector<uint32_t> spirv_words(spirv.size() / 4);
    std::memcpy(spirv_words.data(), spirv.data(), spirv.size());

    try {
        spirv_cross::CompilerMSL msl(std::move(spirv_words));

        // Configure MSL options
        spirv_cross::CompilerMSL::Options msl_options;
        msl_options.platform = spirv_cross::CompilerMSL::Options::macOS;
        msl_options.msl_version = spirv_cross::CompilerMSL::Options::make_msl_version(3, 0);
        msl_options.enable_decoration_binding = true;
        msl_options.argument_buffers = false;  // Use discrete resources for simplicity

        // Remap push constants to buffer index 30 to avoid conflicts with uniform buffers
        // Uniform buffers use indices 0, 1, 2, etc. based on their binding decorations
        spirv_cross::MSLResourceBinding push_constant_binding;
        push_constant_binding.stage = spv::ExecutionModelVertex;  // Applies to vertex stage
        push_constant_binding.desc_set = spirv_cross::kPushConstDescSet;
        push_constant_binding.binding = spirv_cross::kPushConstBinding;
        push_constant_binding.msl_buffer = 30;
        msl.add_msl_resource_binding(push_constant_binding);

        // Also for fragment stage in case push constants are used there
        push_constant_binding.stage = spv::ExecutionModelFragment;
        msl.add_msl_resource_binding(push_constant_binding);

        msl.set_msl_options(msl_options);

        std::string msl_source = msl.compile();
        return msl_source;
    } catch (const spirv_cross::CompilerError& e) {
        spdlog::error("SPIR-V to MSL cross-compilation failed: {}", e.what());
        return std::nullopt;
    }
}

std::optional<ShaderReflection> ShaderCompiler::reflect_spirv(std::span<const uint8_t> spirv) {
    if (spirv.empty() || spirv.size() % 4 != 0) {
        return std::nullopt;
    }

    std::vector<uint32_t> spirv_words(spirv.size() / 4);
    std::memcpy(spirv_words.data(), spirv.data(), spirv.size());

    try {
        spirv_cross::Compiler compiler(std::move(spirv_words));
        return extract_reflection(compiler, ShaderStage::Vertex);  // Stage will be overwritten
    } catch (const spirv_cross::CompilerError& e) {
        spdlog::error("SPIR-V reflection failed: {}", e.what());
        return std::nullopt;
    }
}

void ShaderCompiler::watch_file(const std::filesystem::path& path, const ShaderCompileOptions& options,
                                ReloadCallback on_reload, ErrorCallback on_error) {
    std::lock_guard<std::mutex> lock(impl_->watched_files_mutex);

    Impl::WatchedFile watched;
    watched.path = path;
    watched.options = options;
    watched.on_reload = std::move(on_reload);
    watched.on_error = std::move(on_error);

    if (std::filesystem::exists(path)) {
        watched.last_modified = std::filesystem::last_write_time(path);
    }

    impl_->watched_files[path.string()] = std::move(watched);
    impl_->hot_reload_enabled = true;

    spdlog::debug("Watching shader file: {}", path.string());
}

void ShaderCompiler::stop_watching(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(impl_->watched_files_mutex);
    impl_->watched_files.erase(path.string());
}

void ShaderCompiler::stop_watching_all() {
    std::lock_guard<std::mutex> lock(impl_->watched_files_mutex);
    impl_->watched_files.clear();
    impl_->hot_reload_enabled = false;
}

void ShaderCompiler::poll_file_changes() {
    if (!impl_->hot_reload_enabled)
        return;

    std::lock_guard<std::mutex> lock(impl_->watched_files_mutex);

    for (auto& [path_str, watched] : impl_->watched_files) {
        if (!std::filesystem::exists(watched.path))
            continue;

        auto current_modified = std::filesystem::last_write_time(watched.path);
        if (current_modified > watched.last_modified) {
            watched.last_modified = current_modified;

            spdlog::info("Recompiling modified shader: {}", path_str);
            auto result = compile_glsl_file(watched.path, watched.options);

            if (result.success) {
                if (watched.on_reload) {
                    watched.on_reload(watched.path, result);
                }
            } else {
                if (watched.on_error) {
                    watched.on_error(watched.path, result.error_message);
                }
            }
        }
    }
}

bool ShaderCompiler::is_hot_reload_enabled() const {
    return impl_->hot_reload_enabled;
}

void ShaderCompiler::set_hot_reload_enabled(bool enabled) {
    impl_->hot_reload_enabled = enabled;
}

// ============================================================================
// Utility Functions
// ============================================================================

std::optional<ShaderStage> shader_stage_from_extension(std::string_view extension) {
    if (extension == ".vert" || extension == ".vs")
        return ShaderStage::Vertex;
    if (extension == ".frag" || extension == ".fs")
        return ShaderStage::Fragment;
    if (extension == ".comp")
        return ShaderStage::Compute;
    if (extension == ".rgen")
        return ShaderStage::RayGeneration;
    if (extension == ".rmiss")
        return ShaderStage::RayMiss;
    if (extension == ".rchit")
        return ShaderStage::RayClosestHit;
    if (extension == ".rahit")
        return ShaderStage::RayAnyHit;
    if (extension == ".rint")
        return ShaderStage::RayIntersection;
    return std::nullopt;
}

const char* shader_stage_extension(ShaderStage stage) {
    switch (stage) {
        case ShaderStage::Vertex:
            return ".vert";
        case ShaderStage::Fragment:
            return ".frag";
        case ShaderStage::Compute:
            return ".comp";
        case ShaderStage::RayGeneration:
            return ".rgen";
        case ShaderStage::RayMiss:
            return ".rmiss";
        case ShaderStage::RayClosestHit:
            return ".rchit";
        case ShaderStage::RayAnyHit:
            return ".rahit";
        case ShaderStage::RayIntersection:
            return ".rint";
        default:
            return ".glsl";
    }
}

const char* shader_stage_name(ShaderStage stage) {
    switch (stage) {
        case ShaderStage::Vertex:
            return "Vertex";
        case ShaderStage::Fragment:
            return "Fragment";
        case ShaderStage::Compute:
            return "Compute";
        case ShaderStage::RayGeneration:
            return "RayGeneration";
        case ShaderStage::RayMiss:
            return "RayMiss";
        case ShaderStage::RayClosestHit:
            return "RayClosestHit";
        case ShaderStage::RayAnyHit:
            return "RayAnyHit";
        case ShaderStage::RayIntersection:
            return "RayIntersection";
        default:
            return "Unknown";
    }
}

}  // namespace realcraft::graphics
