// RealCraft Rendering System
// texture_manager.cpp - Block texture management

#include <realcraft/core/logger.hpp>
#include <realcraft/rendering/procedural_texture.hpp>
#include <realcraft/rendering/texture_manager.hpp>

namespace realcraft::rendering {

TextureManager::TextureManager() = default;
TextureManager::~TextureManager() = default;

bool TextureManager::initialize(graphics::GraphicsDevice* device, const TextureManagerConfig& config) {
    if (device_ != nullptr) {
        REALCRAFT_LOG_WARN(core::log_category::GRAPHICS, "TextureManager already initialized");
        return true;
    }

    device_ = device;
    config_ = config;

    // Create block atlas
    TextureAtlasDesc atlas_desc;
    atlas_desc.atlas_size = config_.atlas_size;
    atlas_desc.tile_size = config_.tile_size;
    atlas_desc.debug_name = "BlockAtlas";

    block_atlas_ = std::make_unique<TextureAtlas>(device_, atlas_desc);

    // Generate procedural textures for all registered blocks
    if (config_.generate_procedural) {
        generate_block_textures();
    }

    // Build the atlas
    if (!block_atlas_->build()) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to build block atlas");
        shutdown();
        return false;
    }

    REALCRAFT_LOG_INFO(core::log_category::GRAPHICS, "TextureManager initialized");
    return true;
}

void TextureManager::shutdown() {
    block_atlas_.reset();
    device_ = nullptr;
}

void TextureManager::generate_block_textures() {
    const auto& registry = world::BlockRegistry::instance();

    // Get all block types and generate textures
    const size_t count = registry.count();
    for (size_t i = 0; i < count; i++) {
        auto id = static_cast<world::BlockId>(i);
        const auto* block = registry.get(id);
        if (!block)
            continue;

        register_block_textures(*block);
    }
}

void TextureManager::register_block_textures(const world::BlockType& block_type) {
    const std::string& name = block_type.get_name();
    uint32_t size = config_.tile_size;

    std::vector<uint8_t> texture_data;

    // Generate appropriate texture based on block type
    if (name == "realcraft:air") {
        return;  // No texture for air
    } else if (name == "realcraft:stone") {
        texture_data = ProceduralTextureGenerator::generate_stone(size, 12345);
    } else if (name == "realcraft:dirt") {
        texture_data = ProceduralTextureGenerator::generate_dirt(size, 23456);
    } else if (name == "realcraft:grass") {
        // Grass has different top/side textures - add both
        auto top_data = ProceduralTextureGenerator::generate_grass_top(size, 34567);
        auto side_data = ProceduralTextureGenerator::generate_grass_side(size, 34568);
        auto bottom_data = ProceduralTextureGenerator::generate_dirt(size, 23456);

        (void)block_atlas_->add_texture(name + "_top", size, size, top_data);
        (void)block_atlas_->add_texture(name + "_side", size, size, side_data);
        (void)block_atlas_->add_texture(name + "_bottom", size, size, bottom_data);
        return;
    } else if (name == "realcraft:sand") {
        texture_data = ProceduralTextureGenerator::generate_sand(size, 45678);
    } else if (name == "realcraft:water") {
        texture_data = ProceduralTextureGenerator::generate_water(size, 56789);
    } else {
        // Default: checkerboard magenta/black (missing texture)
        texture_data = ProceduralTextureGenerator::generate(
            size, size, ProceduralTextureGenerator::rgba(255, 0, 255, 255),
            ProceduralTextureGenerator::rgba(0, 0, 0, 255), TexturePattern::Checkerboard);
    }

    if (!texture_data.empty()) {
        (void)block_atlas_->add_texture(name, size, size, texture_data);
    }
}

uint16_t TextureManager::get_block_texture_index(world::BlockId block_id, uint8_t face) const {
    const auto& registry = world::BlockRegistry::instance();
    const auto* block = registry.get(block_id);
    if (!block) {
        return 0;
    }

    // For grass, use different textures for different faces
    const std::string& name = block->get_name();
    if (name == "realcraft:grass") {
        std::string tex_name;
        if (face == 3) {  // PosY (top)
            tex_name = name + "_top";
        } else if (face == 2) {  // NegY (bottom)
            tex_name = name + "_bottom";
        } else {
            tex_name = name + "_side";
        }
        auto idx = block_atlas_->find_index(tex_name);
        return idx.value_or(0);
    }

    auto idx = block_atlas_->find_index(name);
    return idx.value_or(0);
}

glm::vec4 TextureManager::get_texture_uv(uint16_t texture_index) const {
    if (!block_atlas_) {
        return glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }
    return block_atlas_->get_uv_rect(texture_index);
}

}  // namespace realcraft::rendering
