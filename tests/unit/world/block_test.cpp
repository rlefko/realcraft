// RealCraft World System Tests
// block_test.cpp - Tests for BlockType and BlockRegistry

#include <gtest/gtest.h>

#include <realcraft/world/block.hpp>

namespace realcraft::world {
namespace {

class BlockRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure defaults are registered
        BlockRegistry::instance().register_defaults();
    }
};

TEST_F(BlockRegistryTest, AirBlockExists) {
    const BlockType* air = BlockRegistry::instance().get(BLOCK_AIR);
    ASSERT_NE(air, nullptr);
    EXPECT_EQ(air->get_id(), BLOCK_AIR);
    EXPECT_TRUE(air->is_air());
    EXPECT_TRUE(air->is_transparent());
    EXPECT_FALSE(air->is_solid());
}

TEST_F(BlockRegistryTest, DefaultBlocksExist) {
    auto& registry = BlockRegistry::instance();

    // Check all default blocks exist
    EXPECT_NE(registry.get(registry.stone_id()), nullptr);
    EXPECT_NE(registry.get(registry.dirt_id()), nullptr);
    EXPECT_NE(registry.get(registry.grass_id()), nullptr);
    EXPECT_NE(registry.get(registry.water_id()), nullptr);
    EXPECT_NE(registry.get(registry.sand_id()), nullptr);
}

TEST_F(BlockRegistryTest, StoneProperties) {
    const BlockType* stone = BlockRegistry::instance().get(BlockRegistry::instance().stone_id());
    ASSERT_NE(stone, nullptr);

    EXPECT_EQ(stone->get_name(), "realcraft:stone");
    EXPECT_TRUE(stone->is_solid());
    EXPECT_TRUE(stone->is_full_cube());
    EXPECT_TRUE(stone->has_collision());
    EXPECT_TRUE(stone->is_breakable());
    EXPECT_FALSE(stone->is_transparent());
    EXPECT_FALSE(stone->is_liquid());
}

TEST_F(BlockRegistryTest, WaterProperties) {
    const BlockType* water = BlockRegistry::instance().get(BlockRegistry::instance().water_id());
    ASSERT_NE(water, nullptr);

    EXPECT_TRUE(water->is_transparent());
    EXPECT_TRUE(water->is_liquid());
    EXPECT_FALSE(water->is_solid());
}

TEST_F(BlockRegistryTest, SandHasGravity) {
    const BlockType* sand = BlockRegistry::instance().get(BlockRegistry::instance().sand_id());
    ASSERT_NE(sand, nullptr);

    EXPECT_TRUE(sand->has_gravity());
}

TEST_F(BlockRegistryTest, LookupByName) {
    const BlockType* stone = BlockRegistry::instance().get("realcraft:stone");
    ASSERT_NE(stone, nullptr);
    EXPECT_EQ(stone->get_name(), "realcraft:stone");
}

TEST_F(BlockRegistryTest, FindIdByName) {
    auto id = BlockRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(*id, BlockRegistry::instance().stone_id());
}

TEST_F(BlockRegistryTest, InvalidLookupReturnsNull) {
    const BlockType* invalid = BlockRegistry::instance().get(BLOCK_INVALID);
    EXPECT_EQ(invalid, nullptr);

    const BlockType* nonexistent = BlockRegistry::instance().get("realcraft:nonexistent");
    EXPECT_EQ(nonexistent, nullptr);
}

TEST_F(BlockRegistryTest, ForEachIteratesAllBlocks) {
    size_t count = 0;
    BlockRegistry::instance().for_each([&count](const BlockType& block) {
        ++count;
        EXPECT_NE(block.get_id(), BLOCK_INVALID);
    });

    EXPECT_EQ(count, BlockRegistry::instance().count());
    EXPECT_GE(count, 10u);  // At least the default blocks
}

TEST_F(BlockRegistryTest, TextureIndexByFace) {
    const BlockType* grass = BlockRegistry::instance().get(BlockRegistry::instance().grass_id());
    ASSERT_NE(grass, nullptr);

    // Grass has different top texture
    uint16_t top_tex = grass->get_texture_index(Direction::PosY);
    uint16_t side_tex = grass->get_texture_index(Direction::PosX);
    uint16_t bottom_tex = grass->get_texture_index(Direction::NegY);

    EXPECT_NE(top_tex, side_tex);    // Top is grass texture, sides are different
    EXPECT_NE(top_tex, bottom_tex);  // Bottom is dirt texture
}

TEST_F(BlockRegistryTest, MaterialProperties) {
    const BlockType* stone = BlockRegistry::instance().get(BlockRegistry::instance().stone_id());
    ASSERT_NE(stone, nullptr);

    const MaterialProperties& mat = stone->get_material();
    EXPECT_GT(mat.weight, 0.0f);
    EXPECT_GT(mat.strength, 0.0f);
    EXPECT_GT(mat.hardness, 0.0f);
}

TEST_F(BlockRegistryTest, CustomBlockRegistration) {
    BlockTypeDesc desc;
    desc.name = "realcraft:test_block";
    desc.display_name = "Test Block";
    desc.flags = BlockFlags::Solid | BlockFlags::FullCube;
    desc.material = MaterialProperties::stone();
    desc.texture_index = 100;

    BlockId id = BlockRegistry::instance().register_block(desc);
    EXPECT_NE(id, BLOCK_INVALID);

    const BlockType* block = BlockRegistry::instance().get(id);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(block->get_name(), "realcraft:test_block");
    EXPECT_EQ(block->get_display_name(), "Test Block");
    EXPECT_TRUE(block->is_solid());
}

TEST_F(BlockRegistryTest, DuplicateRegistrationReturnsExisting) {
    BlockTypeDesc desc;
    desc.name = "realcraft:stone";         // Already registered
    desc.flags = BlockFlags::Transparent;  // Different flags

    BlockId id = BlockRegistry::instance().register_block(desc);
    EXPECT_EQ(id, BlockRegistry::instance().stone_id());

    // Original stone should still be solid
    const BlockType* stone = BlockRegistry::instance().get(id);
    EXPECT_TRUE(stone->is_solid());
}

}  // namespace
}  // namespace realcraft::world
