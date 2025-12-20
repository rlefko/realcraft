// RealCraft Physics Engine
// chunk_collider.hpp - Chunk-to-collision shape conversion

#pragma once

#include "types.hpp"

#include <memory>
#include <realcraft/world/chunk.hpp>

// Forward declarations for Bullet types
class btCollisionObject;
class btCompoundShape;
class btBoxShape;

namespace realcraft::physics {

// ============================================================================
// Shape Cache (singleton for shared shapes)
// ============================================================================

class ShapeCache {
public:
    [[nodiscard]] static ShapeCache& instance();

    // Non-copyable, non-movable
    ShapeCache(const ShapeCache&) = delete;
    ShapeCache& operator=(const ShapeCache&) = delete;
    ShapeCache(ShapeCache&&) = delete;
    ShapeCache& operator=(ShapeCache&&) = delete;

    // Get shared unit box shape (half-extents 0.5 for 1.0 full block)
    [[nodiscard]] btBoxShape* get_unit_box();

    // Statistics
    [[nodiscard]] size_t cached_shape_count() const;

private:
    ShapeCache();
    ~ShapeCache();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Chunk Collider
// ============================================================================

class ChunkCollider {
public:
    ChunkCollider(const world::ChunkPos& position, const world::Chunk& chunk);
    ~ChunkCollider();

    ChunkCollider(const ChunkCollider&) = delete;
    ChunkCollider& operator=(const ChunkCollider&) = delete;
    ChunkCollider(ChunkCollider&&) noexcept;
    ChunkCollider& operator=(ChunkCollider&&) noexcept;

    // Rebuild collision shape from chunk data
    void rebuild(const world::Chunk& chunk);

    // Update single block (incremental update)
    // Returns true if update was successful, false if full rebuild is needed
    bool update_block(const world::LocalBlockPos& pos, world::BlockId old_block, world::BlockId new_block);

    // Access for PhysicsWorld
    [[nodiscard]] btCollisionObject* get_collision_object();
    [[nodiscard]] const btCollisionObject* get_collision_object() const;

    // Position in world coordinates
    [[nodiscard]] world::ChunkPos get_chunk_position() const;

    // Update world offset (for origin shifting)
    void set_world_offset(const glm::dvec3& offset);

    // Statistics
    [[nodiscard]] size_t get_block_count() const;
    [[nodiscard]] bool is_empty() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::physics
