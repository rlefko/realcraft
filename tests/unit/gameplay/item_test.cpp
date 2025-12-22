// RealCraft Unit Tests
// item_test.cpp - Tests for item system

#include <gtest/gtest.h>

#include <realcraft/gameplay/item.hpp>
#include <realcraft/world/block.hpp>

namespace realcraft::gameplay {
namespace {

class ItemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure blocks and items are registered
        world::BlockRegistry::instance().register_defaults();
        ItemRegistry::instance().register_defaults();
    }
};

// ============================================================================
// ItemStack Tests
// ============================================================================

TEST_F(ItemTest, EmptyStack) {
    ItemStack stack;
    EXPECT_TRUE(stack.is_empty());
    EXPECT_EQ(stack.count, 0);
    EXPECT_EQ(stack.item_id, ITEM_NONE);
}

TEST_F(ItemTest, NonEmptyStack) {
    auto stone_item = ItemRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_item.has_value());

    ItemStack stack{stone_item.value(), 10, 0};
    EXPECT_FALSE(stack.is_empty());
    EXPECT_EQ(stack.count, 10);
    EXPECT_EQ(stack.item_id, stone_item.value());
}

TEST_F(ItemTest, StackAdd) {
    auto stone_item = ItemRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_item.has_value());

    ItemStack stack{stone_item.value(), 10, 0};

    // Add some items
    uint16_t overflow = stack.add(20);
    EXPECT_EQ(overflow, 0);
    EXPECT_EQ(stack.count, 30);

    // Add items beyond max stack size (64)
    overflow = stack.add(50);
    EXPECT_EQ(overflow, 16);  // 30 + 50 = 80, max 64, overflow 16
    EXPECT_EQ(stack.count, 64);
}

TEST_F(ItemTest, StackRemove) {
    auto stone_item = ItemRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_item.has_value());

    ItemStack stack{stone_item.value(), 50, 0};

    // Remove some items
    uint16_t removed = stack.remove(20);
    EXPECT_EQ(removed, 20);
    EXPECT_EQ(stack.count, 30);

    // Remove more items than available
    removed = stack.remove(50);
    EXPECT_EQ(removed, 30);
    EXPECT_TRUE(stack.is_empty());
}

TEST_F(ItemTest, StackClear) {
    auto stone_item = ItemRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_item.has_value());

    ItemStack stack{stone_item.value(), 50, 0};
    EXPECT_FALSE(stack.is_empty());

    stack.clear();
    EXPECT_TRUE(stack.is_empty());
    EXPECT_EQ(stack.item_id, ITEM_NONE);
}

TEST_F(ItemTest, CanMergeSameItem) {
    auto stone_item = ItemRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_item.has_value());

    ItemStack stack1{stone_item.value(), 30, 0};
    ItemStack stack2{stone_item.value(), 20, 0};

    EXPECT_TRUE(stack1.can_merge(stack2));
}

TEST_F(ItemTest, CanMergeDifferentItems) {
    auto stone_item = ItemRegistry::instance().find_id("realcraft:stone");
    auto dirt_item = ItemRegistry::instance().find_id("realcraft:dirt");
    ASSERT_TRUE(stone_item.has_value());
    ASSERT_TRUE(dirt_item.has_value());

    ItemStack stack1{stone_item.value(), 30, 0};
    ItemStack stack2{dirt_item.value(), 20, 0};

    EXPECT_FALSE(stack1.can_merge(stack2));
}

TEST_F(ItemTest, CanMergeFullStack) {
    auto stone_item = ItemRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_item.has_value());

    ItemStack stack1{stone_item.value(), 64, 0};  // Full stack
    ItemStack stack2{stone_item.value(), 20, 0};

    EXPECT_FALSE(stack1.can_merge(stack2));  // Full stack can't accept more
}

// ============================================================================
// ItemRegistry Tests
// ============================================================================

TEST_F(ItemTest, RegistryHasItems) {
    EXPECT_GT(ItemRegistry::instance().count(), 1);  // At least "none" + blocks
}

TEST_F(ItemTest, BlockItemsRegistered) {
    // Check that common block items exist
    auto stone = ItemRegistry::instance().find_id("realcraft:stone");
    auto dirt = ItemRegistry::instance().find_id("realcraft:dirt");
    auto grass = ItemRegistry::instance().find_id("realcraft:grass");

    EXPECT_TRUE(stone.has_value());
    EXPECT_TRUE(dirt.has_value());
    EXPECT_TRUE(grass.has_value());
}

TEST_F(ItemTest, ItemTypeProperties) {
    auto stone_id = ItemRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_id.has_value());

    const ItemType* stone_type = ItemRegistry::instance().get(stone_id.value());
    ASSERT_NE(stone_type, nullptr);

    EXPECT_EQ(stone_type->get_category(), ItemCategory::Block);
    EXPECT_EQ(stone_type->get_max_stack_size(), 64);
    EXPECT_TRUE(stone_type->is_block_item());
    EXPECT_FALSE(stone_type->is_tool());
}

TEST_F(ItemTest, BlockToItemMapping) {
    // Get stone block ID
    auto stone_block_id = world::BlockRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_block_id.has_value());

    // Get item for block
    auto stone_item_id = ItemRegistry::instance().get_item_for_block(stone_block_id.value());
    EXPECT_TRUE(stone_item_id.has_value());

    // Check the item is the correct block item
    const ItemType* item_type = ItemRegistry::instance().get(stone_item_id.value());
    ASSERT_NE(item_type, nullptr);
    EXPECT_TRUE(item_type->is_block_item());
    EXPECT_EQ(item_type->get_block_id(), stone_block_id.value());
}

TEST_F(ItemTest, CreateBlockItem) {
    auto stone_block_id = world::BlockRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_block_id.has_value());

    ItemStack stack = ItemRegistry::instance().create_block_item(stone_block_id.value(), 32);

    EXPECT_FALSE(stack.is_empty());
    EXPECT_EQ(stack.count, 32);

    // Verify the item corresponds to the block
    const ItemType* type = ItemRegistry::instance().get(stack.item_id);
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(type->get_block_id(), stone_block_id.value());
}

TEST_F(ItemTest, NonBlockItemForWater) {
    // Water should not have a corresponding item (liquids are special)
    auto water_block_id = world::BlockRegistry::instance().find_id("realcraft:water");
    ASSERT_TRUE(water_block_id.has_value());

    auto water_item_id = ItemRegistry::instance().get_item_for_block(water_block_id.value());
    EXPECT_FALSE(water_item_id.has_value());  // No item for water
}

TEST_F(ItemTest, LookupByName) {
    const ItemType* type = ItemRegistry::instance().get("realcraft:stone");
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(type->get_name(), "realcraft:stone");
}

TEST_F(ItemTest, ForEach) {
    size_t count = 0;
    ItemRegistry::instance().for_each([&count](const ItemType& type) { count++; });
    EXPECT_EQ(count, ItemRegistry::instance().count());
}

}  // namespace
}  // namespace realcraft::gameplay
