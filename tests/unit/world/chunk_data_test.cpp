// RealCraft World System Tests
// chunk_data_test.cpp - Tests for VoxelStorage and ChunkSection

#include <gtest/gtest.h>

#include <realcraft/world/chunk_data.hpp>

namespace realcraft::world {
namespace {

class VoxelStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Logger initialization removed for test stability
    }
};

TEST_F(VoxelStorageTest, InitiallyEmpty) {
    VoxelStorage storage;
    EXPECT_TRUE(storage.is_empty());
    EXPECT_TRUE(storage.is_uniform());
    EXPECT_EQ(storage.non_air_count(), 0u);
}

TEST_F(VoxelStorageTest, GetSetBlock) {
    VoxelStorage storage;

    LocalBlockPos pos(10, 50, 20);
    storage.set_block(pos, 1);  // Set to stone (assuming ID 1)

    EXPECT_EQ(storage.get_block(pos), 1);
    EXPECT_FALSE(storage.is_empty());
}

TEST_F(VoxelStorageTest, GetSetEntry) {
    VoxelStorage storage;

    LocalBlockPos pos(5, 100, 15);
    PaletteEntry entry{42, 7};
    storage.set(pos, entry);

    PaletteEntry result = storage.get(pos);
    EXPECT_EQ(result.block_id, 42);
    EXPECT_EQ(result.state_id, 7);
}

TEST_F(VoxelStorageTest, OutOfBoundsReturnsAir) {
    VoxelStorage storage;

    // Out of bounds positions should return air
    EXPECT_EQ(storage.get(LocalBlockPos(-1, 0, 0)).block_id, BLOCK_AIR);
    EXPECT_EQ(storage.get(LocalBlockPos(0, -1, 0)).block_id, BLOCK_AIR);
    EXPECT_EQ(storage.get(LocalBlockPos(CHUNK_SIZE_X, 0, 0)).block_id, BLOCK_AIR);
    EXPECT_EQ(storage.get(LocalBlockPos(0, CHUNK_SIZE_Y, 0)).block_id, BLOCK_AIR);
}

TEST_F(VoxelStorageTest, Fill) {
    VoxelStorage storage;

    PaletteEntry stone{1, 0};
    storage.fill(stone);

    EXPECT_TRUE(storage.is_uniform());
    EXPECT_EQ(storage.get(LocalBlockPos(0, 0, 0)).block_id, 1);
    EXPECT_EQ(storage.get(LocalBlockPos(15, 128, 15)).block_id, 1);
    EXPECT_EQ(storage.non_air_count(), static_cast<size_t>(CHUNK_VOLUME));
}

TEST_F(VoxelStorageTest, FillRegion) {
    VoxelStorage storage;

    PaletteEntry stone{1, 0};
    LocalBlockPos min(0, 0, 0);
    LocalBlockPos max(9, 9, 9);

    storage.fill_region(min, max, stone);

    // Inside region
    EXPECT_EQ(storage.get(LocalBlockPos(5, 5, 5)).block_id, 1);

    // Outside region
    EXPECT_EQ(storage.get(LocalBlockPos(10, 0, 0)).block_id, BLOCK_AIR);
    EXPECT_EQ(storage.get(LocalBlockPos(0, 10, 0)).block_id, BLOCK_AIR);
}

TEST_F(VoxelStorageTest, PaletteGrows) {
    VoxelStorage storage;

    // Add different block types
    for (BlockId id = 1; id <= 10; ++id) {
        storage.set_block(LocalBlockPos(static_cast<int32_t>(id), 0, 0), id);
    }

    EXPECT_GE(storage.palette_size(), 10u);
}

TEST_F(VoxelStorageTest, OptimizePalette) {
    VoxelStorage storage;

    // Add some blocks
    storage.set_block(LocalBlockPos(0, 0, 0), 1);
    storage.set_block(LocalBlockPos(1, 0, 0), 2);
    storage.set_block(LocalBlockPos(2, 0, 0), 3);

    // Now remove block type 2 by overwriting with air
    storage.set_block(LocalBlockPos(1, 0, 0), BLOCK_AIR);

    size_t before = storage.palette_size();
    storage.optimize_palette();
    size_t after = storage.palette_size();

    // Palette should shrink (or at least not grow)
    EXPECT_LE(after, before);

    // Data should still be correct
    EXPECT_EQ(storage.get_block(LocalBlockPos(0, 0, 0)), 1);
    EXPECT_EQ(storage.get_block(LocalBlockPos(1, 0, 0)), BLOCK_AIR);
    EXPECT_EQ(storage.get_block(LocalBlockPos(2, 0, 0)), 3);
}

TEST_F(VoxelStorageTest, SerializeDeserializeRLE) {
    VoxelStorage storage;

    // Create some test data
    storage.set_block(LocalBlockPos(0, 0, 0), 1);
    storage.set_block(LocalBlockPos(1, 1, 1), 2);
    storage.set_block(LocalBlockPos(15, 100, 15), 3);

    // Serialize
    std::vector<uint8_t> data = storage.serialize_rle();
    EXPECT_FALSE(data.empty());

    // Deserialize into new storage
    VoxelStorage storage2;
    EXPECT_TRUE(storage2.deserialize_rle(data));

    // Verify data matches
    EXPECT_EQ(storage2.get_block(LocalBlockPos(0, 0, 0)), 1);
    EXPECT_EQ(storage2.get_block(LocalBlockPos(1, 1, 1)), 2);
    EXPECT_EQ(storage2.get_block(LocalBlockPos(15, 100, 15)), 3);
    EXPECT_EQ(storage2.get_block(LocalBlockPos(5, 5, 5)), BLOCK_AIR);
}

TEST_F(VoxelStorageTest, SerializeDeserializeRaw) {
    VoxelStorage storage;

    // Create test data
    storage.set_block(LocalBlockPos(5, 50, 10), 42);

    // Serialize
    std::vector<uint8_t> data = storage.serialize_raw();
    EXPECT_FALSE(data.empty());

    // Deserialize
    VoxelStorage storage2;
    EXPECT_TRUE(storage2.deserialize_raw(data));

    // Verify
    EXPECT_EQ(storage2.get_block(LocalBlockPos(5, 50, 10)), 42);
}

TEST_F(VoxelStorageTest, RLECompressionEfficiency) {
    VoxelStorage storage;

    // Fill with uniform data (should compress well)
    storage.fill(PaletteEntry{1, 0});

    std::vector<uint8_t> rle_data = storage.serialize_rle();
    std::vector<uint8_t> raw_data = storage.serialize_raw();

    // RLE should be much smaller for uniform data
    EXPECT_LT(rle_data.size(), raw_data.size() / 10);
}

TEST_F(VoxelStorageTest, MoveConstructor) {
    VoxelStorage storage1;
    storage1.set_block(LocalBlockPos(5, 5, 5), 42);

    VoxelStorage storage2(std::move(storage1));
    EXPECT_EQ(storage2.get_block(LocalBlockPos(5, 5, 5)), 42);
}

TEST_F(VoxelStorageTest, MoveAssignment) {
    VoxelStorage storage1;
    storage1.set_block(LocalBlockPos(5, 5, 5), 42);

    VoxelStorage storage2;
    storage2 = std::move(storage1);
    EXPECT_EQ(storage2.get_block(LocalBlockPos(5, 5, 5)), 42);
}

TEST_F(VoxelStorageTest, UniqueBlockCount) {
    VoxelStorage storage;

    EXPECT_EQ(storage.unique_block_count(), 1u);  // Just air

    storage.set_block(LocalBlockPos(0, 0, 0), 1);
    EXPECT_EQ(storage.unique_block_count(), 2u);  // Air + stone

    storage.set_block(LocalBlockPos(1, 0, 0), 2);
    EXPECT_EQ(storage.unique_block_count(), 3u);
}

TEST_F(VoxelStorageTest, BitsPerVoxel) {
    VoxelStorage storage;

    // With only 1 entry (air), should use 4 bits
    EXPECT_EQ(storage.bits_per_voxel(), 4);

    // Add more entries
    for (int i = 1; i <= 20; ++i) {
        storage.set_block(LocalBlockPos(i, 0, 0), static_cast<BlockId>(i));
    }

    // With 21 entries (0-20), should use 8 bits
    EXPECT_EQ(storage.bits_per_voxel(), 8);
}

// ============================================================================
// ChunkSection Tests
// ============================================================================

class ChunkSectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Logger initialization removed for test stability
    }
};

TEST_F(ChunkSectionTest, InitiallyEmpty) {
    ChunkSection section;
    EXPECT_TRUE(section.is_empty());
    EXPECT_EQ(section.non_air_count(), 0u);
}

TEST_F(ChunkSectionTest, GetSetBlock) {
    ChunkSection section;

    section.set(5, 10, 8, PaletteEntry{42, 0});
    PaletteEntry result = section.get(5, 10, 8);

    EXPECT_EQ(result.block_id, 42);
}

TEST_F(ChunkSectionTest, OutOfBoundsReturnsAir) {
    ChunkSection section;

    EXPECT_EQ(section.get(-1, 0, 0).block_id, BLOCK_AIR);
    EXPECT_EQ(section.get(0, ChunkSection::SIZE, 0).block_id, BLOCK_AIR);
}

TEST_F(ChunkSectionTest, SerializeDeserialize) {
    ChunkSection section;
    section.set(0, 0, 0, PaletteEntry{1, 0});
    section.set(15, 15, 15, PaletteEntry{2, 0});

    std::vector<uint8_t> data = section.serialize();
    EXPECT_FALSE(data.empty());

    ChunkSection section2;
    EXPECT_TRUE(section2.deserialize(data));

    EXPECT_EQ(section2.get(0, 0, 0).block_id, 1);
    EXPECT_EQ(section2.get(15, 15, 15).block_id, 2);
    EXPECT_EQ(section2.get(8, 8, 8).block_id, BLOCK_AIR);
}

}  // namespace
}  // namespace realcraft::world
