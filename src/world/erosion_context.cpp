// RealCraft World System
// erosion_context.cpp - Cross-chunk erosion context implementation

#include <realcraft/world/erosion_context.hpp>

namespace realcraft::world {

// ============================================================================
// Border Data Management
// ============================================================================

void ErosionContext::submit_border_data(const ErosionBorderData& data) {
    std::unique_lock lock(mutex_);

    // Ensure the chunk entry exists
    auto& chunk_borders = borders_[data.source_chunk];

    // Store the border data for the specified direction
    chunk_borders[static_cast<size_t>(data.direction)] = data;
}

std::optional<ErosionBorderData> ErosionContext::get_neighbor_border(const ChunkPos& chunk,
                                                                     HorizontalDirection direction) const {
    std::shared_lock lock(mutex_);

    // We want the border data FROM the neighbor chunk
    // If we're looking in the PosX direction, we want the neighbor at chunk + (1, 0)
    // That neighbor exported its NegX border (opposite direction)
    ChunkPos neighbor = neighbor_chunk(chunk, direction);
    HorizontalDirection opposite = opposite_direction(direction);

    auto it = borders_.find(neighbor);
    if (it == borders_.end()) {
        return std::nullopt;
    }

    return it->second[static_cast<size_t>(opposite)];
}

bool ErosionContext::has_neighbor_data(const ChunkPos& chunk, HorizontalDirection direction) const {
    std::shared_lock lock(mutex_);

    ChunkPos neighbor = neighbor_chunk(chunk, direction);
    HorizontalDirection opposite = opposite_direction(direction);

    auto it = borders_.find(neighbor);
    if (it == borders_.end()) {
        return false;
    }

    return it->second[static_cast<size_t>(opposite)].has_value();
}

// ============================================================================
// Memory Management
// ============================================================================

void ErosionContext::clear_chunk_data(const ChunkPos& chunk) {
    std::unique_lock lock(mutex_);
    borders_.erase(chunk);
}

void ErosionContext::clear_all() {
    std::unique_lock lock(mutex_);
    borders_.clear();
}

size_t ErosionContext::stored_chunk_count() const {
    std::shared_lock lock(mutex_);
    return borders_.size();
}

// ============================================================================
// Helper Functions
// ============================================================================

HorizontalDirection ErosionContext::opposite_direction(HorizontalDirection dir) {
    switch (dir) {
        case HorizontalDirection::NegX:
            return HorizontalDirection::PosX;
        case HorizontalDirection::PosX:
            return HorizontalDirection::NegX;
        case HorizontalDirection::NegZ:
            return HorizontalDirection::PosZ;
        case HorizontalDirection::PosZ:
            return HorizontalDirection::NegZ;
        default:
            return dir;
    }
}

ChunkPos ErosionContext::neighbor_chunk(const ChunkPos& chunk, HorizontalDirection dir) {
    switch (dir) {
        case HorizontalDirection::NegX:
            return ChunkPos(chunk.x - 1, chunk.y);
        case HorizontalDirection::PosX:
            return ChunkPos(chunk.x + 1, chunk.y);
        case HorizontalDirection::NegZ:
            return ChunkPos(chunk.x, chunk.y - 1);
        case HorizontalDirection::PosZ:
            return ChunkPos(chunk.x, chunk.y + 1);
        default:
            return chunk;
    }
}

}  // namespace realcraft::world
