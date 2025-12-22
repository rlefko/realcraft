// RealCraft Unit Tests
// inventory_test.cpp - Tests for inventory system

#include <gtest/gtest.h>

#include <realcraft/gameplay/inventory.hpp>
#include <realcraft/world/block.hpp>

namespace realcraft::gameplay {
namespace {

class InventoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure blocks and items are registered
        world::BlockRegistry::instance().register_defaults();
        ItemRegistry::instance().register_defaults();

        // Initialize inventory for tests
        InventoryConfig config;
        config.creative_mode = false;
        inventory_.initialize(config);
    }

    void TearDown() override { inventory_.shutdown(); }

    Inventory inventory_;
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(InventoryTest, InitializesCorrectly) {
    EXPECT_TRUE(inventory_.is_initialized());
    EXPECT_EQ(inventory_.slot_count(), Inventory::TOTAL_SIZE);
}

TEST_F(InventoryTest, StartsEmpty) {
    for (size_t i = 0; i < inventory_.slot_count(); ++i) {
        EXPECT_TRUE(inventory_.get_slot(i).is_empty());
    }
}

// ============================================================================
// Hotbar Selection Tests
// ============================================================================

TEST_F(InventoryTest, DefaultSelectedSlot) {
    EXPECT_EQ(inventory_.get_selected_slot(), 0);
}

TEST_F(InventoryTest, SelectSlot) {
    inventory_.select_slot(5);
    EXPECT_EQ(inventory_.get_selected_slot(), 5);
}

TEST_F(InventoryTest, SelectSlotWrapsNegative) {
    inventory_.select_slot(-1);
    EXPECT_EQ(inventory_.get_selected_slot(), 0);  // Invalid, stays at 0
}

TEST_F(InventoryTest, SelectSlotWrapsTooHigh) {
    inventory_.select_slot(9);
    EXPECT_EQ(inventory_.get_selected_slot(), 0);  // Invalid, stays at 0
}

TEST_F(InventoryTest, ScrollSlotForward) {
    inventory_.select_slot(0);
    inventory_.scroll_slot(1);
    EXPECT_EQ(inventory_.get_selected_slot(), 1);
}

TEST_F(InventoryTest, ScrollSlotBackward) {
    inventory_.select_slot(5);
    inventory_.scroll_slot(-1);
    EXPECT_EQ(inventory_.get_selected_slot(), 4);
}

TEST_F(InventoryTest, ScrollSlotWrapsForward) {
    inventory_.select_slot(8);
    inventory_.scroll_slot(1);
    EXPECT_EQ(inventory_.get_selected_slot(), 0);
}

TEST_F(InventoryTest, ScrollSlotWrapsBackward) {
    inventory_.select_slot(0);
    inventory_.scroll_slot(-1);
    EXPECT_EQ(inventory_.get_selected_slot(), 8);
}

// ============================================================================
// Add Item Tests
// ============================================================================

TEST_F(InventoryTest, AddSingleItem) {
    auto stone_id = ItemRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_id.has_value());

    ItemStack item{stone_id.value(), 10, 0};
    ItemStack overflow = inventory_.add_item(item);

    EXPECT_TRUE(overflow.is_empty());
    EXPECT_TRUE(inventory_.has_item(stone_id.value(), 10));
}

TEST_F(InventoryTest, AddItemsStack) {
    auto stone_id = ItemRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_id.has_value());

    // Add first batch
    inventory_.add_item(ItemStack{stone_id.value(), 40, 0});
    // Add second batch - should stack
    inventory_.add_item(ItemStack{stone_id.value(), 20, 0});

    // Should be in one slot stacked to 60
    EXPECT_EQ(inventory_.count_item(stone_id.value()), 60);

    // Check it's actually in one slot (first available hotbar slot)
    EXPECT_EQ(inventory_.get_slot(0).count, 60);
}

TEST_F(InventoryTest, AddItemsOverflow) {
    auto stone_id = ItemRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_id.has_value());

    // Add 50 items
    inventory_.add_item(ItemStack{stone_id.value(), 50, 0});
    // Add 30 more - should overflow into second slot
    inventory_.add_item(ItemStack{stone_id.value(), 30, 0});

    // First slot should be full (64), second should have remainder (16)
    EXPECT_EQ(inventory_.get_slot(0).count, 64);
    EXPECT_EQ(inventory_.get_slot(1).count, 16);
    EXPECT_EQ(inventory_.count_item(stone_id.value()), 80);
}

TEST_F(InventoryTest, AddDifferentItems) {
    auto stone_id = ItemRegistry::instance().find_id("realcraft:stone");
    auto dirt_id = ItemRegistry::instance().find_id("realcraft:dirt");
    ASSERT_TRUE(stone_id.has_value());
    ASSERT_TRUE(dirt_id.has_value());

    inventory_.add_item(ItemStack{stone_id.value(), 10, 0});
    inventory_.add_item(ItemStack{dirt_id.value(), 20, 0});

    EXPECT_EQ(inventory_.count_item(stone_id.value()), 10);
    EXPECT_EQ(inventory_.count_item(dirt_id.value()), 20);
}

// ============================================================================
// Remove Item Tests
// ============================================================================

TEST_F(InventoryTest, RemoveItem) {
    auto stone_id = ItemRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_id.has_value());

    inventory_.add_item(ItemStack{stone_id.value(), 50, 0});
    EXPECT_TRUE(inventory_.remove_item(stone_id.value(), 20));
    EXPECT_EQ(inventory_.count_item(stone_id.value()), 30);
}

TEST_F(InventoryTest, RemoveAllItems) {
    auto stone_id = ItemRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_id.has_value());

    inventory_.add_item(ItemStack{stone_id.value(), 50, 0});
    EXPECT_TRUE(inventory_.remove_item(stone_id.value(), 50));
    EXPECT_EQ(inventory_.count_item(stone_id.value()), 0);
}

TEST_F(InventoryTest, RemoveTooManyItems) {
    auto stone_id = ItemRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_id.has_value());

    inventory_.add_item(ItemStack{stone_id.value(), 50, 0});
    EXPECT_FALSE(inventory_.remove_item(stone_id.value(), 100));
    EXPECT_EQ(inventory_.count_item(stone_id.value()), 50);  // No change
}

// ============================================================================
// Has Item Tests
// ============================================================================

TEST_F(InventoryTest, HasItemTrue) {
    auto stone_id = ItemRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_id.has_value());

    inventory_.add_item(ItemStack{stone_id.value(), 50, 0});
    EXPECT_TRUE(inventory_.has_item(stone_id.value(), 1));
    EXPECT_TRUE(inventory_.has_item(stone_id.value(), 50));
}

TEST_F(InventoryTest, HasItemFalse) {
    auto stone_id = ItemRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_id.has_value());

    inventory_.add_item(ItemStack{stone_id.value(), 50, 0});
    EXPECT_FALSE(inventory_.has_item(stone_id.value(), 100));
}

TEST_F(InventoryTest, HasItemEmpty) {
    auto stone_id = ItemRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_id.has_value());

    EXPECT_FALSE(inventory_.has_item(stone_id.value(), 1));
}

// ============================================================================
// Block Interaction Integration Tests
// ============================================================================

TEST_F(InventoryTest, GetHeldBlockEmpty) {
    // Empty inventory should return no block
    EXPECT_FALSE(inventory_.get_held_block().has_value());
}

TEST_F(InventoryTest, GetHeldBlockWithItem) {
    auto stone_id = ItemRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_id.has_value());

    inventory_.get_slot(0) = ItemStack{stone_id.value(), 10, 0};
    inventory_.select_slot(0);

    auto held_block = inventory_.get_held_block();
    ASSERT_TRUE(held_block.has_value());

    // Verify it's the stone block
    auto stone_block_id = world::BlockRegistry::instance().find_id("realcraft:stone");
    EXPECT_EQ(held_block.value(), stone_block_id.value());
}

TEST_F(InventoryTest, ConsumeHeldItem) {
    auto stone_id = ItemRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_id.has_value());

    inventory_.get_slot(0) = ItemStack{stone_id.value(), 10, 0};
    inventory_.select_slot(0);

    inventory_.consume_held_item(1);
    EXPECT_EQ(inventory_.get_slot(0).count, 9);
}

TEST_F(InventoryTest, ConsumeHeldItemCreativeMode) {
    auto stone_id = ItemRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_id.has_value());

    inventory_.set_creative_mode(true);
    inventory_.get_slot(0) = ItemStack{stone_id.value(), 10, 0};
    inventory_.select_slot(0);

    inventory_.consume_held_item(1);
    EXPECT_EQ(inventory_.get_slot(0).count, 10);  // No consumption in creative
}

// ============================================================================
// Hotbar Defaults Tests
// ============================================================================

TEST_F(InventoryTest, FillHotbarDefaults) {
    inventory_.fill_hotbar_defaults();

    // Check that hotbar has items
    bool has_items = false;
    for (size_t i = 0; i < Inventory::HOTBAR_SIZE; ++i) {
        if (!inventory_.get_hotbar_slot(i).is_empty()) {
            has_items = true;
            break;
        }
    }
    EXPECT_TRUE(has_items);
}

// ============================================================================
// Swap Slots Tests
// ============================================================================

TEST_F(InventoryTest, SwapSlots) {
    auto stone_id = ItemRegistry::instance().find_id("realcraft:stone");
    auto dirt_id = ItemRegistry::instance().find_id("realcraft:dirt");
    ASSERT_TRUE(stone_id.has_value());
    ASSERT_TRUE(dirt_id.has_value());

    inventory_.get_slot(0) = ItemStack{stone_id.value(), 10, 0};
    inventory_.get_slot(1) = ItemStack{dirt_id.value(), 20, 0};

    inventory_.swap_slots(0, 1);

    EXPECT_EQ(inventory_.get_slot(0).item_id, dirt_id.value());
    EXPECT_EQ(inventory_.get_slot(0).count, 20);
    EXPECT_EQ(inventory_.get_slot(1).item_id, stone_id.value());
    EXPECT_EQ(inventory_.get_slot(1).count, 10);
}

// ============================================================================
// Clear Tests
// ============================================================================

TEST_F(InventoryTest, ClearSlot) {
    auto stone_id = ItemRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_id.has_value());

    inventory_.get_slot(0) = ItemStack{stone_id.value(), 10, 0};
    inventory_.clear_slot(0);

    EXPECT_TRUE(inventory_.get_slot(0).is_empty());
}

TEST_F(InventoryTest, ClearAll) {
    auto stone_id = ItemRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_id.has_value());

    inventory_.add_item(ItemStack{stone_id.value(), 100, 0});
    inventory_.clear();

    for (size_t i = 0; i < inventory_.slot_count(); ++i) {
        EXPECT_TRUE(inventory_.get_slot(i).is_empty());
    }
}

// ============================================================================
// Give Item Tests
// ============================================================================

TEST_F(InventoryTest, GiveItem) {
    auto stone_id = ItemRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_id.has_value());

    inventory_.give_item(stone_id.value(), 50);
    EXPECT_EQ(inventory_.count_item(stone_id.value()), 50);
}

TEST_F(InventoryTest, GiveBlock) {
    auto stone_block_id = world::BlockRegistry::instance().find_id("realcraft:stone");
    ASSERT_TRUE(stone_block_id.has_value());

    inventory_.give_block(stone_block_id.value(), 50);

    auto stone_item_id = ItemRegistry::instance().get_item_for_block(stone_block_id.value());
    ASSERT_TRUE(stone_item_id.has_value());
    EXPECT_EQ(inventory_.count_item(stone_item_id.value()), 50);
}

}  // namespace
}  // namespace realcraft::gameplay
