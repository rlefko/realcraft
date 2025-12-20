// RealCraft Physics Engine Tests
// chunk_collider_test.cpp - Tests for chunk collision generation

#include <gtest/gtest.h>

#include <realcraft/physics/chunk_collider.hpp>
#include <realcraft/world/block.hpp>
#include <realcraft/world/chunk.hpp>

namespace realcraft::physics {
namespace {

// ============================================================================
// ShapeCache Tests
// ============================================================================

class ShapeCacheTest : public ::testing::Test {};

TEST_F(ShapeCacheTest, SingletonAccess) {
    ShapeCache& cache1 = ShapeCache::instance();
    ShapeCache& cache2 = ShapeCache::instance();

    EXPECT_EQ(&cache1, &cache2);
}

TEST_F(ShapeCacheTest, UnitBoxNotNull) {
    btBoxShape* box = ShapeCache::instance().get_unit_box();
    EXPECT_NE(box, nullptr);
}

TEST_F(ShapeCacheTest, UnitBoxReturnsame) {
    btBoxShape* box1 = ShapeCache::instance().get_unit_box();
    btBoxShape* box2 = ShapeCache::instance().get_unit_box();

    EXPECT_EQ(box1, box2);
}

TEST_F(ShapeCacheTest, CachedShapeCount) {
    EXPECT_GE(ShapeCache::instance().cached_shape_count(), 1u);
}

// ============================================================================
// ChunkCollider Tests
// ============================================================================

class ChunkColliderTest : public ::testing::Test {
protected:
    void SetUp() override { world::BlockRegistry::instance().register_defaults(); }
};

TEST_F(ChunkColliderTest, ConstructionWithEmptyChunk) {
    world::ChunkDesc chunk_desc;
    chunk_desc.position = world::ChunkPos(0, 0);
    world::Chunk chunk(chunk_desc);

    ChunkCollider collider(world::ChunkPos(0, 0), chunk);

    EXPECT_EQ(collider.get_chunk_position(), world::ChunkPos(0, 0));
    EXPECT_TRUE(collider.is_empty());
    EXPECT_EQ(collider.get_block_count(), 0u);
}

TEST_F(ChunkColliderTest, CollisionObjectExists) {
    world::ChunkDesc chunk_desc;
    chunk_desc.position = world::ChunkPos(0, 0);
    world::Chunk chunk(chunk_desc);

    ChunkCollider collider(world::ChunkPos(0, 0), chunk);

    EXPECT_NE(collider.get_collision_object(), nullptr);
}

TEST_F(ChunkColliderTest, ChunkPosition) {
    world::ChunkDesc chunk_desc;
    chunk_desc.position = world::ChunkPos(5, -3);
    world::Chunk chunk(chunk_desc);

    ChunkCollider collider(world::ChunkPos(5, -3), chunk);

    EXPECT_EQ(collider.get_chunk_position().x, 5);
    EXPECT_EQ(collider.get_chunk_position().y, -3);
}

TEST_F(ChunkColliderTest, SetWorldOffset) {
    world::ChunkDesc chunk_desc;
    chunk_desc.position = world::ChunkPos(0, 0);
    world::Chunk chunk(chunk_desc);

    ChunkCollider collider(world::ChunkPos(0, 0), chunk);

    // Should not throw
    collider.set_world_offset(glm::dvec3(100.0, 200.0, 300.0));
}

TEST_F(ChunkColliderTest, MoveConstruction) {
    world::ChunkDesc chunk_desc;
    chunk_desc.position = world::ChunkPos(0, 0);
    world::Chunk chunk(chunk_desc);

    ChunkCollider collider1(world::ChunkPos(0, 0), chunk);
    ChunkCollider collider2(std::move(collider1));

    EXPECT_EQ(collider2.get_chunk_position(), world::ChunkPos(0, 0));
    EXPECT_NE(collider2.get_collision_object(), nullptr);
}

TEST_F(ChunkColliderTest, UpdateBlockNoCollisionChange) {
    world::ChunkDesc chunk_desc;
    chunk_desc.position = world::ChunkPos(0, 0);
    world::Chunk chunk(chunk_desc);

    ChunkCollider collider(world::ChunkPos(0, 0), chunk);

    // Air to air - no collision change
    bool success = collider.update_block(world::LocalBlockPos(0, 0, 0), world::BLOCK_AIR, world::BLOCK_AIR);
    EXPECT_TRUE(success);
}

// ============================================================================
// ChunkCollider with Blocks Tests
// ============================================================================

TEST_F(ChunkColliderTest, ColliderWithSolidBlocks) {
    world::ChunkDesc chunk_desc;
    chunk_desc.position = world::ChunkPos(0, 0);
    world::Chunk chunk(chunk_desc);

    // Add some solid blocks
    world::BlockId stone_id = world::BlockRegistry::instance().stone_id();
    {
        auto write_lock = chunk.write_lock();
        write_lock.set_entry(world::LocalBlockPos(0, 0, 0), world::PaletteEntry{stone_id, 0});
        write_lock.set_entry(world::LocalBlockPos(1, 0, 0), world::PaletteEntry{stone_id, 0});
        write_lock.set_entry(world::LocalBlockPos(0, 1, 0), world::PaletteEntry{stone_id, 0});
    }

    ChunkCollider collider(world::ChunkPos(0, 0), chunk);

    EXPECT_FALSE(collider.is_empty());
    EXPECT_EQ(collider.get_block_count(), 3u);
}

TEST_F(ChunkColliderTest, RebuildChunkCollider) {
    world::ChunkDesc chunk_desc;
    chunk_desc.position = world::ChunkPos(0, 0);
    world::Chunk chunk(chunk_desc);

    ChunkCollider collider(world::ChunkPos(0, 0), chunk);
    EXPECT_TRUE(collider.is_empty());

    // Add blocks to chunk
    world::BlockId stone_id = world::BlockRegistry::instance().stone_id();
    {
        auto write_lock = chunk.write_lock();
        write_lock.set_entry(world::LocalBlockPos(5, 5, 5), world::PaletteEntry{stone_id, 0});
    }

    // Rebuild
    collider.rebuild(chunk);

    EXPECT_FALSE(collider.is_empty());
    EXPECT_EQ(collider.get_block_count(), 1u);
}

}  // namespace
}  // namespace realcraft::physics
