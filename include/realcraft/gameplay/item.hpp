// RealCraft Gameplay System
// item.hpp - Item type system and registry

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <realcraft/world/block.hpp>
#include <string>
#include <string_view>

namespace realcraft::gameplay {

// ============================================================================
// Item Identifier
// ============================================================================

using ItemId = uint16_t;

inline constexpr ItemId ITEM_NONE = 0;
inline constexpr ItemId ITEM_INVALID = 0xFFFF;

// ============================================================================
// Item Category
// ============================================================================

enum class ItemCategory : uint8_t {
    Block,       // Placeable blocks
    Tool,        // Mining/combat tools
    Consumable,  // Food, potions
    Material     // Crafting materials (not placeable)
};

// ============================================================================
// Item Type Descriptor (for registration)
// ============================================================================

struct ItemTypeDesc {
    std::string name;          // Unique identifier "realcraft:stone"
    std::string display_name;  // Human-readable name (empty = derive from name)
    ItemCategory category = ItemCategory::Block;
    uint16_t max_stack_size = 64;  // 1 for tools, 64 for blocks/materials
    uint16_t texture_index = 0;    // Index into item texture atlas (or block texture)

    // Block items - set for placeable items
    std::optional<world::BlockId> block_id;

    // Tool items
    float tool_speed = 1.0f;      // Mining speed multiplier
    uint16_t max_durability = 0;  // 0 = infinite/not applicable
};

// ============================================================================
// Item Type (immutable, registered)
// ============================================================================

class ItemType {
public:
    [[nodiscard]] ItemId get_id() const { return id_; }
    [[nodiscard]] const std::string& get_name() const { return name_; }
    [[nodiscard]] const std::string& get_display_name() const { return display_name_; }
    [[nodiscard]] ItemCategory get_category() const { return category_; }
    [[nodiscard]] uint16_t get_max_stack_size() const { return max_stack_size_; }
    [[nodiscard]] uint16_t get_texture_index() const { return texture_index_; }

    // Block items
    [[nodiscard]] bool is_block_item() const { return block_id_.has_value(); }
    [[nodiscard]] std::optional<world::BlockId> get_block_id() const { return block_id_; }

    // Tool items
    [[nodiscard]] bool is_tool() const { return category_ == ItemCategory::Tool; }
    [[nodiscard]] float get_tool_speed() const { return tool_speed_; }
    [[nodiscard]] uint16_t get_max_durability() const { return max_durability_; }
    [[nodiscard]] bool has_durability() const { return max_durability_ > 0; }

    // Category checks
    [[nodiscard]] bool is_consumable() const { return category_ == ItemCategory::Consumable; }
    [[nodiscard]] bool is_material() const { return category_ == ItemCategory::Material; }
    [[nodiscard]] bool is_stackable() const { return max_stack_size_ > 1; }

private:
    friend class ItemRegistry;
    ItemType(ItemId id, const ItemTypeDesc& desc);

    ItemId id_;
    std::string name_;
    std::string display_name_;
    ItemCategory category_;
    uint16_t max_stack_size_;
    uint16_t texture_index_;
    std::optional<world::BlockId> block_id_;
    float tool_speed_;
    uint16_t max_durability_;
};

// ============================================================================
// Item Stack
// ============================================================================

struct ItemStack {
    ItemId item_id = ITEM_NONE;
    uint16_t count = 0;
    uint16_t durability = 0;  // Current durability for tools

    // Constructors
    ItemStack() = default;
    explicit ItemStack(ItemId id, uint16_t amount = 1, uint16_t dur = 0)
        : item_id(id), count(amount), durability(dur) {}

    // State checks
    [[nodiscard]] bool is_empty() const { return count == 0 || item_id == ITEM_NONE; }

    // Check if two stacks can be merged (same item, not full)
    [[nodiscard]] bool can_merge(const ItemStack& other) const;

    // Get max stack size for this item
    [[nodiscard]] uint16_t get_max_stack_size() const;

    // Get remaining space in this stack
    [[nodiscard]] uint16_t get_space_remaining() const;

    // Add items to this stack, returns overflow count
    uint16_t add(uint16_t amount);

    // Remove items from this stack, returns actual amount removed
    uint16_t remove(uint16_t amount);

    // Clear the stack
    void clear() {
        item_id = ITEM_NONE;
        count = 0;
        durability = 0;
    }

    // Comparison
    bool operator==(const ItemStack& other) const {
        return item_id == other.item_id && count == other.count && durability == other.durability;
    }
    bool operator!=(const ItemStack& other) const { return !(*this == other); }
};

// ============================================================================
// Item Registry (singleton)
// ============================================================================

class ItemRegistry {
public:
    // Singleton access
    [[nodiscard]] static ItemRegistry& instance();

    // Non-copyable, non-movable
    ItemRegistry(const ItemRegistry&) = delete;
    ItemRegistry& operator=(const ItemRegistry&) = delete;
    ItemRegistry(ItemRegistry&&) = delete;
    ItemRegistry& operator=(ItemRegistry&&) = delete;

    // Registration
    ItemId register_item(const ItemTypeDesc& desc);
    void register_defaults();  // Registers items for all registered blocks

    // Lookup
    [[nodiscard]] const ItemType* get(ItemId id) const;
    [[nodiscard]] const ItemType* get(std::string_view name) const;
    [[nodiscard]] std::optional<ItemId> find_id(std::string_view name) const;

    // Block-to-item mapping
    [[nodiscard]] std::optional<ItemId> get_item_for_block(world::BlockId block_id) const;
    [[nodiscard]] ItemStack create_block_item(world::BlockId block_id, uint16_t count = 1) const;

    // Iteration
    [[nodiscard]] size_t count() const;
    void for_each(const std::function<void(const ItemType&)>& callback) const;

private:
    ItemRegistry();
    ~ItemRegistry();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::gameplay
