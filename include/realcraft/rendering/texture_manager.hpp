// RealCraft Rendering System
// texture_manager.hpp - Manages block textures and atlas

#pragma once

#include "texture_atlas.hpp"

#include <cstdint>
#include <memory>
#include <realcraft/graphics/device.hpp>
#include <realcraft/world/block.hpp>

namespace realcraft::rendering {

// Configuration for texture manager
struct TextureManagerConfig {
    uint32_t atlas_size = 512;
    uint32_t tile_size = 16;
    bool generate_procedural = true;  // Generate placeholder textures
};

// Manages block textures
class TextureManager {
public:
    TextureManager();
    ~TextureManager();

    // Non-copyable
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    // Initialize with graphics device
    bool initialize(graphics::GraphicsDevice* device, const TextureManagerConfig& config = {});

    // Shutdown and release resources
    void shutdown();

    // Check if initialized
    [[nodiscard]] bool is_initialized() const { return device_ != nullptr; }

    // ========================================================================
    // Block Texture Access
    // ========================================================================

    // Get the block texture atlas
    [[nodiscard]] TextureAtlas* get_block_atlas() const { return block_atlas_.get(); }

    // Get texture index for a block face
    // face: 0=negX, 1=posX, 2=negY, 3=posY, 4=negZ, 5=posZ
    [[nodiscard]] uint16_t get_block_texture_index(world::BlockId block_id, uint8_t face) const;

    // Get UV rect for a texture index
    [[nodiscard]] glm::vec4 get_texture_uv(uint16_t texture_index) const;

private:
    graphics::GraphicsDevice* device_ = nullptr;
    TextureManagerConfig config_;

    std::unique_ptr<TextureAtlas> block_atlas_;

    // Generate procedural textures for all registered blocks
    void generate_block_textures();

    // Register a single block's textures
    void register_block_textures(const world::BlockType& block_type);
};

}  // namespace realcraft::rendering
