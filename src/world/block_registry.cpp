// RealCraft World System
// block_registry.cpp - Block registry singleton implementation

#include <mutex>
#include <realcraft/core/logger.hpp>
#include <realcraft/world/block.hpp>
#include <unordered_map>
#include <vector>

namespace realcraft::world {

// ============================================================================
// BlockRegistry Implementation
// ============================================================================

struct BlockRegistry::Impl {
    std::vector<std::unique_ptr<BlockType>> blocks;
    std::unordered_map<std::string, BlockId> name_to_id;
    std::unordered_map<BlockId, BlockTickCallback> tick_callbacks;

    BlockPlacedCallback placed_callback;
    BlockBrokenCallback broken_callback;

    mutable std::mutex mutex;
    bool defaults_registered = false;
};

BlockRegistry::BlockRegistry() : impl_(std::make_unique<Impl>()) {
    // Pre-allocate for common blocks
    impl_->blocks.reserve(256);

    // Register air block (ID 0)
    BlockTypeDesc air_desc;
    air_desc.name = "realcraft:air";
    air_desc.display_name = "Air";
    air_desc.flags = BlockFlags::Transparent | BlockFlags::Replaceable;
    air_desc.material = MaterialProperties{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, "none"};
    air_desc.light_absorption = 0;

    std::unique_ptr<BlockType> air_block(new BlockType(BLOCK_AIR, air_desc));
    impl_->blocks.push_back(std::move(air_block));
    impl_->name_to_id["realcraft:air"] = BLOCK_AIR;
}

BlockRegistry::~BlockRegistry() = default;

BlockRegistry& BlockRegistry::instance() {
    static BlockRegistry instance;
    return instance;
}

BlockId BlockRegistry::register_block(const BlockTypeDesc& desc) {
    std::lock_guard<std::mutex> lock(impl_->mutex);

    // Check for duplicate name
    if (impl_->name_to_id.count(desc.name) > 0) {
        REALCRAFT_LOG_WARN(core::log_category::WORLD, "Block '{}' already registered", desc.name);
        return impl_->name_to_id[desc.name];
    }

    // Generate new ID
    BlockId id = static_cast<BlockId>(impl_->blocks.size());
    if (id >= BLOCK_INVALID) {
        REALCRAFT_LOG_ERROR(core::log_category::WORLD, "Block registry full, cannot register '{}'", desc.name);
        return BLOCK_INVALID;
    }

    // Create and store block type
    std::unique_ptr<BlockType> block(new BlockType(id, desc));
    impl_->blocks.push_back(std::move(block));
    impl_->name_to_id[desc.name] = id;

    REALCRAFT_LOG_DEBUG(core::log_category::WORLD, "Registered block '{}' with ID {}", desc.name, id);
    return id;
}

void BlockRegistry::register_defaults() {
    std::lock_guard<std::mutex> lock(impl_->mutex);

    if (impl_->defaults_registered) {
        return;
    }
    impl_->defaults_registered = true;

    REALCRAFT_LOG_INFO(core::log_category::WORLD, "Registering default block types");

    // Note: Air (ID 0) is already registered in constructor

    // Stone (ID 1)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:stone";
        desc.display_name = "Stone";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable;
        desc.material = MaterialProperties::stone();
        desc.texture_index = 1;
        stone_id_ = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(stone_id_, desc)));
        impl_->name_to_id[desc.name] = stone_id_;
    }

    // Dirt (ID 2)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:dirt";
        desc.display_name = "Dirt";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable;
        desc.material = MaterialProperties::dirt();
        desc.texture_index = 2;
        dirt_id_ = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(dirt_id_, desc)));
        impl_->name_to_id[desc.name] = dirt_id_;
    }

    // Grass (ID 3)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:grass";
        desc.display_name = "Grass Block";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable;
        desc.material = MaterialProperties::dirt();
        desc.texture_index = 3;   // Side texture
        desc.texture_top = 4;     // Top texture (grass)
        desc.texture_bottom = 2;  // Bottom texture (dirt)
        grass_id_ = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(grass_id_, desc)));
        impl_->name_to_id[desc.name] = grass_id_;
    }

    // Water (ID 4)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:water";
        desc.display_name = "Water";
        desc.flags = BlockFlags::Transparent | BlockFlags::Liquid;
        desc.material = MaterialProperties::water();
        desc.texture_index = 5;
        desc.light_absorption = 2;  // Water absorbs some light
        water_id_ = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(water_id_, desc)));
        impl_->name_to_id[desc.name] = water_id_;
    }

    // Sand (ID 5)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:sand";
        desc.display_name = "Sand";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable |
                     BlockFlags::Gravity;
        desc.material = MaterialProperties::sand();
        desc.texture_index = 6;
        sand_id_ = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(sand_id_, desc)));
        impl_->name_to_id[desc.name] = sand_id_;
    }

    // Gravel (ID 6)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:gravel";
        desc.display_name = "Gravel";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable |
                     BlockFlags::Gravity;
        desc.material = MaterialProperties{1800.0f, 100.0f, 1.0f, 0.5f, 0.0f, "gravel"};
        desc.texture_index = 7;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Wood (ID 7)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:wood";
        desc.display_name = "Oak Wood";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable |
                     BlockFlags::Flammable;
        desc.material = MaterialProperties::wood();
        desc.texture_index = 8;   // Side (bark)
        desc.texture_top = 9;     // Top (rings)
        desc.texture_bottom = 9;  // Bottom (rings)
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Leaves (ID 8)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:leaves";
        desc.display_name = "Oak Leaves";
        desc.flags = BlockFlags::Solid | BlockFlags::Transparent | BlockFlags::HasCollision | BlockFlags::Breakable |
                     BlockFlags::Flammable;
        desc.material = MaterialProperties{100.0f, 10.0f, 0.2f, 0.3f, 0.0f, "grass"};
        desc.texture_index = 10;
        desc.light_absorption = 1;  // Leaves allow some light through
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Glass (ID 9)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:glass";
        desc.display_name = "Glass";
        desc.flags = BlockFlags::Solid | BlockFlags::Transparent | BlockFlags::FullCube | BlockFlags::HasCollision |
                     BlockFlags::Breakable;
        desc.material = MaterialProperties::glass();
        desc.texture_index = 11;
        desc.light_absorption = 0;  // Glass is fully transparent
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Torch (ID 10)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:torch";
        desc.display_name = "Torch";
        desc.flags = BlockFlags::Transparent | BlockFlags::Emissive | BlockFlags::Breakable;
        desc.material = MaterialProperties{50.0f, 5.0f, 0.0f, 0.0f, 0.0f, "wood"};
        desc.texture_index = 12;
        desc.light_emission = 14;
        desc.light_absorption = 0;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Bedrock (ID 11) - unbreakable
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:bedrock";
        desc.display_name = "Bedrock";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision;  // Note: NOT Breakable
        desc.material = MaterialProperties{10000.0f, 100000.0f, 100.0f, 0.9f, 0.0f, "stone"};
        desc.texture_index = 13;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    REALCRAFT_LOG_INFO(core::log_category::WORLD, "Registered {} default block types", impl_->blocks.size());
}

const BlockType* BlockRegistry::get(BlockId id) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (id >= impl_->blocks.size()) {
        return nullptr;
    }
    return impl_->blocks[id].get();
}

const BlockType* BlockRegistry::get(std::string_view name) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto it = impl_->name_to_id.find(std::string(name));
    if (it == impl_->name_to_id.end()) {
        return nullptr;
    }
    return impl_->blocks[it->second].get();
}

std::optional<BlockId> BlockRegistry::find_id(std::string_view name) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto it = impl_->name_to_id.find(std::string(name));
    if (it == impl_->name_to_id.end()) {
        return std::nullopt;
    }
    return it->second;
}

size_t BlockRegistry::count() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->blocks.size();
}

void BlockRegistry::for_each(const std::function<void(const BlockType&)>& callback) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (const auto& block : impl_->blocks) {
        callback(*block);
    }
}

void BlockRegistry::set_placed_callback(BlockPlacedCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->placed_callback = std::move(callback);
}

void BlockRegistry::set_broken_callback(BlockBrokenCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->broken_callback = std::move(callback);
}

void BlockRegistry::set_tick_callback(BlockId id, BlockTickCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->tick_callbacks[id] = std::move(callback);
}

void BlockRegistry::on_block_placed(const WorldBlockPos& pos, BlockId id, BlockStateId state) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->placed_callback) {
        impl_->placed_callback(pos, id, state);
    }
}

void BlockRegistry::on_block_broken(const WorldBlockPos& pos, BlockId id) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->broken_callback) {
        impl_->broken_callback(pos, id);
    }
}

void BlockRegistry::on_block_tick(const WorldBlockPos& pos, BlockId id, double dt) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto it = impl_->tick_callbacks.find(id);
    if (it != impl_->tick_callbacks.end()) {
        it->second(pos, id, dt);
    }
}

}  // namespace realcraft::world
