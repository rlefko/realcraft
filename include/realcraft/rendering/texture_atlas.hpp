// RealCraft Rendering System
// texture_atlas.hpp - Block texture atlas management

#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <realcraft/graphics/device.hpp>
#include <realcraft/graphics/sampler.hpp>
#include <realcraft/graphics/texture.hpp>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace realcraft::rendering {

// Information about a texture in the atlas
struct TextureAtlasEntry {
    uint16_t index;        // Unique texture index
    uint16_t atlas_layer;  // Layer in array texture

    // UV rectangle in normalized coordinates [0,1]
    float u_min;
    float v_min;
    float u_max;
    float v_max;

    // Original size in pixels
    uint16_t width;
    uint16_t height;
};

// Atlas configuration
struct TextureAtlasDesc {
    uint32_t atlas_size = 512;  // Width/height of atlas texture
    uint32_t tile_size = 16;    // Standard block texture size
    uint32_t padding = 2;       // Padding between tiles
    uint32_t max_layers = 4;    // Max array layers
    bool generate_mipmaps = true;
    std::string debug_name = "BlockAtlas";
};

// Manages a texture atlas for block textures
class TextureAtlas {
public:
    explicit TextureAtlas(graphics::GraphicsDevice* device, const TextureAtlasDesc& desc = {});
    ~TextureAtlas();

    // Non-copyable
    TextureAtlas(const TextureAtlas&) = delete;
    TextureAtlas& operator=(const TextureAtlas&) = delete;

    // ========================================================================
    // Texture Registration
    // ========================================================================

    // Add texture from RGBA pixel data
    // Returns texture index, or UINT16_MAX on failure
    [[nodiscard]] uint16_t add_texture(std::string_view name, uint32_t width, uint32_t height,
                                       std::span<const uint8_t> rgba_data);

    // Add a solid color texture
    [[nodiscard]] uint16_t add_solid_color(std::string_view name, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);

    // ========================================================================
    // Atlas Building
    // ========================================================================

    // Build the atlas texture (must call after adding all textures)
    bool build();

    // Check if atlas is built
    [[nodiscard]] bool is_built() const { return is_built_; }

    // ========================================================================
    // Lookup
    // ========================================================================

    [[nodiscard]] const TextureAtlasEntry* get_entry(uint16_t index) const;
    [[nodiscard]] const TextureAtlasEntry* get_entry(std::string_view name) const;
    [[nodiscard]] std::optional<uint16_t> find_index(std::string_view name) const;

    // Get UV rect with safe inset to prevent bleeding
    // Returns (u_min, v_min, u_max, v_max)
    [[nodiscard]] glm::vec4 get_uv_rect(uint16_t index) const;

    // ========================================================================
    // GPU Resources
    // ========================================================================

    [[nodiscard]] graphics::Texture* get_texture() const { return atlas_texture_.get(); }
    [[nodiscard]] graphics::Sampler* get_sampler() const { return atlas_sampler_.get(); }

    // ========================================================================
    // Info
    // ========================================================================

    [[nodiscard]] uint32_t get_atlas_size() const { return desc_.atlas_size; }
    [[nodiscard]] uint32_t get_tile_size() const { return desc_.tile_size; }
    [[nodiscard]] uint32_t get_layer_count() const { return current_layer_ + 1; }
    [[nodiscard]] size_t get_texture_count() const { return entries_.size(); }

private:
    graphics::GraphicsDevice* device_;
    TextureAtlasDesc desc_;

    // Staging data before build
    struct StagedTexture {
        std::string name;
        uint32_t width;
        uint32_t height;
        std::vector<uint8_t> data;
    };
    std::vector<StagedTexture> staged_textures_;

    // Built atlas data
    std::vector<TextureAtlasEntry> entries_;
    std::unordered_map<std::string, uint16_t> name_to_index_;
    std::unique_ptr<graphics::Texture> atlas_texture_;
    std::unique_ptr<graphics::Sampler> atlas_sampler_;

    bool is_built_ = false;
    uint32_t current_layer_ = 0;
    uint32_t current_x_ = 0;
    uint32_t current_y_ = 0;
    uint32_t row_height_ = 0;

    // Pack a texture into the atlas
    bool pack_texture(const StagedTexture& tex, TextureAtlasEntry& out_entry);
};

}  // namespace realcraft::rendering
