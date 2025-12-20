// RealCraft Rendering System
// texture_atlas.cpp - Texture atlas implementation

#include <algorithm>
#include <realcraft/core/logger.hpp>
#include <realcraft/graphics/buffer.hpp>
#include <realcraft/graphics/command_buffer.hpp>
#include <realcraft/rendering/texture_atlas.hpp>
#include <unordered_map>

namespace realcraft::rendering {

TextureAtlas::TextureAtlas(graphics::GraphicsDevice* device, const TextureAtlasDesc& desc)
    : device_(device), desc_(desc) {}

TextureAtlas::~TextureAtlas() = default;

uint16_t TextureAtlas::add_texture(std::string_view name, uint32_t width, uint32_t height,
                                   std::span<const uint8_t> rgba_data) {
    if (is_built_) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Cannot add texture after atlas is built: {}", name);
        return UINT16_MAX;
    }

    if (rgba_data.size() != static_cast<size_t>(width) * height * 4) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Invalid texture data size for {}: expected {}, got {}", name,
                            width * height * 4, rgba_data.size());
        return UINT16_MAX;
    }

    StagedTexture tex;
    tex.name = std::string(name);
    tex.width = width;
    tex.height = height;
    tex.data.assign(rgba_data.begin(), rgba_data.end());

    uint16_t index = static_cast<uint16_t>(staged_textures_.size());
    name_to_index_[tex.name] = index;
    staged_textures_.push_back(std::move(tex));

    return index;
}

uint16_t TextureAtlas::add_solid_color(std::string_view name, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    std::vector<uint8_t> data(static_cast<size_t>(desc_.tile_size) * desc_.tile_size * 4);
    for (size_t i = 0; i < data.size(); i += 4) {
        data[i + 0] = r;
        data[i + 1] = g;
        data[i + 2] = b;
        data[i + 3] = a;
    }
    return add_texture(name, desc_.tile_size, desc_.tile_size, data);
}

bool TextureAtlas::pack_texture(const StagedTexture& tex, TextureAtlasEntry& out_entry) {
    uint32_t padded_w = tex.width + desc_.padding * 2;
    uint32_t padded_h = tex.height + desc_.padding * 2;

    // Check if fits in current row
    if (current_x_ + padded_w > desc_.atlas_size) {
        // Move to next row
        current_x_ = 0;
        current_y_ += row_height_;
        row_height_ = 0;
    }

    // Check if fits in current layer
    if (current_y_ + padded_h > desc_.atlas_size) {
        // Move to next layer
        current_layer_++;
        current_x_ = 0;
        current_y_ = 0;
        row_height_ = 0;

        if (current_layer_ >= desc_.max_layers) {
            REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Atlas full, cannot pack: {}", tex.name);
            return false;
        }
    }

    // Pack at current position (accounting for padding)
    out_entry.u_min = static_cast<float>(current_x_ + desc_.padding) / static_cast<float>(desc_.atlas_size);
    out_entry.v_min = static_cast<float>(current_y_ + desc_.padding) / static_cast<float>(desc_.atlas_size);
    out_entry.u_max = static_cast<float>(current_x_ + desc_.padding + tex.width) / static_cast<float>(desc_.atlas_size);
    out_entry.v_max =
        static_cast<float>(current_y_ + desc_.padding + tex.height) / static_cast<float>(desc_.atlas_size);
    out_entry.atlas_layer = static_cast<uint16_t>(current_layer_);
    out_entry.width = static_cast<uint16_t>(tex.width);
    out_entry.height = static_cast<uint16_t>(tex.height);

    // Advance position
    current_x_ += padded_w;
    row_height_ = std::max(row_height_, padded_h);

    return true;
}

bool TextureAtlas::build() {
    if (is_built_) {
        REALCRAFT_LOG_WARN(core::log_category::GRAPHICS, "Atlas already built");
        return true;
    }

    if (staged_textures_.empty()) {
        REALCRAFT_LOG_WARN(core::log_category::GRAPHICS, "No textures to pack in atlas");
        return false;
    }

    // Reset packing state
    current_layer_ = 0;
    current_x_ = 0;
    current_y_ = 0;
    row_height_ = 0;

    // Pack all textures
    entries_.clear();
    entries_.reserve(staged_textures_.size());

    for (size_t i = 0; i < staged_textures_.size(); i++) {
        TextureAtlasEntry entry;
        entry.index = static_cast<uint16_t>(i);

        if (!pack_texture(staged_textures_[i], entry)) {
            return false;
        }

        entries_.push_back(entry);
    }

    uint32_t num_layers = current_layer_ + 1;

    // Create atlas texture
    graphics::TextureDesc tex_desc;
    tex_desc.type = graphics::TextureType::Texture2DArray;
    tex_desc.format = graphics::TextureFormat::RGBA8UnormSrgb;
    tex_desc.width = desc_.atlas_size;
    tex_desc.height = desc_.atlas_size;
    tex_desc.array_layers = num_layers;
    tex_desc.mip_levels = desc_.generate_mipmaps ? 1 : 1;  // Simplified: 1 mip for now
    tex_desc.usage = graphics::TextureUsage::Sampled | graphics::TextureUsage::TransferDst;
    tex_desc.debug_name = desc_.debug_name;

    atlas_texture_ = device_->create_texture(tex_desc);
    if (!atlas_texture_) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create atlas texture");
        return false;
    }

    // Create atlas data and copy textures
    std::vector<uint8_t> atlas_data(static_cast<size_t>(desc_.atlas_size) * desc_.atlas_size * 4 * num_layers, 0);

    for (size_t i = 0; i < staged_textures_.size(); i++) {
        const auto& tex = staged_textures_[i];
        const auto& entry = entries_[i];

        uint32_t dst_x = static_cast<uint32_t>(entry.u_min * desc_.atlas_size);
        uint32_t dst_y = static_cast<uint32_t>(entry.v_min * desc_.atlas_size);
        uint32_t layer = entry.atlas_layer;

        // Copy texture data row by row
        for (uint32_t row = 0; row < tex.height; row++) {
            size_t src_offset = static_cast<size_t>(row) * tex.width * 4;
            size_t dst_offset =
                (layer * desc_.atlas_size * desc_.atlas_size + (dst_y + row) * desc_.atlas_size + dst_x) * 4;

            std::memcpy(&atlas_data[dst_offset], &tex.data[src_offset], tex.width * 4);
        }
    }

    // Upload to GPU via staging buffer
    graphics::BufferDesc staging_desc;
    staging_desc.size = atlas_data.size();
    staging_desc.usage = graphics::BufferUsage::TransferSrc;
    staging_desc.host_visible = true;
    staging_desc.debug_name = "AtlasStagingBuffer";

    auto staging_buffer = device_->create_buffer(staging_desc);
    if (!staging_buffer) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create staging buffer");
        return false;
    }

    staging_buffer->write(atlas_data.data(), atlas_data.size());

    // Copy to texture
    auto cmd = device_->create_command_buffer();
    cmd->begin();

    for (uint32_t layer = 0; layer < num_layers; layer++) {
        graphics::BufferImageCopy region;
        region.buffer_offset = static_cast<size_t>(layer) * desc_.atlas_size * desc_.atlas_size * 4;
        region.buffer_row_length = desc_.atlas_size;
        region.buffer_image_height = desc_.atlas_size;
        region.texture_offset_x = 0;
        region.texture_offset_y = 0;
        region.texture_offset_z = 0;
        region.texture_width = desc_.atlas_size;
        region.texture_height = desc_.atlas_size;
        region.texture_depth = 1;
        region.mip_level = 0;
        region.array_layer = layer;

        cmd->copy_buffer_to_texture(staging_buffer.get(), atlas_texture_.get(), region);
    }

    cmd->end();
    device_->submit(cmd.get(), true);  // Wait for completion

    // Create sampler
    graphics::SamplerDesc sampler_desc;
    sampler_desc.min_filter = graphics::FilterMode::Nearest;
    sampler_desc.mag_filter = graphics::FilterMode::Nearest;
    sampler_desc.mip_filter = graphics::FilterMode::Nearest;
    sampler_desc.address_u = graphics::AddressMode::ClampToEdge;
    sampler_desc.address_v = graphics::AddressMode::ClampToEdge;
    sampler_desc.debug_name = desc_.debug_name + "_Sampler";

    atlas_sampler_ = device_->create_sampler(sampler_desc);
    if (!atlas_sampler_) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create atlas sampler");
        return false;
    }

    // Clear staged data
    staged_textures_.clear();
    staged_textures_.shrink_to_fit();

    is_built_ = true;

    REALCRAFT_LOG_INFO(core::log_category::GRAPHICS, "Built texture atlas: {} textures, {} layers", entries_.size(),
                       num_layers);

    return true;
}

const TextureAtlasEntry* TextureAtlas::get_entry(uint16_t index) const {
    if (index >= entries_.size()) {
        return nullptr;
    }
    return &entries_[index];
}

const TextureAtlasEntry* TextureAtlas::get_entry(std::string_view name) const {
    auto idx = find_index(name);
    if (!idx) {
        return nullptr;
    }
    return get_entry(*idx);
}

std::optional<uint16_t> TextureAtlas::find_index(std::string_view name) const {
    auto it = name_to_index_.find(std::string(name));
    if (it != name_to_index_.end()) {
        return it->second;
    }
    return std::nullopt;
}

glm::vec4 TextureAtlas::get_uv_rect(uint16_t index) const {
    const auto* entry = get_entry(index);
    if (!entry) {
        return glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }

    // Inset slightly to prevent bleeding
    float inset = 0.001f;
    return glm::vec4(entry->u_min + inset, entry->v_min + inset, entry->u_max - inset, entry->v_max - inset);
}

}  // namespace realcraft::rendering
