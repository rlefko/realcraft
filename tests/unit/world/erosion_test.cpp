// RealCraft World System Tests
// erosion_test.cpp - Tests for hydraulic erosion system

#include <gtest/gtest.h>

#include <cmath>
#include <realcraft/world/biome.hpp>
#include <realcraft/world/block.hpp>
#include <realcraft/world/erosion.hpp>
#include <realcraft/world/erosion_heightmap.hpp>
#include <realcraft/world/terrain_generator.hpp>
#include <vector>

namespace realcraft::world {
namespace {

class ErosionHeightmapTest : public ::testing::Test {
protected:
    // Test heightmap with known dimensions
    static constexpr int32_t kChunkWidth = 32;
    static constexpr int32_t kChunkHeight = 32;
    static constexpr int32_t kBorder = 8;
};

// Test: ErosionHeightmap dimensions are correct
TEST_F(ErosionHeightmapTest, DimensionsCorrect) {
    ErosionHeightmap map(kChunkWidth, kChunkHeight, kBorder);

    EXPECT_EQ(map.chunk_width(), kChunkWidth);
    EXPECT_EQ(map.chunk_height(), kChunkHeight);
    EXPECT_EQ(map.border(), kBorder);
    EXPECT_EQ(map.total_width(), kChunkWidth + 2 * kBorder);
    EXPECT_EQ(map.total_height(), kChunkHeight + 2 * kBorder);
}

// Test: Height get/set works correctly
TEST_F(ErosionHeightmapTest, HeightGetSet) {
    ErosionHeightmap map(kChunkWidth, kChunkHeight, kBorder);

    // Set some heights
    map.set(5, 5, 100.0f);
    map.set(10, 15, 75.5f);
    map.set(0, 0, 50.0f);

    // Verify they're stored correctly
    EXPECT_FLOAT_EQ(map.get(5, 5), 100.0f);
    EXPECT_FLOAT_EQ(map.get(10, 15), 75.5f);
    EXPECT_FLOAT_EQ(map.get(0, 0), 50.0f);
}

// Test: Invalid coordinates return 0
TEST_F(ErosionHeightmapTest, InvalidCoordsReturnZero) {
    ErosionHeightmap map(kChunkWidth, kChunkHeight, kBorder);
    map.set(5, 5, 100.0f);

    // Out of bounds should return 0
    EXPECT_FLOAT_EQ(map.get(-1, 0), 0.0f);
    EXPECT_FLOAT_EQ(map.get(0, -1), 0.0f);
    EXPECT_FLOAT_EQ(map.get(1000, 0), 0.0f);
    EXPECT_FLOAT_EQ(map.get(0, 1000), 0.0f);
}

// Test: Bilinear interpolation works correctly
TEST_F(ErosionHeightmapTest, BilinearInterpolation) {
    ErosionHeightmap map(kChunkWidth, kChunkHeight, kBorder);

    // Set up a simple 2x2 grid for interpolation testing
    map.set(10, 10, 0.0f);
    map.set(11, 10, 100.0f);
    map.set(10, 11, 100.0f);
    map.set(11, 11, 200.0f);

    // Center should be average of all 4 corners
    float center = map.sample_bilinear(10.5f, 10.5f);
    EXPECT_NEAR(center, 100.0f, 0.01f);

    // Corners should be exact values
    EXPECT_FLOAT_EQ(map.sample_bilinear(10.0f, 10.0f), 0.0f);
    EXPECT_FLOAT_EQ(map.sample_bilinear(11.0f, 10.0f), 100.0f);
    EXPECT_FLOAT_EQ(map.sample_bilinear(10.0f, 11.0f), 100.0f);
    EXPECT_FLOAT_EQ(map.sample_bilinear(11.0f, 11.0f), 200.0f);

    // Edge midpoints
    EXPECT_NEAR(map.sample_bilinear(10.5f, 10.0f), 50.0f, 0.01f);
    EXPECT_NEAR(map.sample_bilinear(10.5f, 11.0f), 150.0f, 0.01f);
}

// Test: Gradient calculation
TEST_F(ErosionHeightmapTest, GradientCalculation) {
    ErosionHeightmap map(kChunkWidth, kChunkHeight, kBorder);

    // Create a slope going up in +X direction
    for (int x = 0; x < map.total_width(); ++x) {
        for (int z = 0; z < map.total_height(); ++z) {
            map.set(x, z, static_cast<float>(x) * 10.0f);
        }
    }

    // Gradient at center should point in +X direction
    glm::vec3 gradient = map.calculate_gradient(20.0f, 20.0f);
    EXPECT_GT(gradient.x, 0.0f);
    EXPECT_NEAR(gradient.z, 0.0f, 0.1f);
}

// Test: Sediment tracking
TEST_F(ErosionHeightmapTest, SedimentTracking) {
    ErosionHeightmap map(kChunkWidth, kChunkHeight, kBorder);

    // Initially zero
    EXPECT_FLOAT_EQ(map.get_sediment(10, 10), 0.0f);

    // Add sediment
    map.add_sediment(10, 10, 5.0f);
    EXPECT_FLOAT_EQ(map.get_sediment(10, 10), 5.0f);

    map.add_sediment(10, 10, 3.0f);
    EXPECT_FLOAT_EQ(map.get_sediment(10, 10), 8.0f);
}

// Test: is_valid and is_in_core
TEST_F(ErosionHeightmapTest, ValidityChecks) {
    ErosionHeightmap map(kChunkWidth, kChunkHeight, kBorder);

    // Valid coordinates
    EXPECT_TRUE(map.is_valid(0, 0));
    EXPECT_TRUE(map.is_valid(kBorder, kBorder));
    EXPECT_TRUE(map.is_valid(map.total_width() - 1, map.total_height() - 1));

    // Invalid coordinates
    EXPECT_FALSE(map.is_valid(-1, 0));
    EXPECT_FALSE(map.is_valid(0, -1));
    EXPECT_FALSE(map.is_valid(map.total_width(), 0));
    EXPECT_FALSE(map.is_valid(0, map.total_height()));

    // Core area
    EXPECT_TRUE(map.is_in_core(kBorder, kBorder));
    EXPECT_TRUE(map.is_in_core(kBorder + kChunkWidth - 1, kBorder + kChunkHeight - 1));
    EXPECT_FALSE(map.is_in_core(0, 0));
    EXPECT_FALSE(map.is_in_core(kBorder - 1, kBorder));
}

class CPUErosionEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        BlockRegistry::instance().register_defaults();
        BiomeRegistry::instance().register_defaults();
    }
};

// Test: CPU erosion modifies terrain
TEST_F(CPUErosionEngineTest, ModifiesTerrain) {
    ErosionHeightmap map(32, 32, 8);

    // Create a simple slope
    for (int x = 0; x < map.total_width(); ++x) {
        for (int z = 0; z < map.total_height(); ++z) {
            map.set(x, z, 100.0f - static_cast<float>(x) * 0.5f);
        }
    }

    // Record initial heights
    std::vector<float> initial_heights = map.heights();

    // Run erosion with minimal droplets for quick test
    CPUErosionEngine engine;
    ErosionConfig config;
    config.enabled = true;
    config.particle.droplet_count = 1000;
    config.particle.max_lifetime = 20;

    EXPECT_TRUE(engine.is_available());
    engine.erode(map, config, 42);

    // Heights should have changed
    std::vector<float>& current_heights = map.heights();
    int changed_count = 0;
    for (size_t i = 0; i < initial_heights.size(); ++i) {
        if (std::abs(initial_heights[i] - current_heights[i]) > 0.0001f) {
            ++changed_count;
        }
    }

    EXPECT_GT(changed_count, 0) << "Erosion should modify terrain";
}

// Test: Erosion produces similar overall effects with same seed
// Note: Multi-threaded erosion may have minor variations due to float accumulation order,
// but the overall erosion effect should be statistically similar
TEST_F(CPUErosionEngineTest, SimilarResultsWithSameSeed) {
    ErosionConfig config;
    config.enabled = true;
    config.particle.droplet_count = 500;
    config.particle.max_lifetime = 15;

    CPUErosionEngine engine;

    // Create two identical heightmaps
    auto create_slope = [](ErosionHeightmap& map) {
        for (int x = 0; x < map.total_width(); ++x) {
            for (int z = 0; z < map.total_height(); ++z) {
                map.set(x, z, 100.0f - static_cast<float>(x + z) * 0.3f);
            }
        }
    };

    ErosionHeightmap map1(32, 32, 8);
    ErosionHeightmap map2(32, 32, 8);
    create_slope(map1);
    create_slope(map2);

    // Erode with same seed
    engine.erode(map1, config, 12345);
    engine.erode(map2, config, 12345);

    // Calculate statistics to verify similar overall erosion effects
    std::vector<float>& heights1 = map1.heights();
    std::vector<float>& heights2 = map2.heights();

    double sum1 = 0.0, sum2 = 0.0;
    double min1 = heights1[0], max1 = heights1[0];
    double min2 = heights2[0], max2 = heights2[0];

    for (size_t i = 0; i < heights1.size(); ++i) {
        sum1 += heights1[i];
        sum2 += heights2[i];
        min1 = std::min(min1, static_cast<double>(heights1[i]));
        max1 = std::max(max1, static_cast<double>(heights1[i]));
        min2 = std::min(min2, static_cast<double>(heights2[i]));
        max2 = std::max(max2, static_cast<double>(heights2[i]));
    }

    double mean1 = sum1 / static_cast<double>(heights1.size());
    double mean2 = sum2 / static_cast<double>(heights2.size());

    // Means should be very close (within 0.1%)
    EXPECT_NEAR(mean1, mean2, mean1 * 0.001) << "Mean heights should be nearly identical with same seed";

    // Min/max should be similar (within 1%)
    EXPECT_NEAR(min1, min2, std::abs(min1) * 0.01 + 0.1) << "Minimum heights should be similar";
    EXPECT_NEAR(max1, max2, std::abs(max1) * 0.01 + 0.1) << "Maximum heights should be similar";
}

class FlowAccumulationTest : public ::testing::Test {};

// Test: Flow accumulation produces reasonable values
TEST_F(FlowAccumulationTest, ProducesReasonableValues) {
    ErosionHeightmap map(32, 32, 8);

    // Create a simple slope going down in +X direction
    for (int x = 0; x < map.total_width(); ++x) {
        for (int z = 0; z < map.total_height(); ++z) {
            map.set(x, z, static_cast<float>(map.total_width() - x) * 10.0f);
        }
    }

    map.compute_flow_accumulation();

    // Flow should increase towards the right edge (lower height)
    float left_flow = map.get_flow(5, 20);
    float middle_flow = map.get_flow(20, 20);
    float right_flow = map.get_flow(map.total_width() - 5, 20);

    EXPECT_LT(left_flow, middle_flow) << "Flow should increase downslope";
    EXPECT_LT(middle_flow, right_flow) << "Flow should continue increasing";
}

// Test: Flat terrain has uniform flow
TEST_F(FlowAccumulationTest, FlatTerrainUniformFlow) {
    ErosionHeightmap map(32, 32, 8);

    // Flat terrain
    for (int x = 0; x < map.total_width(); ++x) {
        for (int z = 0; z < map.total_height(); ++z) {
            map.set(x, z, 50.0f);
        }
    }

    map.compute_flow_accumulation();

    // All cells should have flow = 1 (only self contribution)
    for (int z = 1; z < map.total_height() - 1; ++z) {
        for (int x = 1; x < map.total_width() - 1; ++x) {
            EXPECT_FLOAT_EQ(map.get_flow(x, z), 1.0f)
                << "Flat terrain should have uniform flow at (" << x << ", " << z << ")";
        }
    }
}

class RiverCarverTest : public ::testing::Test {};

// Test: River carver runs without crashing
TEST_F(RiverCarverTest, RunsWithoutCrashing) {
    ErosionHeightmap map(32, 32, 8);

    // Create terrain with a valley
    for (int x = 0; x < map.total_width(); ++x) {
        for (int z = 0; z < map.total_height(); ++z) {
            float center_dist = std::abs(static_cast<float>(z) - map.total_height() / 2.0f);
            map.set(x, z, 100.0f - static_cast<float>(x) * 2.0f + center_dist * 3.0f);
        }
    }

    map.compute_flow_accumulation();

    RiverCarver carver;
    ErosionConfig::RiverParams params;
    params.min_flow_accumulation = 10.0f;
    params.channel_depth_factor = 0.5f;
    params.channel_width_factor = 0.3f;
    params.smoothing_passes = 1;

    // Should not crash
    EXPECT_NO_THROW(carver.carve(map, params));
}

// Test: River carving lowers terrain at high flow points
TEST_F(RiverCarverTest, LowersTerrain) {
    ErosionHeightmap map(32, 32, 8);

    // Create a V-shaped valley
    for (int x = 0; x < map.total_width(); ++x) {
        for (int z = 0; z < map.total_height(); ++z) {
            float center_dist = std::abs(static_cast<float>(z) - map.total_height() / 2.0f);
            map.set(x, z, 100.0f - static_cast<float>(x) * 2.0f + center_dist * 2.0f);
        }
    }

    // Record heights before carving
    std::vector<float> before_heights = map.heights();

    map.compute_flow_accumulation();

    RiverCarver carver;
    ErosionConfig::RiverParams params;
    params.min_flow_accumulation = 5.0f;
    params.channel_depth_factor = 1.0f;
    params.channel_width_factor = 0.5f;
    params.smoothing_passes = 0;

    carver.carve(map, params);

    // Check that some terrain was lowered
    int lowered_count = 0;
    for (size_t i = 0; i < before_heights.size(); ++i) {
        if (map.heights()[i] < before_heights[i]) {
            ++lowered_count;
        }
    }

    EXPECT_GT(lowered_count, 0) << "River carving should lower some terrain";
}

class ErosionSimulatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        BlockRegistry::instance().register_defaults();
        BiomeRegistry::instance().register_defaults();
    }
};

// Test: ErosionSimulator initializes correctly
TEST_F(ErosionSimulatorTest, InitializesCorrectly) {
    ErosionSimulator sim(nullptr);  // CPU-only mode

    // Should not have GPU support without device
    EXPECT_FALSE(sim.has_gpu_support());
}

// Test: Full erosion simulation runs
TEST_F(ErosionSimulatorTest, FullSimulationRuns) {
    ErosionSimulator sim(nullptr);

    ErosionHeightmap map(32, 32, 16);

    // Create simple terrain
    for (int x = 0; x < map.total_width(); ++x) {
        for (int z = 0; z < map.total_height(); ++z) {
            map.set(x, z, 80.0f - static_cast<float>(x + z) * 0.5f);
        }
    }

    std::vector<float> before = map.heights();

    ErosionConfig config;
    config.enabled = true;
    config.particle.droplet_count = 1000;
    config.particle.max_lifetime = 20;
    config.river.enabled = true;
    config.river.min_flow_accumulation = 20.0f;

    sim.simulate(map, config, 42);

    // Verify terrain was modified
    int changed = 0;
    for (size_t i = 0; i < before.size(); ++i) {
        if (std::abs(before[i] - map.heights()[i]) > 0.001f) {
            ++changed;
        }
    }

    EXPECT_GT(changed, 0) << "Full simulation should modify terrain";
}

// Test: Erosion disabled does nothing
TEST_F(ErosionSimulatorTest, DisabledDoesNothing) {
    ErosionSimulator sim(nullptr);

    ErosionHeightmap map(32, 32, 8);

    for (int x = 0; x < map.total_width(); ++x) {
        for (int z = 0; z < map.total_height(); ++z) {
            map.set(x, z, 100.0f);
        }
    }

    std::vector<float> before = map.heights();

    ErosionConfig config;
    config.enabled = false;

    sim.simulate(map, config, 42);

    // Heights should be unchanged
    for (size_t i = 0; i < before.size(); ++i) {
        EXPECT_FLOAT_EQ(before[i], map.heights()[i]);
    }
}

}  // namespace
}  // namespace realcraft::world
