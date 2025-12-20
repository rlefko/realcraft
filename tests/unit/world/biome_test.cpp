// RealCraft World System Tests
// biome_test.cpp - Tests for BiomeType and BiomeRegistry

#include <gtest/gtest.h>

#include <realcraft/world/biome.hpp>
#include <realcraft/world/block.hpp>

namespace realcraft::world {
namespace {

class BiomeTest : public ::testing::Test {
protected:
    void SetUp() override {
        BlockRegistry::instance().register_defaults();
        BiomeRegistry::instance().register_defaults();
    }
};

// Test: String conversion round-trip
TEST_F(BiomeTest, StringConversionRoundTrip) {
    for (size_t i = 0; i < BIOME_COUNT; ++i) {
        BiomeType type = static_cast<BiomeType>(i);
        const char* name = biome_type_to_string(type);
        BiomeType parsed = biome_type_from_string(name);
        EXPECT_EQ(type, parsed) << "Failed for biome index " << i;
    }
}

// Test: All biome types have valid string names
TEST_F(BiomeTest, AllBiomesHaveNames) {
    for (size_t i = 0; i < BIOME_COUNT; ++i) {
        BiomeType type = static_cast<BiomeType>(i);
        const char* name = biome_type_to_string(type);
        EXPECT_NE(name, nullptr);
        EXPECT_STRNE(name, "unknown") << "Missing name for biome index " << i;
    }
}

// Test: Unknown string returns default biome
TEST_F(BiomeTest, UnknownStringReturnsDefault) {
    BiomeType result = biome_type_from_string("nonexistent_biome");
    EXPECT_EQ(result, BiomeType::Plains);
}

// Test: Registry is initialized after register_defaults
TEST_F(BiomeTest, RegistryIsInitialized) {
    EXPECT_TRUE(BiomeRegistry::instance().is_initialized());
}

// Test: All biome configs have valid data
TEST_F(BiomeTest, AllBiomeConfigsValid) {
    const auto& registry = BiomeRegistry::instance();

    for (size_t i = 0; i < BIOME_COUNT; ++i) {
        BiomeType type = static_cast<BiomeType>(i);
        const auto& config = registry.get(type);

        EXPECT_EQ(config.type, type) << "Type mismatch for biome " << i;
        EXPECT_FALSE(config.name.empty()) << "Empty name for biome " << i;
        EXPECT_FALSE(config.display_name.empty()) << "Empty display_name for biome " << i;
    }
}

// Test: All biome palettes have valid block IDs
TEST_F(BiomeTest, AllPalettesHaveValidBlocks) {
    const auto& registry = BiomeRegistry::instance();

    for (size_t i = 0; i < BIOME_COUNT; ++i) {
        BiomeType type = static_cast<BiomeType>(i);
        const auto& palette = registry.get_palette(type);

        EXPECT_NE(palette.surface_block, BLOCK_INVALID) << "Invalid surface_block for biome " << i;
        EXPECT_NE(palette.subsurface_block, BLOCK_INVALID) << "Invalid subsurface_block for biome " << i;
        EXPECT_NE(palette.underwater_block, BLOCK_INVALID) << "Invalid underwater_block for biome " << i;
        EXPECT_NE(palette.stone_block, BLOCK_INVALID) << "Invalid stone_block for biome " << i;
    }
}

// Test: Desert biome uses sand
TEST_F(BiomeTest, DesertUsesSand) {
    const auto& palette = BiomeRegistry::instance().get_palette(BiomeType::Desert);
    BlockId sand_id = BlockRegistry::instance().sand_id();

    EXPECT_EQ(palette.surface_block, sand_id);
    EXPECT_EQ(palette.subsurface_block, sand_id);
}

// Test: Plains biome uses grass/dirt
TEST_F(BiomeTest, PlainsUsesGrassDirt) {
    const auto& palette = BiomeRegistry::instance().get_palette(BiomeType::Plains);
    BlockId grass_id = BlockRegistry::instance().grass_id();
    BlockId dirt_id = BlockRegistry::instance().dirt_id();

    EXPECT_EQ(palette.surface_block, grass_id);
    EXPECT_EQ(palette.subsurface_block, dirt_id);
}

// Test: Mountains biome uses stone
TEST_F(BiomeTest, MountainsUsesStone) {
    const auto& palette = BiomeRegistry::instance().get_palette(BiomeType::Mountains);
    BlockId stone_id = BlockRegistry::instance().stone_id();

    EXPECT_EQ(palette.surface_block, stone_id);
    EXPECT_EQ(palette.subsurface_block, stone_id);
}

// Test: find_biome returns correct biome for known climate values
TEST_F(BiomeTest, FindBiomeDesertConditions) {
    // Hot (0.9) and dry (0.1) should give Desert
    BiomeType result = BiomeRegistry::instance().find_biome(0.9f, 0.1f, 70.0f);
    EXPECT_EQ(result, BiomeType::Desert);
}

TEST_F(BiomeTest, FindBiomePlainsConditions) {
    // Moderate temperature and humidity should give Plains or Forest
    BiomeType result = BiomeRegistry::instance().find_biome(0.5f, 0.45f, 70.0f);
    // Plains: temp 0.3-0.7, humidity 0.3-0.6
    EXPECT_EQ(result, BiomeType::Plains);
}

TEST_F(BiomeTest, FindBiomeTundraConditions) {
    // Very cold should give Tundra
    BiomeType result = BiomeRegistry::instance().find_biome(0.1f, 0.3f, 70.0f);
    EXPECT_EQ(result, BiomeType::Tundra);
}

TEST_F(BiomeTest, FindBiomeMountainsHighAltitude) {
    // High altitude (> 100) should give Mountains
    BiomeType result = BiomeRegistry::instance().find_biome(0.5f, 0.5f, 150.0f);
    EXPECT_EQ(result, BiomeType::Mountains);
}

TEST_F(BiomeTest, FindBiomeSnowyMountainsHighAltitudeCold) {
    // High altitude and cold should give SnowyMountains
    BiomeType result = BiomeRegistry::instance().find_biome(0.1f, 0.5f, 150.0f);
    EXPECT_EQ(result, BiomeType::SnowyMountains);
}

TEST_F(BiomeTest, FindBiomeOceanLowAltitude) {
    // Below sea level should give Ocean
    BiomeType result = BiomeRegistry::instance().find_biome(0.5f, 0.5f, 50.0f);
    EXPECT_EQ(result, BiomeType::Ocean);
}

TEST_F(BiomeTest, FindBiomeBeachAtSeaLevel) {
    // At or slightly above sea level should give Beach
    BiomeType result = BiomeRegistry::instance().find_biome(0.5f, 0.5f, 66.0f);
    EXPECT_EQ(result, BiomeType::Beach);
}

// Test: Height modifiers have reasonable default values
TEST_F(BiomeTest, HeightModifiersReasonable) {
    const auto& registry = BiomeRegistry::instance();

    for (size_t i = 0; i < BIOME_COUNT; ++i) {
        BiomeType type = static_cast<BiomeType>(i);
        const auto& mods = registry.get_height_modifiers(type);

        EXPECT_GT(mods.height_scale, 0.0f) << "Invalid height_scale for biome " << i;
        EXPECT_GE(mods.detail_scale, 0.0f) << "Negative detail_scale for biome " << i;
    }
}

// Test: blend_height_modifiers function
TEST_F(BiomeTest, BlendHeightModifiersZero) {
    BiomeHeightModifiers a{1.0f, 0.0f, 1.0f, 1.0f, 1.0f};
    BiomeHeightModifiers b{2.0f, 10.0f, 0.5f, 0.5f, 0.5f};

    BiomeHeightModifiers result = blend_height_modifiers(a, b, 0.0f);

    EXPECT_FLOAT_EQ(result.height_scale, 1.0f);
    EXPECT_FLOAT_EQ(result.height_offset, 0.0f);
}

TEST_F(BiomeTest, BlendHeightModifiersOne) {
    BiomeHeightModifiers a{1.0f, 0.0f, 1.0f, 1.0f, 1.0f};
    BiomeHeightModifiers b{2.0f, 10.0f, 0.5f, 0.5f, 0.5f};

    BiomeHeightModifiers result = blend_height_modifiers(a, b, 1.0f);

    EXPECT_FLOAT_EQ(result.height_scale, 2.0f);
    EXPECT_FLOAT_EQ(result.height_offset, 10.0f);
}

TEST_F(BiomeTest, BlendHeightModifiersHalf) {
    BiomeHeightModifiers a{1.0f, 0.0f, 1.0f, 1.0f, 1.0f};
    BiomeHeightModifiers b{2.0f, 10.0f, 0.5f, 0.5f, 0.5f};

    BiomeHeightModifiers result = blend_height_modifiers(a, b, 0.5f);

    EXPECT_FLOAT_EQ(result.height_scale, 1.5f);
    EXPECT_FLOAT_EQ(result.height_offset, 5.0f);
}

// Test: get_all returns correct number of biomes
TEST_F(BiomeTest, GetAllReturnsAllBiomes) {
    const auto& all = BiomeRegistry::instance().get_all();
    EXPECT_EQ(all.size(), BIOME_COUNT);
}

}  // namespace
}  // namespace realcraft::world
