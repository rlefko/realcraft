// RealCraft Gameplay System
// inventory.hpp - Player inventory management

#pragma once

#include <optional>
#include <realcraft/gameplay/item.hpp>
#include <vector>

namespace realcraft::gameplay {

// ============================================================================
// Inventory Configuration
// ============================================================================

struct InventoryConfig {
    uint32_t hotbar_slots = 9;   // Hotbar slots (slots 0-8)
    uint32_t main_slots = 27;    // Main inventory (3 rows x 9 columns)
    bool creative_mode = false;  // Unlimited items, no consumption
    double pickup_radius = 2.0;  // Item pickup distance
    double pickup_delay = 0.5;   // Delay before items can be picked up
    bool auto_pickup = true;     // Automatically pick up nearby items
};

// ============================================================================
// Inventory Class
// ============================================================================

class Inventory {
public:
    static constexpr int HOTBAR_SIZE = 9;
    static constexpr int MAIN_SIZE = 27;
    static constexpr int TOTAL_SIZE = HOTBAR_SIZE + MAIN_SIZE;

    Inventory();
    ~Inventory();

    // Non-copyable, movable
    Inventory(const Inventory&) = delete;
    Inventory& operator=(const Inventory&) = delete;
    Inventory(Inventory&&) noexcept;
    Inventory& operator=(Inventory&&) noexcept;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    bool initialize(const InventoryConfig& config = {});
    void shutdown();
    [[nodiscard]] bool is_initialized() const { return initialized_; }

    // ========================================================================
    // Hotbar Selection (slots 0-8)
    // ========================================================================

    // Select a hotbar slot (0-8)
    void select_slot(int slot);

    // Get currently selected slot index (0-8)
    [[nodiscard]] int get_selected_slot() const { return selected_slot_; }

    // Scroll through hotbar slots (+1 = next, -1 = previous)
    void scroll_slot(int direction);

    // Get the item in the selected hotbar slot
    [[nodiscard]] const ItemStack& get_selected_item() const;
    [[nodiscard]] ItemStack& get_selected_item();

    // ========================================================================
    // Slot Access
    // ========================================================================

    // Get slot by index (0-8 = hotbar, 9-35 = main inventory)
    [[nodiscard]] const ItemStack& get_slot(size_t index) const;
    [[nodiscard]] ItemStack& get_slot(size_t index);

    // Get total slot count
    [[nodiscard]] size_t slot_count() const { return slots_.size(); }

    // Get hotbar slot (0-8)
    [[nodiscard]] const ItemStack& get_hotbar_slot(size_t index) const;
    [[nodiscard]] ItemStack& get_hotbar_slot(size_t index);

    // Get main inventory slot (0-26)
    [[nodiscard]] const ItemStack& get_main_slot(size_t index) const;
    [[nodiscard]] ItemStack& get_main_slot(size_t index);

    // ========================================================================
    // Item Operations
    // ========================================================================

    // Add an item to the inventory (auto-stacks, returns overflow)
    // Returns the items that couldn't fit
    [[nodiscard]] ItemStack add_item(const ItemStack& item);

    // Add item to a specific slot, returns overflow
    [[nodiscard]] ItemStack add_item_to_slot(size_t slot, const ItemStack& item);

    // Remove a specific amount of an item type
    // Returns true if the full amount was removed
    bool remove_item(ItemId id, uint16_t count = 1);

    // Check if inventory contains at least the specified amount of an item
    [[nodiscard]] bool has_item(ItemId id, uint16_t count = 1) const;

    // Count total items of a type across all slots
    [[nodiscard]] uint32_t count_item(ItemId id) const;

    // Clear entire inventory
    void clear();

    // Clear a specific slot
    void clear_slot(size_t index);

    // Swap contents of two slots
    void swap_slots(size_t slot_a, size_t slot_b);

    // ========================================================================
    // Block Interaction Integration
    // ========================================================================

    // Get the block ID of the held item (for placement)
    // Returns nullopt if held item is not a block
    [[nodiscard]] std::optional<world::BlockId> get_held_block() const;

    // Consume one of the held item (after placing)
    // Does nothing in creative mode
    void consume_held_item(uint16_t count = 1);

    // Check if player can place the held block
    [[nodiscard]] bool can_place_held_block() const;

    // ========================================================================
    // Configuration
    // ========================================================================

    [[nodiscard]] const InventoryConfig& get_config() const { return config_; }
    void set_creative_mode(bool enabled);
    [[nodiscard]] bool is_creative_mode() const { return config_.creative_mode; }

    // ========================================================================
    // Debug / Testing
    // ========================================================================

    // Give player items (for testing/creative mode)
    void give_item(ItemId item_id, uint16_t count = 1);

    // Give player a block item
    void give_block(world::BlockId block_id, uint16_t count = 1);

    // Fill hotbar with default blocks for testing
    void fill_hotbar_defaults();

private:
    std::vector<ItemStack> slots_;
    int selected_slot_ = 0;
    InventoryConfig config_;
    bool initialized_ = false;

    static const ItemStack EMPTY_STACK;

    // Find first slot that can accept the given item (for stacking)
    [[nodiscard]] std::optional<size_t> find_stackable_slot(ItemId item_id) const;

    // Find first empty slot
    [[nodiscard]] std::optional<size_t> find_empty_slot() const;
};

}  // namespace realcraft::gameplay
