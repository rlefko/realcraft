// RealCraft World System Tests
// types_test.cpp - Tests for coordinate types and conversions

#include <gtest/gtest.h>

#include <realcraft/world/types.hpp>

namespace realcraft::world {
namespace {

// ============================================================================
// Coordinate Conversion Tests
// ============================================================================

TEST(WorldTypesTest, WorldToChunk_PositiveCoords) {
    // Block at (32, 0, 64) should be in chunk (1, 2)
    WorldBlockPos pos(32, 0, 64);
    ChunkPos chunk = world_to_chunk(pos);
    EXPECT_EQ(chunk.x, 1);
    EXPECT_EQ(chunk.y, 2);
}

TEST(WorldTypesTest, WorldToChunk_NegativeCoords) {
    // Block at (-1, 0, -1) should be in chunk (-1, -1)
    WorldBlockPos pos(-1, 0, -1);
    ChunkPos chunk = world_to_chunk(pos);
    EXPECT_EQ(chunk.x, -1);
    EXPECT_EQ(chunk.y, -1);
}

TEST(WorldTypesTest, WorldToChunk_Origin) {
    WorldBlockPos pos(0, 0, 0);
    ChunkPos chunk = world_to_chunk(pos);
    EXPECT_EQ(chunk.x, 0);
    EXPECT_EQ(chunk.y, 0);
}

TEST(WorldTypesTest, WorldToLocal_PositiveCoords) {
    WorldBlockPos pos(35, 100, 66);
    LocalBlockPos local = world_to_local(pos);
    EXPECT_EQ(local.x, 3);  // 35 % 32 = 3
    EXPECT_EQ(local.y, 100);
    EXPECT_EQ(local.z, 2);  // 66 % 32 = 2
}

TEST(WorldTypesTest, WorldToLocal_NegativeCoords) {
    WorldBlockPos pos(-1, 50, -1);
    LocalBlockPos local = world_to_local(pos);
    EXPECT_EQ(local.x, 31);  // -1 mod 32 should give 31
    EXPECT_EQ(local.y, 50);
    EXPECT_EQ(local.z, 31);
}

TEST(WorldTypesTest, LocalToWorld_Roundtrip) {
    ChunkPos chunk(5, -3);
    LocalBlockPos local(10, 100, 20);

    WorldBlockPos world = local_to_world(chunk, local);
    ChunkPos chunk2 = world_to_chunk(world);
    LocalBlockPos local2 = world_to_local(world);

    EXPECT_EQ(chunk, chunk2);
    EXPECT_EQ(local, local2);
}

TEST(WorldTypesTest, LocalToIndex_Valid) {
    LocalBlockPos pos(5, 10, 15);
    size_t index = local_to_index(pos);

    // Y-major layout: y * CHUNK_SIZE_X * CHUNK_SIZE_Z + z * CHUNK_SIZE_X + x
    size_t expected = 10 * CHUNK_SIZE_X * CHUNK_SIZE_Z + 15 * CHUNK_SIZE_X + 5;
    EXPECT_EQ(index, expected);
}

TEST(WorldTypesTest, IndexToLocal_Roundtrip) {
    for (size_t i = 0; i < 100; ++i) {
        LocalBlockPos pos = index_to_local(i);
        size_t index2 = local_to_index(pos);
        EXPECT_EQ(i, index2);
    }
}

TEST(WorldTypesTest, IsValidLocal) {
    EXPECT_TRUE(is_valid_local(LocalBlockPos(0, 0, 0)));
    EXPECT_TRUE(is_valid_local(LocalBlockPos(31, 255, 31)));
    EXPECT_TRUE(is_valid_local(LocalBlockPos(15, 128, 15)));

    EXPECT_FALSE(is_valid_local(LocalBlockPos(-1, 0, 0)));
    EXPECT_FALSE(is_valid_local(LocalBlockPos(0, -1, 0)));
    EXPECT_FALSE(is_valid_local(LocalBlockPos(0, 0, -1)));
    EXPECT_FALSE(is_valid_local(LocalBlockPos(32, 0, 0)));
    EXPECT_FALSE(is_valid_local(LocalBlockPos(0, 256, 0)));
    EXPECT_FALSE(is_valid_local(LocalBlockPos(0, 0, 32)));
}

TEST(WorldTypesTest, WorldToBlock) {
    WorldPos pos(10.5, 20.7, -5.3);
    WorldBlockPos block = world_to_block(pos);

    EXPECT_EQ(block.x, 10);
    EXPECT_EQ(block.y, 20);
    EXPECT_EQ(block.z, -6);  // floor(-5.3) = -6
}

TEST(WorldTypesTest, ChunkToRegion) {
    EXPECT_EQ(chunk_to_region(ChunkPos(0, 0)), glm::ivec2(0, 0));
    EXPECT_EQ(chunk_to_region(ChunkPos(31, 31)), glm::ivec2(0, 0));
    EXPECT_EQ(chunk_to_region(ChunkPos(32, 32)), glm::ivec2(1, 1));
    EXPECT_EQ(chunk_to_region(ChunkPos(-1, -1)), glm::ivec2(-1, -1));
    EXPECT_EQ(chunk_to_region(ChunkPos(-32, -32)), glm::ivec2(-1, -1));
    EXPECT_EQ(chunk_to_region(ChunkPos(-33, -33)), glm::ivec2(-2, -2));
}

TEST(WorldTypesTest, ChunkLocalInRegion) {
    EXPECT_EQ(chunk_local_in_region(ChunkPos(0, 0)), glm::ivec2(0, 0));
    EXPECT_EQ(chunk_local_in_region(ChunkPos(31, 31)), glm::ivec2(31, 31));
    EXPECT_EQ(chunk_local_in_region(ChunkPos(32, 32)), glm::ivec2(0, 0));
    EXPECT_EQ(chunk_local_in_region(ChunkPos(-1, -1)), glm::ivec2(31, 31));
}

// ============================================================================
// Enum and Flag Tests
// ============================================================================

TEST(WorldTypesTest, BlockFlags_Combination) {
    BlockFlags flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision;

    EXPECT_TRUE(has_flag(flags, BlockFlags::Solid));
    EXPECT_TRUE(has_flag(flags, BlockFlags::FullCube));
    EXPECT_TRUE(has_flag(flags, BlockFlags::HasCollision));
    EXPECT_FALSE(has_flag(flags, BlockFlags::Transparent));
    EXPECT_FALSE(has_flag(flags, BlockFlags::Liquid));
}

TEST(WorldTypesTest, Direction_Opposite) {
    EXPECT_EQ(opposite(Direction::NegX), Direction::PosX);
    EXPECT_EQ(opposite(Direction::PosX), Direction::NegX);
    EXPECT_EQ(opposite(Direction::NegY), Direction::PosY);
    EXPECT_EQ(opposite(Direction::PosY), Direction::NegY);
    EXPECT_EQ(opposite(Direction::NegZ), Direction::PosZ);
    EXPECT_EQ(opposite(Direction::PosZ), Direction::NegZ);
}

TEST(WorldTypesTest, Direction_Offsets) {
    EXPECT_EQ(direction_offset(Direction::NegX), glm::ivec3(-1, 0, 0));
    EXPECT_EQ(direction_offset(Direction::PosX), glm::ivec3(1, 0, 0));
    EXPECT_EQ(direction_offset(Direction::NegY), glm::ivec3(0, -1, 0));
    EXPECT_EQ(direction_offset(Direction::PosY), glm::ivec3(0, 1, 0));
    EXPECT_EQ(direction_offset(Direction::NegZ), glm::ivec3(0, 0, -1));
    EXPECT_EQ(direction_offset(Direction::PosZ), glm::ivec3(0, 0, 1));
}

TEST(WorldTypesTest, ChunkState_ToString) {
    EXPECT_STREQ(chunk_state_to_string(ChunkState::Unloaded), "Unloaded");
    EXPECT_STREQ(chunk_state_to_string(ChunkState::Loading), "Loading");
    EXPECT_STREQ(chunk_state_to_string(ChunkState::Loaded), "Loaded");
    EXPECT_STREQ(chunk_state_to_string(ChunkState::Modified), "Modified");
}

// ============================================================================
// Hash Function Tests
// ============================================================================

TEST(WorldTypesTest, ChunkPos_Hash) {
    std::hash<ChunkPos> hasher;

    // Different positions should (usually) have different hashes
    ChunkPos pos1(0, 0);
    ChunkPos pos2(1, 0);
    ChunkPos pos3(0, 1);
    ChunkPos pos4(-1, -1);

    EXPECT_NE(hasher(pos1), hasher(pos2));
    EXPECT_NE(hasher(pos1), hasher(pos3));
    EXPECT_NE(hasher(pos2), hasher(pos3));
    EXPECT_NE(hasher(pos1), hasher(pos4));

    // Same position should have same hash
    EXPECT_EQ(hasher(pos1), hasher(ChunkPos(0, 0)));
}

}  // namespace
}  // namespace realcraft::world
