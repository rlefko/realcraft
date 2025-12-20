// RealCraft World System Tests
// chunk_border_test.cpp - Tests for cross-chunk border handling

#include <gtest/gtest.h>

#include <realcraft/world/biome.hpp>
#include <realcraft/world/block.hpp>
#include <realcraft/world/erosion_context.hpp>
#include <realcraft/world/erosion_heightmap.hpp>
#include <realcraft/world/terrain_generator.hpp>

namespace realcraft::world {
namespace {

class ErosionContextTest : public ::testing::Test {
protected:
    void SetUp() override {
        BlockRegistry::instance().register_defaults();
        BiomeRegistry::instance().register_defaults();
    }
};

// Test: ErosionContext stores and retrieves border data correctly
TEST_F(ErosionContextTest, StoresAndRetrievesBorderData) {
    ErosionContext context;

    ErosionBorderData data;
    data.source_chunk = ChunkPos(5, 10);
    data.direction = HorizontalDirection::PosX;
    data.width = 32;
    data.depth = 16;
    data.height_deltas.resize(static_cast<size_t>(data.width) * data.depth, 1.0f);
    data.sediment_values.resize(static_cast<size_t>(data.width) * data.depth, 0.5f);
    data.flow_values.resize(static_cast<size_t>(data.width) * data.depth, 10.0f);

    context.submit_border_data(data);

    // Should retrieve data for the neighbor looking in opposite direction
    // Chunk (5,10) exported PosX border, so chunk (6,10) looking NegX should find it
    EXPECT_TRUE(context.has_neighbor_data(ChunkPos(6, 10), HorizontalDirection::NegX));

    auto retrieved = context.get_neighbor_border(ChunkPos(6, 10), HorizontalDirection::NegX);
    ASSERT_TRUE(retrieved.has_value());

    EXPECT_EQ(retrieved->source_chunk, data.source_chunk);
    EXPECT_EQ(retrieved->width, data.width);
    EXPECT_EQ(retrieved->depth, data.depth);
    EXPECT_EQ(retrieved->height_deltas.size(), data.height_deltas.size());
    EXPECT_FLOAT_EQ(retrieved->height_deltas[0], 1.0f);
    EXPECT_FLOAT_EQ(retrieved->sediment_values[0], 0.5f);
    EXPECT_FLOAT_EQ(retrieved->flow_values[0], 10.0f);
}

// Test: ErosionContext returns nullopt for missing neighbors
TEST_F(ErosionContextTest, ReturnsNulloptForMissingNeighbors) {
    ErosionContext context;

    // No data submitted yet
    EXPECT_FALSE(context.has_neighbor_data(ChunkPos(0, 0), HorizontalDirection::PosX));

    auto result = context.get_neighbor_border(ChunkPos(0, 0), HorizontalDirection::PosX);
    EXPECT_FALSE(result.has_value());
}

// Test: All four directions work correctly
TEST_F(ErosionContextTest, AllDirectionsWork) {
    ErosionContext context;

    // Submit border data for all four directions from chunk (5, 5)
    for (int dir = 0; dir < 4; ++dir) {
        ErosionBorderData data;
        data.source_chunk = ChunkPos(5, 5);
        data.direction = static_cast<HorizontalDirection>(dir);
        data.width = 32;
        data.depth = 16;
        data.height_deltas.resize(static_cast<size_t>(data.width) * data.depth, static_cast<float>(dir + 1));
        data.sediment_values.resize(static_cast<size_t>(data.width) * data.depth, 0.0f);
        data.flow_values.resize(static_cast<size_t>(data.width) * data.depth, 0.0f);

        context.submit_border_data(data);
    }

    // Check each neighbor can access the appropriate border
    // NegX exported by (5,5) -> accessible by (4,5) looking PosX
    auto negx = context.get_neighbor_border(ChunkPos(4, 5), HorizontalDirection::PosX);
    ASSERT_TRUE(negx.has_value());
    EXPECT_FLOAT_EQ(negx->height_deltas[0], 1.0f);  // NegX = 0, so 0 + 1 = 1.0

    // PosX exported by (5,5) -> accessible by (6,5) looking NegX
    auto posx = context.get_neighbor_border(ChunkPos(6, 5), HorizontalDirection::NegX);
    ASSERT_TRUE(posx.has_value());
    EXPECT_FLOAT_EQ(posx->height_deltas[0], 2.0f);  // PosX = 1, so 1 + 1 = 2.0

    // NegZ exported by (5,5) -> accessible by (5,4) looking PosZ
    auto negz = context.get_neighbor_border(ChunkPos(5, 4), HorizontalDirection::PosZ);
    ASSERT_TRUE(negz.has_value());
    EXPECT_FLOAT_EQ(negz->height_deltas[0], 3.0f);  // NegZ = 2, so 2 + 1 = 3.0

    // PosZ exported by (5,5) -> accessible by (5,6) looking NegZ
    auto posz = context.get_neighbor_border(ChunkPos(5, 6), HorizontalDirection::NegZ);
    ASSERT_TRUE(posz.has_value());
    EXPECT_FLOAT_EQ(posz->height_deltas[0], 4.0f);  // PosZ = 3, so 3 + 1 = 4.0
}

// Test: Clear removes data
TEST_F(ErosionContextTest, ClearRemovesData) {
    ErosionContext context;

    ErosionBorderData data;
    data.source_chunk = ChunkPos(1, 1);
    data.direction = HorizontalDirection::PosX;
    data.width = 32;
    data.depth = 16;
    data.height_deltas.resize(static_cast<size_t>(data.width) * data.depth, 1.0f);
    data.sediment_values.resize(static_cast<size_t>(data.width) * data.depth, 0.0f);
    data.flow_values.resize(static_cast<size_t>(data.width) * data.depth, 0.0f);

    context.submit_border_data(data);
    EXPECT_EQ(context.stored_chunk_count(), 1);

    context.clear_chunk_data(ChunkPos(1, 1));
    EXPECT_EQ(context.stored_chunk_count(), 0);
    EXPECT_FALSE(context.has_neighbor_data(ChunkPos(2, 1), HorizontalDirection::NegX));

    // Clear all
    context.submit_border_data(data);
    context.clear_all();
    EXPECT_EQ(context.stored_chunk_count(), 0);
}

class ErosionBorderExchangeTest : public ::testing::Test {
protected:
    static constexpr int32_t kChunkSize = 32;
    static constexpr int32_t kBorder = 16;

    void SetUp() override {
        BlockRegistry::instance().register_defaults();
        BiomeRegistry::instance().register_defaults();
    }
};

// Test: Export and import border heights
TEST_F(ErosionBorderExchangeTest, ExportImportHeightDeltas) {
    ErosionHeightmap map1(kChunkSize, kChunkSize, kBorder);
    ErosionHeightmap map2(kChunkSize, kChunkSize, kBorder);

    // Set up initial heights
    for (int x = 0; x < map1.total_width(); ++x) {
        for (int z = 0; z < map1.total_height(); ++z) {
            map1.set(x, z, 50.0f);
            map2.set(x, z, 50.0f);
        }
    }

    // Store original heights for delta calculation
    map1.store_original_heights();

    // Simulate erosion on map1 - add some height changes to the PosX border
    // PosX border: x in [border + chunk_width, total_width)
    const int32_t border_start_x = kBorder + kChunkSize;
    for (int x = border_start_x; x < map1.total_width(); ++x) {
        for (int z = kBorder; z < kBorder + kChunkSize; ++z) {
            map1.add_height(x, z, -5.0f);  // Erode by 5 units
        }
    }

    // Export the PosX border from map1
    auto border_data = map1.export_border_data(HorizontalDirection::PosX);

    EXPECT_EQ(border_data.direction, HorizontalDirection::PosX);
    EXPECT_EQ(border_data.width, kBorder);
    EXPECT_EQ(border_data.depth, kChunkSize);

    // Verify all deltas are -5.0
    for (size_t i = 0; i < border_data.height_deltas.size(); ++i) {
        EXPECT_FLOAT_EQ(border_data.height_deltas[i], -5.0f) << "Delta at index " << i;
    }

    // Import into map2's NegX border (the corresponding region)
    map2.import_border_heights(HorizontalDirection::NegX, border_data.height_deltas);

    // Verify the NegX border (x in [0, border)) was updated
    for (int x = 0; x < kBorder; ++x) {
        for (int z = kBorder; z < kBorder + kChunkSize; ++z) {
            float expected = 50.0f - 5.0f;  // Original 50 + delta -5
            EXPECT_FLOAT_EQ(map2.get(x, z), expected) << "Height at (" << x << ", " << z << ")";
        }
    }
}

// Test: Flow import accumulates correctly
TEST_F(ErosionBorderExchangeTest, FlowImportAccumulates) {
    ErosionHeightmap map(kChunkSize, kChunkSize, kBorder);

    // Initialize with flat terrain and compute flow (all cells get flow=1)
    for (int x = 0; x < map.total_width(); ++x) {
        for (int z = 0; z < map.total_height(); ++z) {
            map.set(x, z, 50.0f);
        }
    }
    map.compute_flow_accumulation();

    // All cells should have flow=1 (flat terrain)
    EXPECT_FLOAT_EQ(map.get_flow(5, kBorder + 5), 1.0f);

    // Import additional flow from neighbor
    std::vector<float> imported_flow(static_cast<size_t>(kBorder) * kChunkSize, 100.0f);
    map.import_border_flow(HorizontalDirection::NegX, imported_flow);

    // NegX border should now have flow = 1 + 100 = 101
    for (int x = 0; x < kBorder; ++x) {
        for (int z = kBorder; z < kBorder + kChunkSize; ++z) {
            EXPECT_FLOAT_EQ(map.get_flow(x, z), 101.0f) << "Flow at (" << x << ", " << z << ")";
        }
    }
}

// Test: Sediment import works
TEST_F(ErosionBorderExchangeTest, SedimentImportWorks) {
    ErosionHeightmap map(kChunkSize, kChunkSize, kBorder);

    // Initially zero sediment
    EXPECT_FLOAT_EQ(map.get_sediment(5, kBorder + 5), 0.0f);

    // Import sediment data
    std::vector<float> imported_sediment(static_cast<size_t>(kBorder) * kChunkSize, 2.5f);
    map.import_border_sediment(HorizontalDirection::NegX, imported_sediment);

    // NegX border should have the imported sediment
    for (int x = 0; x < kBorder; ++x) {
        for (int z = kBorder; z < kBorder + kChunkSize; ++z) {
            EXPECT_FLOAT_EQ(map.get_sediment(x, z), 2.5f) << "Sediment at (" << x << ", " << z << ")";
        }
    }
}

// Test: Export all four borders produces correct dimensions
TEST_F(ErosionBorderExchangeTest, ExportAllBordersCorrectDimensions) {
    ErosionHeightmap map(kChunkSize, kChunkSize, kBorder);
    map.store_original_heights();

    // NegX and PosX borders: width=border, height=chunk_height
    auto negx = map.export_border_data(HorizontalDirection::NegX);
    EXPECT_EQ(negx.width, kBorder);
    EXPECT_EQ(negx.depth, kChunkSize);

    auto posx = map.export_border_data(HorizontalDirection::PosX);
    EXPECT_EQ(posx.width, kBorder);
    EXPECT_EQ(posx.depth, kChunkSize);

    // NegZ and PosZ borders: width=chunk_width, height=border
    auto negz = map.export_border_data(HorizontalDirection::NegZ);
    EXPECT_EQ(negz.width, kChunkSize);
    EXPECT_EQ(negz.depth, kBorder);

    auto posz = map.export_border_data(HorizontalDirection::PosZ);
    EXPECT_EQ(posz.width, kChunkSize);
    EXPECT_EQ(posz.depth, kBorder);
}

class BiomeBlendTest : public ::testing::Test {
protected:
    void SetUp() override {
        BlockRegistry::instance().register_defaults();
        BiomeRegistry::instance().register_defaults();
    }
};

// Test: Blended block selection is deterministic
TEST_F(BiomeBlendTest, DeterministicBlockSelection) {
    TerrainConfig config;
    config.seed = 12345;
    config.biome_system.enabled = true;

    TerrainGenerator gen(config);

    // Get biome blend at a specific location
    auto blend1 = gen.get_biome_blend(1000, 1000);
    auto blend2 = gen.get_biome_blend(1000, 1000);

    // Same coordinates should give identical results
    EXPECT_EQ(blend1.primary_biome, blend2.primary_biome);
    EXPECT_EQ(blend1.secondary_biome, blend2.secondary_biome);
    EXPECT_FLOAT_EQ(blend1.blend_factor, blend2.blend_factor);
}

// Test: Biome blend factor is in valid range
TEST_F(BiomeBlendTest, BlendFactorInValidRange) {
    TerrainConfig config;
    config.seed = 42;
    config.biome_system.enabled = true;

    TerrainGenerator gen(config);

    // Check multiple locations
    for (int64_t x = -1000; x <= 1000; x += 500) {
        for (int64_t z = -1000; z <= 1000; z += 500) {
            auto blend = gen.get_biome_blend(x, z);
            EXPECT_GE(blend.blend_factor, 0.0f) << "Blend factor at (" << x << ", " << z << ")";
            EXPECT_LE(blend.blend_factor, 1.0f) << "Blend factor at (" << x << ", " << z << ")";
        }
    }
}

class TerrainGeneratorContextTest : public ::testing::Test {
protected:
    void SetUp() override {
        BlockRegistry::instance().register_defaults();
        BiomeRegistry::instance().register_defaults();
    }
};

// Test: TerrainGenerator accepts erosion context
TEST_F(TerrainGeneratorContextTest, AcceptsErosionContext) {
    TerrainConfig config;
    config.seed = 42;
    config.erosion.enabled = true;
    config.erosion.particle.droplet_count = 100;

    TerrainGenerator gen(config);
    ErosionContext context;

    gen.set_erosion_context(&context);
    EXPECT_EQ(gen.get_erosion_context(), &context);

    // Clear context
    gen.set_erosion_context(nullptr);
    EXPECT_EQ(gen.get_erosion_context(), nullptr);
}

// Test: Generation with context populates context
TEST_F(TerrainGeneratorContextTest, GenerationPopulatesContext) {
    TerrainConfig config;
    config.seed = 42;
    config.erosion.enabled = true;
    config.erosion.particle.droplet_count = 100;
    config.erosion.border_size = 8;

    TerrainGenerator gen(config);
    ErosionContext context;
    gen.set_erosion_context(&context);

    // Generate a chunk
    ChunkDesc desc;
    desc.position = ChunkPos(0, 0);
    Chunk chunk(desc);

    gen.generate(chunk);

    // Context should have border data for this chunk
    EXPECT_EQ(context.stored_chunk_count(), 1);

    // Neighboring chunks should be able to access the border data
    EXPECT_TRUE(context.has_neighbor_data(ChunkPos(1, 0), HorizontalDirection::NegX));
    EXPECT_TRUE(context.has_neighbor_data(ChunkPos(-1, 0), HorizontalDirection::PosX));
    EXPECT_TRUE(context.has_neighbor_data(ChunkPos(0, 1), HorizontalDirection::NegZ));
    EXPECT_TRUE(context.has_neighbor_data(ChunkPos(0, -1), HorizontalDirection::PosZ));
}

}  // namespace
}  // namespace realcraft::world
