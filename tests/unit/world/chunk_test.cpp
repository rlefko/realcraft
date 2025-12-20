// RealCraft World System Tests
// chunk_test.cpp - Tests for Chunk class

#include <gtest/gtest.h>

#include <realcraft/world/block.hpp>
#include <realcraft/world/chunk.hpp>
#include <thread>
#include <vector>

namespace realcraft::world {
namespace {

class ChunkTest : public ::testing::Test {
protected:
    void SetUp() override { BlockRegistry::instance().register_defaults(); }
};

TEST_F(ChunkTest, Construction) {
    ChunkDesc desc;
    desc.position = ChunkPos(5, -3);
    desc.seed = 12345;

    Chunk chunk(desc);

    EXPECT_EQ(chunk.get_position(), ChunkPos(5, -3));
    EXPECT_EQ(chunk.get_state(), ChunkState::Unloaded);
    EXPECT_FALSE(chunk.is_dirty());
}

TEST_F(ChunkTest, WorldOrigin) {
    ChunkDesc desc;
    desc.position = ChunkPos(2, 3);

    Chunk chunk(desc);
    WorldBlockPos origin = chunk.get_world_origin();

    EXPECT_EQ(origin.x, 2 * CHUNK_SIZE_X);
    EXPECT_EQ(origin.y, 0);
    EXPECT_EQ(origin.z, 3 * CHUNK_SIZE_Z);
}

TEST_F(ChunkTest, GetSetBlock) {
    ChunkDesc desc;
    Chunk chunk(desc);

    LocalBlockPos pos(10, 50, 20);
    BlockId stone = BlockRegistry::instance().stone_id();

    chunk.set_block(pos, stone);

    EXPECT_EQ(chunk.get_block(pos), stone);
    EXPECT_TRUE(chunk.is_dirty());
}

TEST_F(ChunkTest, GetSetEntry) {
    ChunkDesc desc;
    Chunk chunk(desc);

    LocalBlockPos pos(5, 100, 15);
    PaletteEntry entry{42, 7};

    chunk.set_entry(pos, entry);
    PaletteEntry result = chunk.get_entry(pos);

    EXPECT_EQ(result.block_id, 42);
    EXPECT_EQ(result.state_id, 7);
}

TEST_F(ChunkTest, MarkDirtyClean) {
    ChunkDesc desc;
    Chunk chunk(desc);

    EXPECT_FALSE(chunk.is_dirty());

    chunk.mark_dirty();
    EXPECT_TRUE(chunk.is_dirty());

    chunk.mark_clean();
    EXPECT_FALSE(chunk.is_dirty());
}

TEST_F(ChunkTest, ReadLock) {
    ChunkDesc desc;
    Chunk chunk(desc);

    LocalBlockPos pos(5, 5, 5);
    chunk.set_block(pos, 42);

    // Read lock should allow reading
    {
        auto lock = chunk.read_lock();
        EXPECT_EQ(lock.get_block(pos), 42);
        EXPECT_EQ(lock.get_entry(pos).block_id, 42);
    }
}

TEST_F(ChunkTest, WriteLock) {
    ChunkDesc desc;
    Chunk chunk(desc);

    LocalBlockPos pos(5, 5, 5);

    // Write lock should allow modification
    {
        auto lock = chunk.write_lock();
        lock.set_block(pos, 42);
    }

    EXPECT_EQ(chunk.get_block(pos), 42);
}

TEST_F(ChunkTest, WriteLockFill) {
    ChunkDesc desc;
    Chunk chunk(desc);

    PaletteEntry stone{1, 0};

    {
        auto lock = chunk.write_lock();
        lock.fill(stone);
    }

    EXPECT_EQ(chunk.get_block(LocalBlockPos(0, 0, 0)), 1);
    EXPECT_EQ(chunk.get_block(LocalBlockPos(15, 100, 15)), 1);
}

TEST_F(ChunkTest, WriteLockFillRegion) {
    ChunkDesc desc;
    Chunk chunk(desc);

    PaletteEntry stone{1, 0};

    {
        auto lock = chunk.write_lock();
        lock.fill_region(LocalBlockPos(0, 0, 0), LocalBlockPos(4, 4, 4), stone);
    }

    EXPECT_EQ(chunk.get_block(LocalBlockPos(2, 2, 2)), 1);
    EXPECT_EQ(chunk.get_block(LocalBlockPos(5, 5, 5)), BLOCK_AIR);
}

TEST_F(ChunkTest, NeighborManagement) {
    ChunkDesc desc1, desc2;
    desc1.position = ChunkPos(0, 0);
    desc2.position = ChunkPos(1, 0);

    Chunk chunk1(desc1);
    Chunk chunk2(desc2);

    chunk1.set_neighbor(HorizontalDirection::PosX, &chunk2);
    chunk2.set_neighbor(HorizontalDirection::NegX, &chunk1);

    EXPECT_EQ(chunk1.get_neighbor(HorizontalDirection::PosX), &chunk2);
    EXPECT_EQ(chunk2.get_neighbor(HorizontalDirection::NegX), &chunk1);
    EXPECT_EQ(chunk1.get_neighbor(HorizontalDirection::NegX), nullptr);
}

TEST_F(ChunkTest, HasAllNeighbors) {
    ChunkDesc desc;
    Chunk chunk(desc);

    EXPECT_FALSE(chunk.has_all_neighbors());

    ChunkDesc neighbor_desc;
    Chunk neighbors[4] = {Chunk(neighbor_desc), Chunk(neighbor_desc), Chunk(neighbor_desc), Chunk(neighbor_desc)};

    chunk.set_neighbor(HorizontalDirection::NegX, &neighbors[0]);
    EXPECT_FALSE(chunk.has_all_neighbors());

    chunk.set_neighbor(HorizontalDirection::PosX, &neighbors[1]);
    chunk.set_neighbor(HorizontalDirection::NegZ, &neighbors[2]);
    chunk.set_neighbor(HorizontalDirection::PosZ, &neighbors[3]);

    EXPECT_TRUE(chunk.has_all_neighbors());
}

TEST_F(ChunkTest, GetEntryOrNeighbor_InChunk) {
    ChunkDesc desc;
    Chunk chunk(desc);

    chunk.set_block(LocalBlockPos(5, 5, 5), 42);

    auto result = chunk.get_entry_or_neighbor(LocalBlockPos(5, 5, 5));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->block_id, 42);
}

TEST_F(ChunkTest, GetEntryOrNeighbor_NoNeighbor) {
    ChunkDesc desc;
    Chunk chunk(desc);

    // Position outside chunk with no neighbor
    auto result = chunk.get_entry_or_neighbor(LocalBlockPos(-1, 5, 5));
    EXPECT_FALSE(result.has_value());
}

TEST_F(ChunkTest, GetEntryOrNeighbor_WithNeighbor) {
    ChunkDesc desc1, desc2;
    desc1.position = ChunkPos(0, 0);
    desc2.position = ChunkPos(-1, 0);

    Chunk chunk1(desc1);
    Chunk chunk2(desc2);

    chunk2.set_block(LocalBlockPos(CHUNK_SIZE_X - 1, 5, 5), 42);
    chunk1.set_neighbor(HorizontalDirection::NegX, &chunk2);

    // Access block at x=-1 (which should map to neighbor's x=31)
    auto result = chunk1.get_entry_or_neighbor(LocalBlockPos(-1, 5, 5));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->block_id, 42);
}

TEST_F(ChunkTest, SerializeDeserialize) {
    ChunkDesc desc;
    desc.position = ChunkPos(5, 10);
    Chunk chunk(desc);

    // Add some blocks
    chunk.set_block(LocalBlockPos(0, 0, 0), 1);
    chunk.set_block(LocalBlockPos(15, 100, 15), 2);

    // Serialize
    std::vector<uint8_t> data = chunk.serialize();
    EXPECT_FALSE(data.empty());

    // Create new chunk and deserialize
    ChunkDesc desc2;
    desc2.position = ChunkPos(5, 10);
    Chunk chunk2(desc2);

    EXPECT_TRUE(chunk2.deserialize(data));

    // Verify data
    EXPECT_EQ(chunk2.get_block(LocalBlockPos(0, 0, 0)), 1);
    EXPECT_EQ(chunk2.get_block(LocalBlockPos(15, 100, 15)), 2);
    EXPECT_EQ(chunk2.get_block(LocalBlockPos(5, 5, 5)), BLOCK_AIR);
}

TEST_F(ChunkTest, Statistics) {
    ChunkDesc desc;
    Chunk chunk(desc);

    EXPECT_EQ(chunk.non_air_count(), 0u);

    chunk.set_block(LocalBlockPos(0, 0, 0), 1);
    chunk.set_block(LocalBlockPos(1, 0, 0), 1);
    chunk.set_block(LocalBlockPos(2, 0, 0), 1);

    EXPECT_EQ(chunk.non_air_count(), 3u);
    EXPECT_GT(chunk.memory_usage(), 0u);
}

TEST_F(ChunkTest, Metadata) {
    ChunkDesc desc;
    desc.seed = 12345;
    Chunk chunk(desc);

    EXPECT_EQ(chunk.get_metadata().generation_seed, 12345u);
    EXPECT_FALSE(chunk.get_metadata().has_been_generated);

    chunk.get_metadata_mut().has_been_generated = true;
    EXPECT_TRUE(chunk.get_metadata().has_been_generated);
}

TEST_F(ChunkTest, InitialState) {
    ChunkDesc desc;
    Chunk chunk(desc);

    // Initial state should be Unloaded
    EXPECT_EQ(chunk.get_state(), ChunkState::Unloaded);
    EXPECT_FALSE(chunk.is_ready());
    EXPECT_FALSE(chunk.is_dirty());
}

TEST_F(ChunkTest, MarkDirtyFromLoaded) {
    // This test verifies mark_dirty behavior
    // Note: State transitions to Loaded/Modified are handled by WorldManager
    ChunkDesc desc;
    Chunk chunk(desc);

    // Initial dirty state
    EXPECT_FALSE(chunk.is_dirty());

    // mark_dirty should set dirty flag
    chunk.mark_dirty();
    EXPECT_TRUE(chunk.is_dirty());

    // mark_clean should clear it
    chunk.mark_clean();
    EXPECT_FALSE(chunk.is_dirty());
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(ChunkTest, ConcurrentReads) {
    ChunkDesc desc;
    Chunk chunk(desc);

    // Set up some initial data
    for (int i = 0; i < 100; ++i) {
        chunk.set_block(LocalBlockPos(i % CHUNK_SIZE_X, i % CHUNK_SIZE_Y, 0), static_cast<BlockId>(i + 1));
    }

    // Launch multiple reader threads
    std::vector<std::thread> readers;
    std::atomic<int> errors{0};

    for (int t = 0; t < 4; ++t) {
        readers.emplace_back([&chunk, &errors, t]() {
            for (int i = 0; i < 1000; ++i) {
                auto lock = chunk.read_lock();
                int idx = (i + t) % 100;
                BlockId expected = static_cast<BlockId>(idx + 1);
                if (lock.get_block(LocalBlockPos(idx % CHUNK_SIZE_X, idx % CHUNK_SIZE_Y, 0)) != expected) {
                    ++errors;
                }
            }
        });
    }

    for (auto& t : readers) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);
}

TEST_F(ChunkTest, ConcurrentReadsAndWrites) {
    ChunkDesc desc;
    Chunk chunk(desc);

    std::atomic<bool> running{true};
    std::atomic<int> read_count{0};
    std::atomic<int> write_count{0};
    std::atomic<int> readers_ready{0};

    // Writer thread
    std::thread writer([&]() {
        // Wait for all readers to be ready before starting writes
        while (readers_ready.load() < 3) {
            std::this_thread::yield();
        }
        for (int i = 0; i < 100 && running; ++i) {
            chunk.set_block(LocalBlockPos(0, 0, 0), static_cast<BlockId>(i + 1));
            ++write_count;
            std::this_thread::yield();
        }
    });

    // Reader threads
    std::vector<std::thread> readers;
    for (int t = 0; t < 3; ++t) {
        readers.emplace_back([&]() {
            ++readers_ready;  // Signal ready before entering loop
            while (running && write_count < 50) {
                auto block = chunk.get_block(LocalBlockPos(0, 0, 0));
                (void)block;  // Just verify we can read
                ++read_count;
                std::this_thread::yield();
            }
        });
    }

    writer.join();
    running = false;

    for (auto& t : readers) {
        t.join();
    }

    EXPECT_EQ(write_count.load(), 100);
    EXPECT_GT(read_count.load(), 0);
}

}  // namespace
}  // namespace realcraft::world
