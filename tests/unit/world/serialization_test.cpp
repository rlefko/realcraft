// RealCraft World System Tests
// serialization_test.cpp - Tests for ChunkSerializer, RegionFile, and RegionManager

#include <gtest/gtest.h>

#include <realcraft/platform/file_io.hpp>
#include <realcraft/world/block.hpp>
#include <realcraft/world/chunk.hpp>
#include <realcraft/world/serialization.hpp>

#include <filesystem>
#include <random>

namespace realcraft::world {
namespace {

// ============================================================================
// Test Fixtures
// ============================================================================

class ChunkSerializerTest : public ::testing::Test {
protected:
    void SetUp() override { BlockRegistry::instance().register_defaults(); }

    ChunkSerializer serializer;
};

class RegionFileTest : public ::testing::Test {
protected:
    void SetUp() override {
        BlockRegistry::instance().register_defaults();
        test_dir_ = platform::FileSystem::get_temp_directory() / "realcraft_test_regions";
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    std::filesystem::path test_dir_;
    ChunkSerializer serializer_;
};

class RegionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        BlockRegistry::instance().register_defaults();
        test_dir_ = platform::FileSystem::get_temp_directory() / "realcraft_test_world";
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    std::filesystem::path test_dir_;
    ChunkSerializer serializer_;
};

// ============================================================================
// ChunkSerializer Basic Tests
// ============================================================================

TEST_F(ChunkSerializerTest, SerializeDeserialize_EmptyChunk) {
    ChunkDesc desc;
    desc.position = ChunkPos(5, 10);
    Chunk chunk(desc);

    // Serialize
    std::vector<uint8_t> data = serializer.serialize(chunk);
    ASSERT_FALSE(data.empty());
    EXPECT_GE(data.size(), sizeof(ChunkFileHeader));

    // Deserialize into new chunk
    ChunkDesc desc2;
    desc2.position = ChunkPos(5, 10);
    Chunk chunk2(desc2);

    EXPECT_TRUE(serializer.deserialize(chunk2, data));
    EXPECT_EQ(chunk2.get_position(), ChunkPos(5, 10));

    // Verify blocks match (all should be air)
    for (int x = 0; x < 16; ++x) {
        for (int y = 0; y < 16; ++y) {
            for (int z = 0; z < 16; ++z) {
                EXPECT_EQ(chunk2.get_block(LocalBlockPos(x, y, z)), BLOCK_AIR);
            }
        }
    }
}

TEST_F(ChunkSerializerTest, SerializeDeserialize_SingleBlock) {
    ChunkDesc desc;
    Chunk chunk(desc);

    BlockId stone = BlockRegistry::instance().stone_id();
    chunk.set_block(LocalBlockPos(10, 50, 20), stone);

    std::vector<uint8_t> data = serializer.serialize(chunk);
    ASSERT_FALSE(data.empty());

    ChunkDesc desc2;
    Chunk chunk2(desc2);
    EXPECT_TRUE(serializer.deserialize(chunk2, data));

    EXPECT_EQ(chunk2.get_block(LocalBlockPos(10, 50, 20)), stone);
    EXPECT_EQ(chunk2.get_block(LocalBlockPos(0, 0, 0)), BLOCK_AIR);
}

TEST_F(ChunkSerializerTest, SerializeDeserialize_FilledChunk) {
    ChunkDesc desc;
    Chunk chunk(desc);

    BlockId stone = BlockRegistry::instance().stone_id();
    PaletteEntry stone_entry{stone, 0};

    {
        auto lock = chunk.write_lock();
        lock.fill(stone_entry);
    }

    std::vector<uint8_t> data = serializer.serialize(chunk);
    ASSERT_FALSE(data.empty());

    ChunkDesc desc2;
    Chunk chunk2(desc2);
    EXPECT_TRUE(serializer.deserialize(chunk2, data));

    // Verify all blocks are stone
    EXPECT_EQ(chunk2.get_block(LocalBlockPos(0, 0, 0)), stone);
    EXPECT_EQ(chunk2.get_block(LocalBlockPos(15, 128, 15)), stone);
    EXPECT_EQ(chunk2.get_block(LocalBlockPos(31, 255, 31)), stone);
}

TEST_F(ChunkSerializerTest, SerializeDeserialize_RandomBlocks) {
    ChunkDesc desc;
    Chunk chunk(desc);

    std::mt19937 rng(42);  // Fixed seed for reproducibility
    std::uniform_int_distribution<int> block_dist(0, 5);

    BlockId blocks[] = {BLOCK_AIR,
                        BlockRegistry::instance().stone_id(),
                        BlockRegistry::instance().dirt_id(),
                        BlockRegistry::instance().grass_id(),
                        BlockRegistry::instance().water_id(),
                        BlockRegistry::instance().sand_id()};

    // Set random blocks in a region
    for (int x = 0; x < 16; ++x) {
        for (int y = 0; y < 64; ++y) {
            for (int z = 0; z < 16; ++z) {
                chunk.set_block(LocalBlockPos(x, y, z), blocks[block_dist(rng)]);
            }
        }
    }

    std::vector<uint8_t> data = serializer.serialize(chunk);
    ASSERT_FALSE(data.empty());

    ChunkDesc desc2;
    Chunk chunk2(desc2);
    EXPECT_TRUE(serializer.deserialize(chunk2, data));

    // Reset RNG and verify
    rng.seed(42);
    for (int x = 0; x < 16; ++x) {
        for (int y = 0; y < 64; ++y) {
            for (int z = 0; z < 16; ++z) {
                BlockId expected = blocks[block_dist(rng)];
                EXPECT_EQ(chunk2.get_block(LocalBlockPos(x, y, z)), expected)
                    << "Mismatch at (" << x << ", " << y << ", " << z << ")";
            }
        }
    }
}

TEST_F(ChunkSerializerTest, SerializeDeserialize_AllBlockTypes) {
    ChunkDesc desc;
    Chunk chunk(desc);

    auto& registry = BlockRegistry::instance();

    // Place each block type at a different position
    int y = 0;
    registry.for_each([&chunk, &y](const BlockType& block) {
        chunk.set_block(LocalBlockPos(0, y, 0), block.get_id());
        ++y;
    });

    std::vector<uint8_t> data = serializer.serialize(chunk);
    ASSERT_FALSE(data.empty());

    ChunkDesc desc2;
    Chunk chunk2(desc2);
    EXPECT_TRUE(serializer.deserialize(chunk2, data));

    // Verify all block types
    y = 0;
    registry.for_each([&chunk2, &y](const BlockType& block) {
        EXPECT_EQ(chunk2.get_block(LocalBlockPos(0, y, 0)), block.get_id()) << "Block type mismatch at y=" << y;
        ++y;
    });
}

// ============================================================================
// Compression Tests
// ============================================================================

TEST_F(ChunkSerializerTest, Compression_ReducesSize) {
    ChunkDesc desc;
    Chunk chunk(desc);

    // Create layered pattern with multiple block types (generates more data for compression)
    // This creates a striped pattern that's less optimal for internal RLE
    // but still benefits from dictionary-based compression like zstd
    const BlockRegistry& registry = BlockRegistry::instance();
    BlockId blocks[] = {registry.stone_id(), registry.dirt_id(), registry.grass_id(),
                        registry.sand_id(), registry.water_id()};

    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < CHUNK_SIZE_X; ++x) {
            for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
                // Add some variation based on position
                BlockId block = blocks[(y + x + z) % 5];
                chunk.set_block(LocalBlockPos(x, y, z), block);
            }
        }
    }

    // Get raw chunk data size (internal RLE)
    std::vector<uint8_t> raw_data = chunk.serialize();

    // Get compressed size
    std::vector<uint8_t> compressed_data = serializer.serialize(chunk);

    // For small data, compression may not reduce size due to frame overhead.
    // This test validates that compression works without errors.
    // For larger datasets, zstd should reduce size.
    EXPECT_FALSE(compressed_data.empty()) << "Compression should produce output";

    // If raw data is large enough, compressed should be smaller
    if (raw_data.size() > 100) {
        size_t payload_size = compressed_data.size() - sizeof(ChunkFileHeader);
        EXPECT_LT(payload_size, raw_data.size()) << "Compression should reduce size for larger data";
    }
}

TEST_F(ChunkSerializerTest, Compression_RoundTrip) {
    ChunkDesc desc;
    Chunk chunk(desc);

    // Create varied pattern
    for (int i = 0; i < 1000; ++i) {
        int x = i % CHUNK_SIZE_X;
        int y = (i / CHUNK_SIZE_X) % CHUNK_SIZE_Y;
        int z = (i / (CHUNK_SIZE_X * CHUNK_SIZE_Y)) % CHUNK_SIZE_Z;
        chunk.set_block(LocalBlockPos(x, y, z), static_cast<BlockId>((i % 10) + 1));
    }

    std::vector<uint8_t> data = serializer.serialize(chunk);

    ChunkDesc desc2;
    Chunk chunk2(desc2);
    EXPECT_TRUE(serializer.deserialize(chunk2, data));

    // Verify exact match
    for (int i = 0; i < 1000; ++i) {
        int x = i % CHUNK_SIZE_X;
        int y = (i / CHUNK_SIZE_X) % CHUNK_SIZE_Y;
        int z = (i / (CHUNK_SIZE_X * CHUNK_SIZE_Y)) % CHUNK_SIZE_Z;
        BlockId expected = static_cast<BlockId>((i % 10) + 1);
        EXPECT_EQ(chunk2.get_block(LocalBlockPos(x, y, z)), expected);
    }
}

TEST_F(ChunkSerializerTest, Compression_EmptyData) {
    // Empty chunk should still serialize/deserialize correctly
    ChunkDesc desc;
    Chunk chunk(desc);

    std::vector<uint8_t> data = serializer.serialize(chunk);
    EXPECT_FALSE(data.empty());

    ChunkDesc desc2;
    Chunk chunk2(desc2);
    EXPECT_TRUE(serializer.deserialize(chunk2, data));
}

// ============================================================================
// Header Validation Tests
// ============================================================================

TEST_F(ChunkSerializerTest, ValidateHeader_ValidData) {
    ChunkDesc desc;
    Chunk chunk(desc);

    std::vector<uint8_t> data = serializer.serialize(chunk);
    EXPECT_TRUE(serializer.validate_header(data));
}

TEST_F(ChunkSerializerTest, ValidateHeader_InvalidMagic) {
    ChunkDesc desc;
    Chunk chunk(desc);

    std::vector<uint8_t> data = serializer.serialize(chunk);

    // Corrupt magic bytes
    data[0] = 0xFF;
    data[1] = 0xFF;

    EXPECT_FALSE(serializer.validate_header(data));
}

TEST_F(ChunkSerializerTest, ValidateHeader_FutureVersion) {
    ChunkDesc desc;
    Chunk chunk(desc);

    std::vector<uint8_t> data = serializer.serialize(chunk);

    // Set version to future value
    ChunkFileHeader* header = reinterpret_cast<ChunkFileHeader*>(data.data());
    header->version = 999;

    EXPECT_FALSE(serializer.validate_header(data));
}

TEST_F(ChunkSerializerTest, ValidateHeader_TruncatedData) {
    std::vector<uint8_t> truncated(10);  // Less than header size
    EXPECT_FALSE(serializer.validate_header(truncated));
}

// ============================================================================
// File I/O Tests
// ============================================================================

TEST_F(RegionFileTest, SaveLoad_RoundTrip) {
    ChunkDesc desc;
    desc.position = ChunkPos(3, 7);
    Chunk chunk(desc);

    chunk.set_block(LocalBlockPos(5, 100, 10), BlockRegistry::instance().stone_id());

    std::filesystem::path path = test_dir_ / "test_chunk.rcvc";
    EXPECT_TRUE(serializer_.save_to_file(chunk, path));
    EXPECT_TRUE(std::filesystem::exists(path));

    ChunkDesc desc2;
    desc2.position = ChunkPos(3, 7);
    Chunk chunk2(desc2);

    EXPECT_TRUE(serializer_.load_from_file(chunk2, path));
    EXPECT_EQ(chunk2.get_block(LocalBlockPos(5, 100, 10)), BlockRegistry::instance().stone_id());
}

TEST_F(RegionFileTest, SaveLoad_CreateDirectories) {
    ChunkDesc desc;
    Chunk chunk(desc);

    std::filesystem::path path = test_dir_ / "deep" / "nested" / "dir" / "chunk.rcvc";
    EXPECT_TRUE(serializer_.save_to_file(chunk, path));
    EXPECT_TRUE(std::filesystem::exists(path));
}

// ============================================================================
// RegionFile Tests
// ============================================================================

TEST_F(RegionFileTest, RegionFile_WriteRead) {
    std::filesystem::path path = test_dir_ / "r.0.0.rcr";
    RegionFile region(path);

    EXPECT_TRUE(region.open(true));
    EXPECT_TRUE(region.is_open());

    ChunkDesc desc;
    desc.position = ChunkPos(5, 5);
    Chunk chunk(desc);
    chunk.set_block(LocalBlockPos(0, 0, 0), BlockRegistry::instance().stone_id());

    std::vector<uint8_t> data = serializer_.serialize(chunk);
    EXPECT_TRUE(region.write_chunk(ChunkPos(5, 5), data));

    auto read_data = region.read_chunk(ChunkPos(5, 5));
    ASSERT_TRUE(read_data.has_value());
    EXPECT_EQ(read_data->size(), data.size());
}

TEST_F(RegionFileTest, RegionFile_MultipleChunks) {
    std::filesystem::path path = test_dir_ / "r.0.0.rcr";
    RegionFile region(path);
    EXPECT_TRUE(region.open(true));

    // Write multiple chunks
    for (int i = 0; i < 10; ++i) {
        ChunkDesc desc;
        desc.position = ChunkPos(i, i);
        Chunk chunk(desc);
        chunk.set_block(LocalBlockPos(i, i, i), static_cast<BlockId>(i + 1));

        std::vector<uint8_t> data = serializer_.serialize(chunk);
        EXPECT_TRUE(region.write_chunk(ChunkPos(i, i), data));
    }

    EXPECT_EQ(region.chunk_count(), 10u);

    // Read them back
    for (int i = 0; i < 10; ++i) {
        auto data = region.read_chunk(ChunkPos(i, i));
        ASSERT_TRUE(data.has_value());

        ChunkDesc desc;
        desc.position = ChunkPos(i, i);
        Chunk chunk(desc);
        EXPECT_TRUE(serializer_.deserialize(chunk, *data));
        EXPECT_EQ(chunk.get_block(LocalBlockPos(i, i, i)), static_cast<BlockId>(i + 1));
    }
}

TEST_F(RegionFileTest, RegionFile_HasChunk) {
    std::filesystem::path path = test_dir_ / "r.0.0.rcr";
    RegionFile region(path);
    EXPECT_TRUE(region.open(true));

    EXPECT_FALSE(region.has_chunk(ChunkPos(5, 5)));

    ChunkDesc desc;
    Chunk chunk(desc);
    std::vector<uint8_t> data = serializer_.serialize(chunk);
    EXPECT_TRUE(region.write_chunk(ChunkPos(5, 5), data));

    EXPECT_TRUE(region.has_chunk(ChunkPos(5, 5)));
    EXPECT_FALSE(region.has_chunk(ChunkPos(6, 6)));
}

TEST_F(RegionFileTest, RegionFile_DeleteChunk) {
    std::filesystem::path path = test_dir_ / "r.0.0.rcr";
    RegionFile region(path);
    EXPECT_TRUE(region.open(true));

    ChunkDesc desc;
    Chunk chunk(desc);
    std::vector<uint8_t> data = serializer_.serialize(chunk);
    EXPECT_TRUE(region.write_chunk(ChunkPos(5, 5), data));
    EXPECT_TRUE(region.has_chunk(ChunkPos(5, 5)));

    EXPECT_TRUE(region.delete_chunk(ChunkPos(5, 5)));
    EXPECT_FALSE(region.has_chunk(ChunkPos(5, 5)));
}

// ============================================================================
// RegionManager Tests
// ============================================================================

TEST_F(RegionManagerTest, RegionManager_WriteRead) {
    RegionManager manager(test_dir_);

    ChunkDesc desc;
    desc.position = ChunkPos(100, 200);
    Chunk chunk(desc);
    chunk.set_block(LocalBlockPos(10, 50, 20), BlockRegistry::instance().dirt_id());

    std::vector<uint8_t> data = serializer_.serialize(chunk);
    EXPECT_TRUE(manager.write_chunk(ChunkPos(100, 200), data));

    auto read_data = manager.read_chunk(ChunkPos(100, 200));
    ASSERT_TRUE(read_data.has_value());

    ChunkDesc desc2;
    desc2.position = ChunkPos(100, 200);
    Chunk chunk2(desc2);
    EXPECT_TRUE(serializer_.deserialize(chunk2, *read_data));
    EXPECT_EQ(chunk2.get_block(LocalBlockPos(10, 50, 20)), BlockRegistry::instance().dirt_id());
}

TEST_F(RegionManagerTest, RegionManager_LRUEviction) {
    RegionManager manager(test_dir_);
    manager.set_max_open_files(2);

    // Write to 3 different regions (forcing eviction)
    for (int r = 0; r < 3; ++r) {
        ChunkPos pos(r * REGION_SIZE, 0);
        ChunkDesc desc;
        desc.position = pos;
        Chunk chunk(desc);
        chunk.set_block(LocalBlockPos(0, 0, 0), static_cast<BlockId>(r + 1));

        std::vector<uint8_t> data = serializer_.serialize(chunk);
        EXPECT_TRUE(manager.write_chunk(pos, data));
    }

    // Should only have 2 open regions
    EXPECT_LE(manager.open_region_count(), 2u);

    // All chunks should still be readable (opens regions as needed)
    for (int r = 0; r < 3; ++r) {
        ChunkPos pos(r * REGION_SIZE, 0);
        auto data = manager.read_chunk(pos);
        ASSERT_TRUE(data.has_value());

        ChunkDesc desc;
        desc.position = pos;
        Chunk chunk(desc);
        EXPECT_TRUE(serializer_.deserialize(chunk, *data));
        EXPECT_EQ(chunk.get_block(LocalBlockPos(0, 0, 0)), static_cast<BlockId>(r + 1));
    }
}

TEST_F(RegionManagerTest, RegionManager_FlushAll) {
    RegionManager manager(test_dir_);

    ChunkDesc desc;
    Chunk chunk(desc);
    std::vector<uint8_t> data = serializer_.serialize(chunk);
    EXPECT_TRUE(manager.write_chunk(ChunkPos(0, 0), data));

    // Flush should not throw
    manager.flush();

    // Data should persist after close_all
    manager.close_all();
    EXPECT_EQ(manager.open_region_count(), 0u);

    // Re-read should work
    auto read_data = manager.read_chunk(ChunkPos(0, 0));
    EXPECT_TRUE(read_data.has_value());
}

TEST_F(RegionManagerTest, RegionManager_HasChunk) {
    RegionManager manager(test_dir_);

    EXPECT_FALSE(manager.has_chunk(ChunkPos(50, 50)));

    ChunkDesc desc;
    Chunk chunk(desc);
    std::vector<uint8_t> data = serializer_.serialize(chunk);
    EXPECT_TRUE(manager.write_chunk(ChunkPos(50, 50), data));

    EXPECT_TRUE(manager.has_chunk(ChunkPos(50, 50)));
}

}  // namespace
}  // namespace realcraft::world
