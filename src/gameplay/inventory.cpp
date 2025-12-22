// RealCraft Gameplay System
// inventory.cpp - Player inventory management implementation

#include <algorithm>
#include <realcraft/core/logger.hpp>
#include <realcraft/gameplay/inventory.hpp>

namespace realcraft::gameplay {

// Static empty stack for returning references to empty slots
const ItemStack Inventory::EMPTY_STACK{};

Inventory::Inventory() = default;

Inventory::~Inventory() {
    if (initialized_) {
        shutdown();
    }
}

Inventory::Inventory(Inventory&&) noexcept = default;
Inventory& Inventory::operator=(Inventory&&) noexcept = default;

bool Inventory::initialize(const InventoryConfig& config) {
    if (initialized_) {
        REALCRAFT_LOG_WARN(core::log_category::GAME, "Inventory already initialized");
        return true;
    }

    config_ = config;

    // Initialize slots (hotbar + main inventory)
    size_t total_slots = config_.hotbar_slots + config_.main_slots;
    slots_.resize(total_slots);

    // Clear all slots
    for (auto& slot : slots_) {
        slot.clear();
    }

    selected_slot_ = 0;
    initialized_ = true;

    REALCRAFT_LOG_INFO(core::log_category::GAME, "Inventory initialized with {} slots ({} hotbar, {} main)",
                       total_slots, config_.hotbar_slots, config_.main_slots);

    return true;
}

void Inventory::shutdown() {
    if (!initialized_) {
        return;
    }

    slots_.clear();
    initialized_ = false;

    REALCRAFT_LOG_DEBUG(core::log_category::GAME, "Inventory shutdown");
}

// ============================================================================
// Hotbar Selection
// ============================================================================

void Inventory::select_slot(int slot) {
    if (slot < 0 || slot >= HOTBAR_SIZE) {
        REALCRAFT_LOG_WARN(core::log_category::GAME, "Invalid hotbar slot: {}", slot);
        return;
    }
    selected_slot_ = slot;
}

void Inventory::scroll_slot(int direction) {
    // Normalize direction to -1 or +1
    if (direction > 0) {
        direction = 1;
    } else if (direction < 0) {
        direction = -1;
    } else {
        return;
    }

    selected_slot_ += direction;

    // Wrap around
    if (selected_slot_ < 0) {
        selected_slot_ = HOTBAR_SIZE - 1;
    } else if (selected_slot_ >= HOTBAR_SIZE) {
        selected_slot_ = 0;
    }
}

const ItemStack& Inventory::get_selected_item() const {
    if (!initialized_ || selected_slot_ < 0 || static_cast<size_t>(selected_slot_) >= slots_.size()) {
        return EMPTY_STACK;
    }
    return slots_[static_cast<size_t>(selected_slot_)];
}

ItemStack& Inventory::get_selected_item() {
    if (!initialized_ || selected_slot_ < 0 || static_cast<size_t>(selected_slot_) >= slots_.size()) {
        // Return a reference to a static empty stack (not ideal but safe)
        static ItemStack empty_mutable;
        empty_mutable.clear();
        return empty_mutable;
    }
    return slots_[static_cast<size_t>(selected_slot_)];
}

// ============================================================================
// Slot Access
// ============================================================================

const ItemStack& Inventory::get_slot(size_t index) const {
    if (index >= slots_.size()) {
        return EMPTY_STACK;
    }
    return slots_[index];
}

ItemStack& Inventory::get_slot(size_t index) {
    if (index >= slots_.size()) {
        static ItemStack empty_mutable;
        empty_mutable.clear();
        return empty_mutable;
    }
    return slots_[index];
}

const ItemStack& Inventory::get_hotbar_slot(size_t index) const {
    if (index >= HOTBAR_SIZE) {
        return EMPTY_STACK;
    }
    return get_slot(index);
}

ItemStack& Inventory::get_hotbar_slot(size_t index) {
    if (index >= HOTBAR_SIZE) {
        static ItemStack empty_mutable;
        empty_mutable.clear();
        return empty_mutable;
    }
    return get_slot(index);
}

const ItemStack& Inventory::get_main_slot(size_t index) const {
    if (index >= MAIN_SIZE) {
        return EMPTY_STACK;
    }
    return get_slot(HOTBAR_SIZE + index);
}

ItemStack& Inventory::get_main_slot(size_t index) {
    if (index >= MAIN_SIZE) {
        static ItemStack empty_mutable;
        empty_mutable.clear();
        return empty_mutable;
    }
    return get_slot(HOTBAR_SIZE + index);
}

// ============================================================================
// Item Operations
// ============================================================================

ItemStack Inventory::add_item(const ItemStack& item) {
    if (item.is_empty()) {
        return ItemStack{};
    }

    ItemStack remaining = item;

    // First, try to stack with existing items
    while (!remaining.is_empty()) {
        auto stackable_slot = find_stackable_slot(remaining.item_id);
        if (!stackable_slot.has_value()) {
            break;
        }

        ItemStack& slot = slots_[stackable_slot.value()];
        remaining.count = slot.add(remaining.count);
        remaining.item_id = item.item_id;  // Keep item ID for remaining
    }

    // Then, fill empty slots
    while (!remaining.is_empty()) {
        auto empty_slot = find_empty_slot();
        if (!empty_slot.has_value()) {
            break;  // Inventory is full
        }

        ItemStack& slot = slots_[empty_slot.value()];
        slot.item_id = remaining.item_id;
        slot.durability = remaining.durability;
        remaining.count = slot.add(remaining.count);
    }

    if (remaining.count > 0) {
        REALCRAFT_LOG_DEBUG(core::log_category::GAME, "Inventory full, {} items could not be added", remaining.count);
    }

    return remaining;
}

ItemStack Inventory::add_item_to_slot(size_t slot, const ItemStack& item) {
    if (slot >= slots_.size() || item.is_empty()) {
        return item;
    }

    ItemStack& target = slots_[slot];

    // If slot is empty, place the item directly
    if (target.is_empty()) {
        target = item;
        uint16_t overflow = 0;
        if (target.count > target.get_max_stack_size()) {
            overflow = target.count - target.get_max_stack_size();
            target.count = target.get_max_stack_size();
        }
        return ItemStack{item.item_id, overflow, item.durability};
    }

    // If same item, try to stack
    if (target.item_id == item.item_id) {
        uint16_t overflow = target.add(item.count);
        return ItemStack{item.item_id, overflow, item.durability};
    }

    // Different item, can't add
    return item;
}

bool Inventory::remove_item(ItemId id, uint16_t count) {
    if (count == 0) {
        return true;
    }

    // First, check if we have enough
    if (!has_item(id, count)) {
        return false;
    }

    uint16_t remaining = count;

    // Remove from slots (prioritize main inventory, then hotbar)
    for (size_t i = HOTBAR_SIZE; i < slots_.size() && remaining > 0; ++i) {
        if (slots_[i].item_id == id) {
            remaining -= slots_[i].remove(remaining);
        }
    }
    for (size_t i = 0; i < HOTBAR_SIZE && remaining > 0; ++i) {
        if (slots_[i].item_id == id) {
            remaining -= slots_[i].remove(remaining);
        }
    }

    return remaining == 0;
}

bool Inventory::has_item(ItemId id, uint16_t count) const {
    return count_item(id) >= count;
}

uint32_t Inventory::count_item(ItemId id) const {
    uint32_t total = 0;
    for (const auto& slot : slots_) {
        if (slot.item_id == id) {
            total += slot.count;
        }
    }
    return total;
}

void Inventory::clear() {
    for (auto& slot : slots_) {
        slot.clear();
    }
}

void Inventory::clear_slot(size_t index) {
    if (index < slots_.size()) {
        slots_[index].clear();
    }
}

void Inventory::swap_slots(size_t slot_a, size_t slot_b) {
    if (slot_a >= slots_.size() || slot_b >= slots_.size()) {
        return;
    }
    std::swap(slots_[slot_a], slots_[slot_b]);
}

// ============================================================================
// Block Interaction Integration
// ============================================================================

std::optional<world::BlockId> Inventory::get_held_block() const {
    const ItemStack& held = get_selected_item();
    if (held.is_empty()) {
        return std::nullopt;
    }

    const ItemType* type = ItemRegistry::instance().get(held.item_id);
    if (type == nullptr || !type->is_block_item()) {
        return std::nullopt;
    }

    return type->get_block_id();
}

void Inventory::consume_held_item(uint16_t count) {
    // Creative mode doesn't consume items
    if (config_.creative_mode) {
        return;
    }

    ItemStack& held = get_selected_item();
    if (!held.is_empty()) {
        held.remove(count);
    }
}

bool Inventory::can_place_held_block() const {
    return get_held_block().has_value();
}

// ============================================================================
// Configuration
// ============================================================================

void Inventory::set_creative_mode(bool enabled) {
    config_.creative_mode = enabled;
    REALCRAFT_LOG_INFO(core::log_category::GAME, "Creative mode: {}", enabled ? "enabled" : "disabled");
}

// ============================================================================
// Debug / Testing
// ============================================================================

void Inventory::give_item(ItemId item_id, uint16_t count) {
    ItemStack item{item_id, count, 0};

    // Set durability for tools
    const ItemType* type = ItemRegistry::instance().get(item_id);
    if (type != nullptr && type->has_durability()) {
        item.durability = type->get_max_durability();
    }

    ItemStack overflow = add_item(item);
    if (!overflow.is_empty()) {
        REALCRAFT_LOG_DEBUG(core::log_category::GAME, "Gave {} items, {} overflow", count - overflow.count,
                            overflow.count);
    }
}

void Inventory::give_block(world::BlockId block_id, uint16_t count) {
    auto item_id = ItemRegistry::instance().get_item_for_block(block_id);
    if (!item_id.has_value()) {
        REALCRAFT_LOG_WARN(core::log_category::GAME, "No item for block ID {}", block_id);
        return;
    }
    give_item(item_id.value(), count);
}

void Inventory::fill_hotbar_defaults() {
    // Clear hotbar first
    for (size_t i = 0; i < HOTBAR_SIZE; ++i) {
        slots_[i].clear();
    }

    // Fill with common blocks for testing
    const std::array<const char*, HOTBAR_SIZE> default_blocks = {
        "realcraft:stone",        // 1
        "realcraft:dirt",         // 2
        "realcraft:grass",        // 3
        "realcraft:cobblestone",  // 4
        "realcraft:wood",         // 5
        "realcraft:glass",        // 6
        "realcraft:sand",         // 7
        "realcraft:torch",        // 8
        "realcraft:leaves"        // 9
    };

    for (size_t i = 0; i < default_blocks.size(); ++i) {
        auto item_id = ItemRegistry::instance().find_id(default_blocks[i]);
        if (item_id.has_value()) {
            slots_[i] = ItemStack{item_id.value(), 64, 0};
        }
    }

    REALCRAFT_LOG_INFO(core::log_category::GAME, "Filled hotbar with default blocks");
}

// ============================================================================
// Private Methods
// ============================================================================

std::optional<size_t> Inventory::find_stackable_slot(ItemId item_id) const {
    const ItemType* type = ItemRegistry::instance().get(item_id);
    if (type == nullptr || type->get_max_stack_size() <= 1) {
        return std::nullopt;  // Non-stackable item
    }

    // First check hotbar
    for (size_t i = 0; i < HOTBAR_SIZE; ++i) {
        const ItemStack& slot = slots_[i];
        if (slot.item_id == item_id && slot.count < type->get_max_stack_size()) {
            return i;
        }
    }

    // Then check main inventory
    for (size_t i = HOTBAR_SIZE; i < slots_.size(); ++i) {
        const ItemStack& slot = slots_[i];
        if (slot.item_id == item_id && slot.count < type->get_max_stack_size()) {
            return i;
        }
    }

    return std::nullopt;
}

std::optional<size_t> Inventory::find_empty_slot() const {
    // First check hotbar
    for (size_t i = 0; i < HOTBAR_SIZE; ++i) {
        if (slots_[i].is_empty()) {
            return i;
        }
    }

    // Then check main inventory
    for (size_t i = HOTBAR_SIZE; i < slots_.size(); ++i) {
        if (slots_[i].is_empty()) {
            return i;
        }
    }

    return std::nullopt;
}

}  // namespace realcraft::gameplay
