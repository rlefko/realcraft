// RealCraft Rendering System
// hud_renderer.hpp - HUD and UI rendering

#pragma once

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <realcraft/graphics/buffer.hpp>
#include <realcraft/graphics/device.hpp>
#include <realcraft/graphics/pipeline.hpp>
#include <realcraft/graphics/sampler.hpp>
#include <realcraft/graphics/shader.hpp>
#include <realcraft/graphics/texture.hpp>
#include <string>

namespace realcraft::rendering {

// Forward declarations
class TextureAtlas;

// ============================================================================
// HUD Configuration
// ============================================================================

enum class CrosshairStyle : uint8_t {
    Dot,    // Simple dot
    Cross,  // Four lines from center
    Plus,   // Plus sign (filled quads)
};

struct HUDConfig {
    // Crosshair settings
    CrosshairStyle crosshair_style = CrosshairStyle::Cross;
    float crosshair_size = 10.0f;
    float crosshair_thickness = 2.0f;
    float crosshair_gap = 3.0f;  // Gap from center for Cross style
    glm::vec4 crosshair_color = {1.0f, 1.0f, 1.0f, 0.8f};

    // Hotbar settings
    float hotbar_slot_size = 40.0f;
    float hotbar_slot_spacing = 4.0f;
    float hotbar_icon_padding = 4.0f;
    float hotbar_bottom_margin = 20.0f;
    glm::vec4 hotbar_slot_color = {0.2f, 0.2f, 0.2f, 0.7f};
    glm::vec4 hotbar_selected_color = {0.8f, 0.8f, 0.2f, 0.9f};

    // Health/hunger bar settings
    float bar_width = 82.0f;   // Width of each bar
    float bar_height = 10.0f;  // Height of bars
    float bar_spacing = 10.0f;
    float bar_bottom_margin = 8.0f;  // Above hotbar
    glm::vec4 health_bar_color = {0.8f, 0.2f, 0.2f, 0.9f};
    glm::vec4 hunger_bar_color = {0.6f, 0.4f, 0.2f, 0.9f};
    glm::vec4 bar_background_color = {0.1f, 0.1f, 0.1f, 0.7f};

    // Debug overlay settings
    bool debug_overlay_visible = false;
    float debug_font_scale = 1.0f;
    float debug_line_height = 16.0f;
    float debug_margin = 10.0f;
    glm::vec4 debug_text_color = {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 debug_shadow_color = {0.0f, 0.0f, 0.0f, 0.8f};

    // UI scale (for HiDPI)
    float ui_scale = 1.0f;
};

// ============================================================================
// HUD Data (per-frame input)
// ============================================================================

struct HotbarSlotData {
    bool empty = true;
    uint16_t texture_index = 0;
    uint16_t count = 0;
    uint16_t durability = 0;      // Current durability
    uint16_t max_durability = 0;  // Max durability (0 = not a tool)
};

struct HUDData {
    // Hotbar (from Inventory)
    std::array<HotbarSlotData, 9> hotbar_slots;
    int selected_slot = 0;

    // Player stats (from PlayerStats)
    float health = 20.0f;
    float max_health = 20.0f;
    float hunger = 20.0f;
    float max_hunger = 20.0f;

    // Debug info
    double fps = 0.0;
    double frame_time_ms = 0.0;
    glm::dvec3 player_position{0.0};
    int32_t chunk_x = 0;
    int32_t chunk_z = 0;
    float camera_pitch = 0.0f;
    float camera_yaw = 0.0f;
    uint32_t chunks_rendered = 0;
    uint32_t chunks_culled = 0;
    uint32_t triangles = 0;
    uint32_t draw_calls = 0;
    float time_of_day = 0.0f;   // 0.0 = midnight, 0.5 = noon, 1.0 = midnight
    uint64_t total_memory = 0;  // Bytes
    uint64_t used_memory = 0;   // Bytes
};

// ============================================================================
// HUD Renderer
// ============================================================================

class HUDRenderer {
public:
    HUDRenderer();
    ~HUDRenderer();

    // Non-copyable
    HUDRenderer(const HUDRenderer&) = delete;
    HUDRenderer& operator=(const HUDRenderer&) = delete;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    bool initialize(graphics::GraphicsDevice* device, TextureAtlas* block_atlas = nullptr,
                    const HUDConfig& config = {});
    void shutdown();
    [[nodiscard]] bool is_initialized() const { return device_ != nullptr; }

    // ========================================================================
    // Update
    // ========================================================================

    // Update HUD data (call each frame before render)
    void update(const HUDData& data);

    // ========================================================================
    // Render
    // ========================================================================

    // Render HUD elements
    void render(graphics::CommandBuffer* cmd, uint32_t screen_width, uint32_t screen_height);

    // ========================================================================
    // Configuration
    // ========================================================================

    [[nodiscard]] HUDConfig& get_config() { return config_; }
    [[nodiscard]] const HUDConfig& get_config() const { return config_; }

    void set_debug_visible(bool visible) { config_.debug_overlay_visible = visible; }
    [[nodiscard]] bool is_debug_visible() const { return config_.debug_overlay_visible; }

    void set_crosshair_style(CrosshairStyle style) { config_.crosshair_style = style; }
    [[nodiscard]] CrosshairStyle get_crosshair_style() const { return config_.crosshair_style; }

private:
    graphics::GraphicsDevice* device_ = nullptr;
    TextureAtlas* block_atlas_ = nullptr;

    HUDConfig config_;
    HUDData data_;

    // ========================================================================
    // GPU Resources
    // ========================================================================

    // Shaders
    std::unique_ptr<graphics::Shader> ui_vertex_shader_;
    std::unique_ptr<graphics::Shader> ui_fragment_shader_;

    // Pipelines
    std::unique_ptr<graphics::Pipeline> ui_pipeline_;

    // Font texture and sampler
    std::unique_ptr<graphics::Texture> font_texture_;
    std::unique_ptr<graphics::Sampler> font_sampler_;

    // Dynamic vertex buffer for UI quads
    std::unique_ptr<graphics::Buffer> ui_vertex_buffer_;
    static constexpr size_t MAX_UI_VERTICES = 4096;

    // ========================================================================
    // Vertex Format
    // ========================================================================

    struct UIVertex {
        glm::vec2 position;  // Screen-space position
        glm::vec2 uv;        // Texture coordinates
        glm::vec4 color;     // Vertex color

        static std::vector<graphics::VertexAttribute> get_attributes() {
            return {
                {0, 0, graphics::TextureFormat::RG32Float, 0},                       // position
                {1, 0, graphics::TextureFormat::RG32Float, sizeof(glm::vec2)},       // uv
                {2, 0, graphics::TextureFormat::RGBA32Float, 2 * sizeof(glm::vec2)}  // color
            };
        }

        static graphics::VertexBinding get_binding() { return {0, sizeof(UIVertex), false}; }
    };

    // Batch vertex data
    std::vector<UIVertex> vertices_;

    // ========================================================================
    // Push Constants
    // ========================================================================

    struct UIPushConstants {
        glm::vec2 screen_size;
        float use_texture;  // 0 = solid color, 1 = textured
        float texture_layer;
    };

    // ========================================================================
    // Initialization Helpers
    // ========================================================================

    bool create_shaders();
    bool create_pipeline();
    bool create_font_texture();
    bool create_vertex_buffer();

    // ========================================================================
    // Rendering Helpers
    // ========================================================================

    void render_crosshair(uint32_t width, uint32_t height);
    void render_hotbar(uint32_t width, uint32_t height);
    void render_health_hunger(uint32_t width, uint32_t height);
    void render_debug_overlay(uint32_t width, uint32_t height);

    // ========================================================================
    // Geometry Helpers
    // ========================================================================

    // Add a colored quad (no texture)
    void add_quad(float x, float y, float w, float h, const glm::vec4& color);

    // Add a textured quad
    void add_textured_quad(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                           const glm::vec4& color = {1.0f, 1.0f, 1.0f, 1.0f});

    // ========================================================================
    // Text Rendering
    // ========================================================================

    // Render text at position (top-left origin)
    void render_text(const std::string& text, float x, float y, const glm::vec4& color, float scale = 1.0f);

    // Render text with drop shadow
    void render_text_shadowed(const std::string& text, float x, float y, const glm::vec4& color,
                              const glm::vec4& shadow_color, float scale = 1.0f);

    // Get text width in pixels
    [[nodiscard]] float get_text_width(const std::string& text, float scale = 1.0f) const;

    // Font metrics
    static constexpr int FONT_CHAR_WIDTH = 8;
    static constexpr int FONT_CHAR_HEIGHT = 16;
    static constexpr int FONT_TEXTURE_SIZE = 256;
    static constexpr int FONT_CHARS_PER_ROW = FONT_TEXTURE_SIZE / FONT_CHAR_WIDTH;

    // Get UV coords for a character
    [[nodiscard]] glm::vec4 get_char_uv(char c) const;

    // ========================================================================
    // Batch Rendering
    // ========================================================================

    void begin_batch();
    void flush_batch(graphics::CommandBuffer* cmd, bool use_texture, graphics::Texture* texture = nullptr);
};

}  // namespace realcraft::rendering
