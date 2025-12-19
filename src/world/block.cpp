// RealCraft World System
// block.cpp - BlockType implementation

#include <realcraft/world/block.hpp>

namespace realcraft::world {

// ============================================================================
// BlockType Implementation
// ============================================================================

BlockType::BlockType(BlockId id, const BlockTypeDesc& desc)
    : id_(id), name_(desc.name), display_name_(desc.display_name.empty() ? desc.name : desc.display_name),
      flags_(desc.flags), material_(desc.material), texture_index_(desc.texture_index), texture_top_(desc.texture_top),
      texture_bottom_(desc.texture_bottom), texture_side_(desc.texture_side), light_emission_(desc.light_emission),
      light_absorption_(desc.light_absorption), state_count_(desc.state_count) {
    // If texture overrides are 0, use the main texture index
    if (texture_top_ == 0) {
        texture_top_ = texture_index_;
    }
    if (texture_bottom_ == 0) {
        texture_bottom_ = texture_index_;
    }
    if (texture_side_ == 0) {
        texture_side_ = texture_index_;
    }
}

uint16_t BlockType::get_texture_index(Direction face) const {
    switch (face) {
        case Direction::PosY:
            return texture_top_;
        case Direction::NegY:
            return texture_bottom_;
        default:
            return texture_side_;
    }
}

}  // namespace realcraft::world
