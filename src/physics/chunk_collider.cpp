// RealCraft Physics Engine
// chunk_collider.cpp - Chunk-to-collision shape conversion implementation

#include <btBulletCollisionCommon.h>
#include <realcraft/physics/chunk_collider.hpp>
#include <realcraft/world/block.hpp>
#include <unordered_set>

namespace realcraft::physics {

// ============================================================================
// ShapeCache Implementation
// ============================================================================

struct ShapeCache::Impl {
    std::unique_ptr<btBoxShape> unit_box;

    Impl() {
        // Create unit box shape (half-extents of 0.5 = full size of 1.0)
        unit_box = std::make_unique<btBoxShape>(btVector3(0.5f, 0.5f, 0.5f));
    }
};

ShapeCache& ShapeCache::instance() {
    static ShapeCache instance;
    return instance;
}

ShapeCache::ShapeCache() : impl_(std::make_unique<Impl>()) {}

ShapeCache::~ShapeCache() = default;

btBoxShape* ShapeCache::get_unit_box() {
    return impl_->unit_box.get();
}

size_t ShapeCache::cached_shape_count() const {
    return 1;  // Currently only the unit box is cached
}

// ============================================================================
// ChunkCollider Implementation
// ============================================================================

struct ChunkCollider::Impl {
    world::ChunkPos chunk_position;
    std::unique_ptr<btCompoundShape> compound_shape;
    std::unique_ptr<btCollisionObject> collision_object;

    // Track which local positions have shapes (for incremental updates)
    std::unordered_set<size_t> active_block_indices;

    size_t block_count = 0;
    glm::dvec3 world_offset{0.0};

    void initialize_collision_object() {
        if (!collision_object) {
            collision_object = std::make_unique<btCollisionObject>();
        }

        if (!compound_shape) {
            compound_shape = std::make_unique<btCompoundShape>();
        }

        collision_object->setCollisionShape(compound_shape.get());

        // Mark as static object
        collision_object->setCollisionFlags(collision_object->getCollisionFlags() |
                                            btCollisionObject::CF_STATIC_OBJECT);

        update_transform();
    }

    void update_transform() {
        if (!collision_object) {
            return;
        }

        // Position at chunk world origin
        double world_x =
            static_cast<double>(chunk_position.x) * static_cast<double>(world::CHUNK_SIZE_X) - world_offset.x;
        double world_z =
            static_cast<double>(chunk_position.y) * static_cast<double>(world::CHUNK_SIZE_Z) - world_offset.z;

        btTransform transform;
        transform.setIdentity();
        transform.setOrigin(btVector3(static_cast<btScalar>(world_x), static_cast<btScalar>(-world_offset.y),
                                      static_cast<btScalar>(world_z)));

        collision_object->setWorldTransform(transform);
    }

    void clear_shapes() {
        if (compound_shape) {
            while (compound_shape->getNumChildShapes() > 0) {
                compound_shape->removeChildShapeByIndex(compound_shape->getNumChildShapes() - 1);
            }
        }
        active_block_indices.clear();
        block_count = 0;
    }

    void add_block_shape(const world::LocalBlockPos& pos) {
        btBoxShape* box_shape = ShapeCache::instance().get_unit_box();

        // Position at block center (local coordinates within chunk)
        btTransform local_transform;
        local_transform.setIdentity();
        local_transform.setOrigin(btVector3(static_cast<btScalar>(pos.x) + 0.5f, static_cast<btScalar>(pos.y) + 0.5f,
                                            static_cast<btScalar>(pos.z) + 0.5f));

        compound_shape->addChildShape(local_transform, box_shape);

        size_t index = world::local_to_index(pos);
        active_block_indices.insert(index);
        ++block_count;
    }
};

ChunkCollider::ChunkCollider(const world::ChunkPos& position, const world::Chunk& chunk)
    : impl_(std::make_unique<Impl>()) {
    impl_->chunk_position = position;
    impl_->initialize_collision_object();
    rebuild(chunk);
}

ChunkCollider::~ChunkCollider() = default;

ChunkCollider::ChunkCollider(ChunkCollider&&) noexcept = default;
ChunkCollider& ChunkCollider::operator=(ChunkCollider&&) noexcept = default;

void ChunkCollider::rebuild(const world::Chunk& chunk) {
    impl_->clear_shapes();

    // Ensure we have a compound shape
    if (!impl_->compound_shape) {
        impl_->compound_shape = std::make_unique<btCompoundShape>();
    }

    // Get read lock on chunk
    auto read_lock = chunk.read_lock();

    // Iterate over all blocks in chunk
    for (int y = 0; y < world::CHUNK_SIZE_Y; ++y) {
        for (int z = 0; z < world::CHUNK_SIZE_Z; ++z) {
            for (int x = 0; x < world::CHUNK_SIZE_X; ++x) {
                world::LocalBlockPos pos(x, y, z);
                auto entry = read_lock.get_entry(pos);

                // Skip air and blocks without collision
                if (entry.block_id == world::BLOCK_AIR) {
                    continue;
                }

                const world::BlockType* block_type = world::BlockRegistry::instance().get(entry.block_id);
                if (!block_type || !block_type->has_collision()) {
                    continue;
                }

                impl_->add_block_shape(pos);
            }
        }
    }

    // Recalculate AABB
    if (impl_->compound_shape) {
        impl_->compound_shape->recalculateLocalAabb();
    }

    // Update collision object shape reference
    if (impl_->collision_object && impl_->compound_shape) {
        impl_->collision_object->setCollisionShape(impl_->compound_shape.get());
    }

    impl_->update_transform();
}

bool ChunkCollider::update_block(const world::LocalBlockPos& pos, world::BlockId old_block, world::BlockId new_block) {
    // Check if old block had collision
    bool old_has_collision = false;
    if (old_block != world::BLOCK_AIR) {
        const world::BlockType* old_type = world::BlockRegistry::instance().get(old_block);
        old_has_collision = old_type && old_type->has_collision();
    }

    // Check if new block has collision
    bool new_has_collision = false;
    if (new_block != world::BLOCK_AIR) {
        const world::BlockType* new_type = world::BlockRegistry::instance().get(new_block);
        new_has_collision = new_type && new_type->has_collision();
    }

    // No change in collision state
    if (old_has_collision == new_has_collision) {
        return true;
    }

    if (new_has_collision && !old_has_collision) {
        // Adding collision
        impl_->add_block_shape(pos);
        impl_->compound_shape->recalculateLocalAabb();
        return true;
    } else if (!new_has_collision && old_has_collision) {
        // Removing collision - btCompoundShape doesn't have efficient single removal
        // For now, trigger a full rebuild
        return false;
    }

    return true;
}

btCollisionObject* ChunkCollider::get_collision_object() {
    return impl_->collision_object.get();
}

const btCollisionObject* ChunkCollider::get_collision_object() const {
    return impl_->collision_object.get();
}

world::ChunkPos ChunkCollider::get_chunk_position() const {
    return impl_->chunk_position;
}

void ChunkCollider::set_world_offset(const glm::dvec3& offset) {
    impl_->world_offset = offset;
    impl_->update_transform();
}

size_t ChunkCollider::get_block_count() const {
    return impl_->block_count;
}

bool ChunkCollider::is_empty() const {
    return impl_->block_count == 0;
}

}  // namespace realcraft::physics
