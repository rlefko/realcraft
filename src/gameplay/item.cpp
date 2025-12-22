// RealCraft Gameplay System
// item.cpp - Item type system and registry implementation

#include <mutex>
#include <realcraft/core/logger.hpp>
#include <realcraft/gameplay/item.hpp>
#include <unordered_map>
#include <vector>

namespace realcraft::gameplay {

// ============================================================================
// ItemType Implementation
// ============================================================================

ItemType::ItemType(ItemId id, const ItemTypeDesc& desc)
    : id_(id), name_(desc.name), display_name_(desc.display_name.empty() ? desc.name : desc.display_name),
      category_(desc.category), max_stack_size_(desc.max_stack_size), texture_index_(desc.texture_index),
      block_id_(desc.block_id), tool_speed_(desc.tool_speed), max_durability_(desc.max_durability) {
    // Strip namespace prefix for display name if not provided
    if (desc.display_name.empty()) {
        size_t colon_pos = display_name_.find(':');
        if (colon_pos != std::string::npos) {
            display_name_ = display_name_.substr(colon_pos + 1);
            // Capitalize first letter
            if (!display_name_.empty()) {
                display_name_[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(display_name_[0])));
            }
        }
    }
}

// ============================================================================
// ItemStack Implementation
// ============================================================================

bool ItemStack::can_merge(const ItemStack& other) const {
    if (is_empty() || other.is_empty()) {
        return true;  // Empty stacks can always "merge" (accept items)
    }
    if (item_id != other.item_id) {
        return false;
    }

    // Check if item is stackable
    const ItemType* type = ItemRegistry::instance().get(item_id);
    if (type == nullptr || type->get_max_stack_size() <= 1) {
        return false;  // Non-stackable items (tools)
    }

    // Can merge if not at max
    return count < type->get_max_stack_size();
}

uint16_t ItemStack::get_max_stack_size() const {
    if (item_id == ITEM_NONE) {
        return 0;
    }
    const ItemType* type = ItemRegistry::instance().get(item_id);
    return type ? type->get_max_stack_size() : 64;
}

uint16_t ItemStack::get_space_remaining() const {
    uint16_t max_size = get_max_stack_size();
    return (count < max_size) ? (max_size - count) : 0;
}

uint16_t ItemStack::add(uint16_t amount) {
    if (amount == 0) {
        return 0;
    }

    uint16_t max_size = get_max_stack_size();
    uint16_t space = (count < max_size) ? (max_size - count) : 0;
    uint16_t to_add = (amount < space) ? amount : space;

    count += to_add;
    return amount - to_add;  // Return overflow
}

uint16_t ItemStack::remove(uint16_t amount) {
    if (amount == 0 || count == 0) {
        return 0;
    }

    uint16_t to_remove = (amount < count) ? amount : count;
    count -= to_remove;

    if (count == 0) {
        item_id = ITEM_NONE;
        durability = 0;
    }

    return to_remove;
}

// ============================================================================
// ItemRegistry Implementation
// ============================================================================

struct ItemRegistry::Impl {
    std::vector<std::unique_ptr<ItemType>> items;
    std::unordered_map<std::string, ItemId> name_to_id;
    std::unordered_map<world::BlockId, ItemId> block_to_item;  // BlockId -> ItemId mapping

    mutable std::mutex mutex;
    bool defaults_registered = false;
};

ItemRegistry::ItemRegistry() : impl_(std::make_unique<Impl>()) {
    // Pre-allocate for common items
    impl_->items.reserve(256);

    // Register "none" item (ID 0)
    ItemTypeDesc none_desc;
    none_desc.name = "realcraft:none";
    none_desc.display_name = "None";
    none_desc.category = ItemCategory::Material;
    none_desc.max_stack_size = 0;

    std::unique_ptr<ItemType> none_item(new ItemType(ITEM_NONE, none_desc));
    impl_->items.push_back(std::move(none_item));
    impl_->name_to_id["realcraft:none"] = ITEM_NONE;
}

ItemRegistry::~ItemRegistry() = default;

ItemRegistry& ItemRegistry::instance() {
    static ItemRegistry instance;
    return instance;
}

ItemId ItemRegistry::register_item(const ItemTypeDesc& desc) {
    std::lock_guard<std::mutex> lock(impl_->mutex);

    // Check for duplicate name
    if (impl_->name_to_id.count(desc.name) > 0) {
        REALCRAFT_LOG_WARN(core::log_category::GAME, "Item '{}' already registered", desc.name);
        return impl_->name_to_id[desc.name];
    }

    // Generate new ID
    ItemId id = static_cast<ItemId>(impl_->items.size());
    if (id >= ITEM_INVALID) {
        REALCRAFT_LOG_ERROR(core::log_category::GAME, "Item registry full, cannot register '{}'", desc.name);
        return ITEM_INVALID;
    }

    // Create and store item type
    std::unique_ptr<ItemType> item(new ItemType(id, desc));
    impl_->items.push_back(std::move(item));
    impl_->name_to_id[desc.name] = id;

    // If this is a block item, map block ID to item ID
    if (desc.block_id.has_value()) {
        impl_->block_to_item[desc.block_id.value()] = id;
    }

    REALCRAFT_LOG_DEBUG(core::log_category::GAME, "Registered item '{}' with ID {}", desc.name, id);
    return id;
}

void ItemRegistry::register_defaults() {
    std::lock_guard<std::mutex> lock(impl_->mutex);

    if (impl_->defaults_registered) {
        return;
    }
    impl_->defaults_registered = true;

    REALCRAFT_LOG_INFO(core::log_category::GAME, "Registering default items from block registry");

    // Register an item for each registered block (except air and bedrock)
    world::BlockRegistry::instance().for_each([this](const world::BlockType& block) {
        // Skip air
        if (block.is_air()) {
            return;
        }

        // Skip bedrock (not obtainable)
        if (block.get_name() == "realcraft:bedrock") {
            return;
        }

        // Skip water (needs special bucket handling in the future)
        if (block.is_liquid()) {
            return;
        }

        // Create item descriptor for this block
        ItemTypeDesc desc;
        desc.name = block.get_name();
        desc.display_name = block.get_display_name();
        desc.category = ItemCategory::Block;
        desc.max_stack_size = 64;
        desc.texture_index = block.get_texture_index(world::Direction::PosZ);  // Use front face texture
        desc.block_id = block.get_id();

        // Register the item (without lock since we already hold it)
        ItemId id = static_cast<ItemId>(impl_->items.size());
        if (id >= ITEM_INVALID) {
            REALCRAFT_LOG_ERROR(core::log_category::GAME, "Item registry full, cannot register '{}'", desc.name);
            return;
        }

        std::unique_ptr<ItemType> item(new ItemType(id, desc));
        impl_->items.push_back(std::move(item));
        impl_->name_to_id[desc.name] = id;
        impl_->block_to_item[block.get_id()] = id;

        REALCRAFT_LOG_DEBUG(core::log_category::GAME, "Registered block item '{}' (ID {}) for block ID {}", desc.name,
                            id, block.get_id());
    });

    REALCRAFT_LOG_INFO(core::log_category::GAME, "Registered {} default items", impl_->items.size() - 1);
}

const ItemType* ItemRegistry::get(ItemId id) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (id >= impl_->items.size()) {
        return nullptr;
    }
    return impl_->items[id].get();
}

const ItemType* ItemRegistry::get(std::string_view name) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto it = impl_->name_to_id.find(std::string(name));
    if (it == impl_->name_to_id.end()) {
        return nullptr;
    }
    return impl_->items[it->second].get();
}

std::optional<ItemId> ItemRegistry::find_id(std::string_view name) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto it = impl_->name_to_id.find(std::string(name));
    if (it == impl_->name_to_id.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<ItemId> ItemRegistry::get_item_for_block(world::BlockId block_id) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto it = impl_->block_to_item.find(block_id);
    if (it == impl_->block_to_item.end()) {
        return std::nullopt;
    }
    return it->second;
}

ItemStack ItemRegistry::create_block_item(world::BlockId block_id, uint16_t count) const {
    auto item_id = get_item_for_block(block_id);
    if (!item_id.has_value()) {
        return ItemStack{};  // Empty stack
    }
    return ItemStack{item_id.value(), count, 0};
}

size_t ItemRegistry::count() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->items.size();
}

void ItemRegistry::for_each(const std::function<void(const ItemType&)>& callback) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (const auto& item : impl_->items) {
        callback(*item);
    }
}

}  // namespace realcraft::gameplay
