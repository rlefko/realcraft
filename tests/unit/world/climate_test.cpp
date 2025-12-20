// RealCraft World System Tests
// climate_test.cpp - Tests for ClimateMap class

#include <gtest/gtest.h>

#include <atomic>
#include <cmath>
#include <realcraft/world/biome.hpp>
#include <realcraft/world/block.hpp>
#include <realcraft/world/climate.hpp>
#include <thread>
#include <vector>

namespace realcraft::world {
namespace {

class ClimateTest : public ::testing::Test {
protected:
    void SetUp() override {
        BlockRegistry::instance().register_defaults();
        BiomeRegistry::instance().register_defaults();
    }
};

// Test: Same seed produces same climate (deterministic)
TEST_F(ClimateTest, DeterministicClimate) {
    ClimateConfig config;
    config.seed = 12345;
    ClimateMap map1(config);
    ClimateMap map2(config);

    // Same coordinates with same seed should produce identical results
    for (int i = 0; i < 100; ++i) {
        int64_t x = i * 100 - 5000;
        int64_t z = i * 50 - 2500;

        EXPECT_FLOAT_EQ(map1.get_raw_temperature(x, z), map2.get_raw_temperature(x, z))
            << "Temperature mismatch at (" << x << ", " << z << ")";
        EXPECT_FLOAT_EQ(map1.get_raw_humidity(x, z), map2.get_raw_humidity(x, z))
            << "Humidity mismatch at (" << x << ", " << z << ")";
    }
}

// Test: Different seeds produce different climate
TEST_F(ClimateTest, DifferentSeedsProduceDifferentClimate) {
    ClimateConfig config1;
    config1.seed = 12345;
    ClimateConfig config2;
    config2.seed = 54321;

    ClimateMap map1(config1);
    ClimateMap map2(config2);

    int different_temp = 0;
    int different_humid = 0;

    for (int i = 0; i < 100; ++i) {
        int64_t x = i * 100;
        int64_t z = i * 100;

        if (std::abs(map1.get_raw_temperature(x, z) - map2.get_raw_temperature(x, z)) > 0.01f) {
            ++different_temp;
        }
        if (std::abs(map1.get_raw_humidity(x, z) - map2.get_raw_humidity(x, z)) > 0.01f) {
            ++different_humid;
        }
    }

    EXPECT_GE(different_temp, 80) << "Temperature too similar with different seeds";
    EXPECT_GE(different_humid, 80) << "Humidity too similar with different seeds";
}

// Test: Temperature and humidity are in [0, 1] range
TEST_F(ClimateTest, ClimateValuesInRange) {
    ClimateConfig config;
    config.seed = 42;
    ClimateMap map(config);

    for (int x = -500; x < 500; x += 50) {
        for (int z = -500; z < 500; z += 50) {
            ClimateSample sample = map.sample(x, z, 70.0f);

            EXPECT_GE(sample.temperature, 0.0f) << "Temperature below 0 at (" << x << ", " << z << ")";
            EXPECT_LE(sample.temperature, 1.0f) << "Temperature above 1 at (" << x << ", " << z << ")";
            EXPECT_GE(sample.humidity, 0.0f) << "Humidity below 0 at (" << x << ", " << z << ")";
            EXPECT_LE(sample.humidity, 1.0f) << "Humidity above 1 at (" << x << ", " << z << ")";
        }
    }
}

// Test: Altitude reduces temperature
TEST_F(ClimateTest, AltitudeReducesTemperature) {
    ClimateConfig config;
    config.seed = 12345;
    config.altitude_temperature_factor = 0.015f;
    config.sea_level = 64.0f;
    ClimateMap map(config);

    // Sample at same x,z but different heights
    int64_t x = 1000;
    int64_t z = 1000;

    ClimateSample low = map.sample(x, z, 64.0f);    // At sea level
    ClimateSample high = map.sample(x, z, 164.0f);  // 100 blocks above sea level

    // Higher altitude should be colder
    EXPECT_LT(high.temperature, low.temperature) << "Temperature should decrease with altitude";

    // Temperature drop is limited by clamping to [0, 1] range
    // Just verify the drop is positive and reasonable
    float actual_drop = low.temperature - high.temperature;
    EXPECT_GT(actual_drop, 0.0f) << "Temperature should drop with altitude";
    // Max possible drop is low.temperature (since high.temperature >= 0)
    EXPECT_LE(actual_drop, low.temperature);
}

// Test: Below sea level altitude doesn't affect temperature
TEST_F(ClimateTest, BelowSeaLevelNoAltitudeEffect) {
    ClimateConfig config;
    config.seed = 12345;
    config.sea_level = 64.0f;
    ClimateMap map(config);

    int64_t x = 500;
    int64_t z = 500;

    ClimateSample at_sea = map.sample(x, z, 64.0f);
    ClimateSample below = map.sample(x, z, 30.0f);

    // Below sea level should have same temperature as at sea level
    EXPECT_FLOAT_EQ(at_sea.temperature, below.temperature);
}

// Test: get_biome returns a valid biome
TEST_F(ClimateTest, GetBiomeReturnsValid) {
    ClimateConfig config;
    config.seed = 42;
    ClimateMap map(config);

    for (int i = 0; i < 100; ++i) {
        BiomeType biome = map.get_biome(i * 100, i * 100, 70.0f);
        EXPECT_LT(static_cast<size_t>(biome), BIOME_COUNT) << "Invalid biome at sample " << i;
    }
}

// Test: Thread-safe concurrent sampling
TEST_F(ClimateTest, ThreadSafeSampling) {
    ClimateConfig config;
    config.seed = 99999;
    ClimateMap map(config);

    // Pre-compute expected values
    std::vector<float> expected_temps(100);
    std::vector<float> expected_humids(100);
    for (int i = 0; i < 100; ++i) {
        expected_temps[i] = map.get_raw_temperature(i * 100, i * 100);
        expected_humids[i] = map.get_raw_humidity(i * 100, i * 100);
    }

    std::atomic<bool> all_match{true};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&map, &expected_temps, &expected_humids, &all_match]() {
            for (int i = 0; i < 100; ++i) {
                float temp = map.get_raw_temperature(i * 100, i * 100);
                float humid = map.get_raw_humidity(i * 100, i * 100);
                if (std::abs(temp - expected_temps[i]) > 0.0001f || std::abs(humid - expected_humids[i]) > 0.0001f) {
                    all_match = false;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_TRUE(all_match.load()) << "Concurrent sampling returned inconsistent results";
}

// Test: Blend factor is in [0, 1] range
TEST_F(ClimateTest, BlendFactorInRange) {
    ClimateConfig config;
    config.seed = 42;
    config.blend_radius = 4;
    ClimateMap map(config);

    for (int x = -100; x < 100; x += 10) {
        for (int z = -100; z < 100; z += 10) {
            BiomeBlend blend = map.sample_blended(x, z, 70.0f);

            EXPECT_GE(blend.blend_factor, 0.0f) << "Blend factor below 0 at (" << x << ", " << z << ")";
            EXPECT_LE(blend.blend_factor, 1.0f) << "Blend factor above 1 at (" << x << ", " << z << ")";
        }
    }
}

// Test: Sample returns same biome as get_biome
TEST_F(ClimateTest, SampleMatchesGetBiome) {
    ClimateConfig config;
    config.seed = 12345;
    ClimateMap map(config);

    for (int i = 0; i < 50; ++i) {
        int64_t x = i * 200 - 5000;
        int64_t z = i * 150 - 3750;
        float height = 70.0f;

        ClimateSample sample = map.sample(x, z, height);
        BiomeType biome = map.get_biome(x, z, height);

        EXPECT_EQ(sample.biome, biome) << "Biome mismatch at (" << x << ", " << z << ")";
    }
}

// Test: Configuration can be changed and rebuilt
TEST_F(ClimateTest, ConfigChangeAfterRebuild) {
    ClimateConfig config;
    config.seed = 111;
    ClimateMap map(config);

    // Sample at a location away from origin for more variance
    float temp1 = map.get_raw_temperature(1000, 2000);

    // Change seed and rebuild
    ClimateConfig new_config;
    new_config.seed = 999999;  // Very different seed
    map.set_config(new_config);
    map.rebuild_nodes();

    float temp2 = map.get_raw_temperature(1000, 2000);

    // Different seeds should produce different temperatures (statistically very unlikely to be equal)
    EXPECT_NE(temp1, temp2) << "Temperature should change after config rebuild";
}

// Test: Blend with radius 0 returns only primary biome
TEST_F(ClimateTest, BlendRadiusZeroNoBlending) {
    ClimateConfig config;
    config.seed = 42;
    config.blend_radius = 0;
    ClimateMap map(config);

    for (int i = 0; i < 20; ++i) {
        BiomeBlend blend = map.sample_blended(i * 100, i * 100, 70.0f);

        // With radius 0, blend factor should be 0 (no blending)
        EXPECT_FLOAT_EQ(blend.blend_factor, 0.0f);
    }
}

// Test: Large coordinates work correctly
TEST_F(ClimateTest, LargeCoordinates) {
    ClimateConfig config;
    config.seed = 42;
    ClimateMap map(config);

    int64_t large_x = 1000000;
    int64_t large_z = -1000000;

    ClimateSample sample = map.sample(large_x, large_z, 70.0f);

    EXPECT_GE(sample.temperature, 0.0f);
    EXPECT_LE(sample.temperature, 1.0f);
    EXPECT_GE(sample.humidity, 0.0f);
    EXPECT_LE(sample.humidity, 1.0f);

    // Should be deterministic
    ClimateSample sample2 = map.sample(large_x, large_z, 70.0f);
    EXPECT_FLOAT_EQ(sample.temperature, sample2.temperature);
    EXPECT_FLOAT_EQ(sample.humidity, sample2.humidity);
}

// Test: Biomes at same height are affected by climate
TEST_F(ClimateTest, ClimateAffectsBiome) {
    ClimateConfig config;
    config.seed = 42;
    ClimateMap map(config);

    // Sample many points and verify we get different biomes based on climate
    std::set<BiomeType> found_biomes;
    for (int x = 0; x < 10000; x += 100) {
        for (int z = 0; z < 10000; z += 100) {
            BiomeType biome = map.get_biome(x, z, 80.0f);  // Above sea level
            found_biomes.insert(biome);
        }
    }

    // Should find multiple biome types due to climate variation
    EXPECT_GE(found_biomes.size(), 3u) << "Expected to find multiple biome types";
}

}  // namespace
}  // namespace realcraft::world
