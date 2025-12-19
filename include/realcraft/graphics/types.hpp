// RealCraft Graphics Abstraction Layer
// types.hpp - Common types, enums, and descriptors

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace realcraft::graphics {

// ============================================================================
// Texture Formats
// ============================================================================

enum class TextureFormat : uint32_t {
    Unknown = 0,

    // 8-bit formats
    R8Unorm,
    R8Snorm,
    R8Uint,
    R8Sint,
    RG8Unorm,
    RG8Snorm,
    RG8Uint,
    RG8Sint,
    RGBA8Unorm,
    RGBA8Snorm,
    RGBA8Uint,
    RGBA8Sint,
    RGBA8UnormSrgb,
    BGRA8Unorm,
    BGRA8UnormSrgb,

    // 16-bit formats
    R16Float,
    R16Uint,
    R16Sint,
    RG16Float,
    RG16Uint,
    RG16Sint,
    RGBA16Float,
    RGBA16Uint,
    RGBA16Sint,

    // 32-bit formats
    R32Float,
    R32Uint,
    R32Sint,
    RG32Float,
    RG32Uint,
    RG32Sint,
    RGBA32Float,
    RGBA32Uint,
    RGBA32Sint,

    // Depth formats
    Depth16Unorm,
    Depth24UnormStencil8,
    Depth32Float,
    Depth32FloatStencil8,
};

// ============================================================================
// Buffer Usage Flags
// ============================================================================

enum class BufferUsage : uint32_t {
    None = 0,
    Vertex = 1 << 0,
    Index = 1 << 1,
    Uniform = 1 << 2,
    Storage = 1 << 3,
    Indirect = 1 << 4,
    TransferSrc = 1 << 5,
    TransferDst = 1 << 6,
};

inline BufferUsage operator|(BufferUsage a, BufferUsage b) {
    return static_cast<BufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline BufferUsage operator&(BufferUsage a, BufferUsage b) {
    return static_cast<BufferUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool has_flag(BufferUsage flags, BufferUsage flag) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

// ============================================================================
// Texture Usage Flags
// ============================================================================

enum class TextureUsage : uint32_t {
    None = 0,
    Sampled = 1 << 0,
    Storage = 1 << 1,
    RenderTarget = 1 << 2,
    DepthStencil = 1 << 3,
    TransferSrc = 1 << 4,
    TransferDst = 1 << 5,
};

inline TextureUsage operator|(TextureUsage a, TextureUsage b) {
    return static_cast<TextureUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline TextureUsage operator&(TextureUsage a, TextureUsage b) {
    return static_cast<TextureUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool has_flag(TextureUsage flags, TextureUsage flag) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

// ============================================================================
// Texture Types
// ============================================================================

enum class TextureType : uint8_t {
    Texture1D,
    Texture2D,
    Texture3D,
    TextureCube,
    Texture2DArray,
};

// ============================================================================
// Shader Stages
// ============================================================================

enum class ShaderStage : uint8_t {
    Vertex,
    Fragment,
    Compute,
    // Ray tracing stages (future)
    RayGeneration,
    RayMiss,
    RayClosestHit,
    RayAnyHit,
    RayIntersection,
};

// ============================================================================
// Rendering State Enums
// ============================================================================

enum class PrimitiveTopology : uint8_t {
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
};

enum class CullMode : uint8_t {
    None,
    Front,
    Back,
};

enum class FrontFace : uint8_t {
    CounterClockwise,
    Clockwise,
};

enum class CompareOp : uint8_t {
    Never,
    Less,
    Equal,
    LessOrEqual,
    Greater,
    NotEqual,
    GreaterOrEqual,
    Always,
};

enum class BlendFactor : uint8_t {
    Zero,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    ConstantColor,
    OneMinusConstantColor,
    SrcAlphaSaturate,
};

enum class BlendOp : uint8_t {
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
};

enum class LoadOp : uint8_t {
    Load,
    Clear,
    DontCare,
};

enum class StoreOp : uint8_t {
    Store,
    DontCare,
};

enum class IndexType : uint8_t {
    Uint16,
    Uint32,
};

enum class FilterMode : uint8_t {
    Nearest,
    Linear,
};

enum class AddressMode : uint8_t {
    Repeat,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder,
};

// ============================================================================
// Descriptors / Configuration Structs
// ============================================================================

struct BufferDesc {
    size_t size = 0;
    BufferUsage usage = BufferUsage::None;
    bool host_visible = false;
    const void* initial_data = nullptr;
    std::string debug_name;
};

struct TextureDesc {
    TextureType type = TextureType::Texture2D;
    TextureFormat format = TextureFormat::RGBA8Unorm;
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth = 1;
    uint32_t mip_levels = 1;
    uint32_t array_layers = 1;
    TextureUsage usage = TextureUsage::Sampled;
    std::string debug_name;
};

struct SamplerDesc {
    FilterMode min_filter = FilterMode::Linear;
    FilterMode mag_filter = FilterMode::Linear;
    FilterMode mip_filter = FilterMode::Linear;
    AddressMode address_u = AddressMode::Repeat;
    AddressMode address_v = AddressMode::Repeat;
    AddressMode address_w = AddressMode::Repeat;
    float max_anisotropy = 1.0f;
    CompareOp compare_op = CompareOp::Never;
    bool compare_enable = false;
    float mip_lod_bias = 0.0f;
    float min_lod = 0.0f;
    float max_lod = 1000.0f;
    std::string debug_name;
};

struct ShaderDesc {
    ShaderStage stage = ShaderStage::Vertex;
    std::span<const uint8_t> bytecode;
    std::string entry_point = "main";
    std::string debug_name;
};

// ============================================================================
// Vertex Input
// ============================================================================

struct VertexAttribute {
    uint32_t location = 0;
    uint32_t binding = 0;
    TextureFormat format = TextureFormat::Unknown;  // Reuse format enum
    uint32_t offset = 0;
};

struct VertexBinding {
    uint32_t binding = 0;
    uint32_t stride = 0;
    bool per_instance = false;
};

// ============================================================================
// Pipeline State
// ============================================================================

struct RasterizerState {
    CullMode cull_mode = CullMode::Back;
    FrontFace front_face = FrontFace::CounterClockwise;
    bool depth_clamp_enable = false;
    bool wireframe = false;
    float depth_bias = 0.0f;
    float depth_bias_slope = 0.0f;
    float depth_bias_clamp = 0.0f;
};

struct DepthStencilState {
    bool depth_test_enable = true;
    bool depth_write_enable = true;
    CompareOp depth_compare = CompareOp::Less;
    bool stencil_test_enable = false;
};

struct BlendState {
    bool enable = false;
    BlendFactor src_color = BlendFactor::One;
    BlendFactor dst_color = BlendFactor::Zero;
    BlendOp color_op = BlendOp::Add;
    BlendFactor src_alpha = BlendFactor::One;
    BlendFactor dst_alpha = BlendFactor::Zero;
    BlendOp alpha_op = BlendOp::Add;
    uint8_t color_write_mask = 0xF;  // RGBA
};

// ============================================================================
// Render Pass / Framebuffer
// ============================================================================

struct AttachmentDesc {
    TextureFormat format = TextureFormat::BGRA8Unorm;
    LoadOp load_op = LoadOp::Clear;
    StoreOp store_op = StoreOp::Store;
    LoadOp stencil_load_op = LoadOp::DontCare;
    StoreOp stencil_store_op = StoreOp::DontCare;
};

struct RenderPassDesc {
    std::vector<AttachmentDesc> color_attachments;
    std::optional<AttachmentDesc> depth_attachment;
    std::string debug_name;
};

// Forward declarations needed for FramebufferDesc
class Texture;
class RenderPass;

struct FramebufferDesc {
    RenderPass* render_pass = nullptr;
    std::vector<Texture*> color_attachments;
    Texture* depth_attachment = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    std::string debug_name;
};

// ============================================================================
// Pipeline Descriptors
// ============================================================================

// Forward declarations
class Shader;

struct PipelineDesc {
    Shader* vertex_shader = nullptr;
    Shader* fragment_shader = nullptr;
    std::vector<VertexAttribute> vertex_attributes;
    std::vector<VertexBinding> vertex_bindings;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    RasterizerState rasterizer;
    DepthStencilState depth_stencil;
    std::vector<BlendState> color_blend;
    std::vector<TextureFormat> color_formats;
    TextureFormat depth_format = TextureFormat::Unknown;
    std::string debug_name;
};

struct ComputePipelineDesc {
    Shader* compute_shader = nullptr;
    std::string debug_name;
};

// ============================================================================
// Geometry / Viewport Types
// ============================================================================

struct Viewport {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float min_depth = 0.0f;
    float max_depth = 1.0f;
};

struct Rect {
    int32_t x = 0;
    int32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct ClearValue {
    union {
        struct {
            float r, g, b, a;
        } color;
        struct {
            float depth;
            uint8_t stencil;
        } depth_stencil;
    };

    static ClearValue Color(float r, float g, float b, float a = 1.0f) {
        ClearValue v;
        v.color = {r, g, b, a};
        return v;
    }

    static ClearValue DepthStencil(float depth, uint8_t stencil = 0) {
        ClearValue v;
        v.depth_stencil = {depth, stencil};
        return v;
    }
};

// ============================================================================
// Copy / Transfer Types
// ============================================================================

struct BufferImageCopy {
    size_t buffer_offset = 0;
    uint32_t buffer_row_length = 0;    // 0 = tightly packed
    uint32_t buffer_image_height = 0;  // 0 = tightly packed
    uint32_t texture_offset_x = 0;
    uint32_t texture_offset_y = 0;
    uint32_t texture_offset_z = 0;
    uint32_t texture_width = 0;
    uint32_t texture_height = 0;
    uint32_t texture_depth = 1;
    uint32_t mip_level = 0;
    uint32_t array_layer = 0;
};

// ============================================================================
// Device Capabilities
// ============================================================================

struct DeviceCapabilities {
    std::string device_name;
    std::string api_name;  // "Metal", "Vulkan", "DirectX 12"
    uint64_t dedicated_video_memory = 0;
    uint64_t max_buffer_size = 0;
    uint32_t max_texture_size_2d = 0;
    uint32_t max_texture_size_3d = 0;
    uint32_t max_texture_array_layers = 0;
    uint32_t max_uniform_buffer_size = 0;
    uint32_t max_storage_buffer_size = 0;
    uint32_t max_compute_work_group_count[3] = {0, 0, 0};
    uint32_t max_compute_work_group_size[3] = {0, 0, 0};
    uint32_t max_compute_work_group_invocations = 0;
    bool supports_ray_tracing = false;
    bool supports_compute = true;
    bool supports_timestamp_query = false;
};

// ============================================================================
// Shader Reflection Types
// ============================================================================

struct ShaderUniformMember {
    std::string name;
    std::string type_name;  // "mat4", "vec3", "float", etc.
    size_t offset = 0;
    size_t size = 0;
    uint32_t array_size = 1;  // 1 for non-arrays
};

struct ShaderUniformBuffer {
    std::string name;
    uint32_t set = 0;
    uint32_t binding = 0;
    size_t size = 0;
    std::vector<ShaderUniformMember> members;
};

struct ShaderSampledImage {
    std::string name;
    uint32_t set = 0;
    uint32_t binding = 0;
    TextureType dimension = TextureType::Texture2D;
    bool is_array = false;
};

struct ShaderStorageBuffer {
    std::string name;
    uint32_t set = 0;
    uint32_t binding = 0;
    size_t size = 0;  // 0 for runtime-sized arrays
};

struct ShaderPushConstant {
    size_t offset = 0;
    size_t size = 0;
    ShaderStage stages = ShaderStage::Vertex;
    std::vector<ShaderUniformMember> members;
};

struct ShaderStageInput {
    std::string name;
    uint32_t location = 0;
    std::string type_name;
};

struct ShaderReflection {
    ShaderStage stage = ShaderStage::Vertex;
    std::string entry_point;
    std::vector<ShaderUniformBuffer> uniform_buffers;
    std::vector<ShaderSampledImage> sampled_images;
    std::vector<ShaderStorageBuffer> storage_buffers;
    std::vector<ShaderPushConstant> push_constants;
    std::vector<ShaderStageInput> inputs;
    std::vector<ShaderStageInput> outputs;
};

}  // namespace realcraft::graphics
