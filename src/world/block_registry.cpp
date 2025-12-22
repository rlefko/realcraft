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

    // Silt (ID 12) - Fine sediment from erosion
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:silt";
        desc.display_name = "Silt";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable;
        desc.material = MaterialProperties{1400.0f, 80.0f, 0.8f, 0.5f, 0.0f, "dirt"};
        desc.texture_index = 14;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Alluvium (ID 13) - Coarser sediment from erosion (floodplains)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:alluvium";
        desc.display_name = "Alluvium";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable;
        desc.material = MaterialProperties{1600.0f, 120.0f, 1.0f, 0.55f, 0.0f, "dirt"};
        desc.texture_index = 15;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // River Gravel (ID 14) - Gravel deposits at river mouths
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:river_gravel";
        desc.display_name = "River Gravel";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable |
                     BlockFlags::Gravity;
        desc.material = MaterialProperties{1900.0f, 110.0f, 1.2f, 0.6f, 0.0f, "gravel"};
        desc.texture_index = 16;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Stalactite (ID 15) - Hangs from cave ceiling
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:stalactite";
        desc.display_name = "Stalactite";
        desc.flags = BlockFlags::Solid | BlockFlags::HasCollision | BlockFlags::Breakable;
        desc.material = MaterialProperties{2200.0f, 800.0f, 4.0f, 0.7f, 0.0f, "stone"};
        desc.texture_index = 17;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Stalagmite (ID 16) - Rises from cave floor
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:stalagmite";
        desc.display_name = "Stalagmite";
        desc.flags = BlockFlags::Solid | BlockFlags::HasCollision | BlockFlags::Breakable;
        desc.material = MaterialProperties{2200.0f, 800.0f, 4.0f, 0.7f, 0.0f, "stone"};
        desc.texture_index = 18;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Crystal (ID 17) - Emissive crystal formation
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:crystal";
        desc.display_name = "Crystal";
        desc.flags = BlockFlags::Solid | BlockFlags::Transparent | BlockFlags::Emissive | BlockFlags::HasCollision |
                     BlockFlags::Breakable;
        desc.material = MaterialProperties{1500.0f, 100.0f, 2.0f, 0.3f, 0.0f, "glass"};
        desc.texture_index = 19;
        desc.light_emission = 7;
        desc.light_absorption = 0;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Cave Moss (ID 18) - Grows in damp cave areas
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:cave_moss";
        desc.display_name = "Cave Moss";
        desc.flags = BlockFlags::Transparent | BlockFlags::Replaceable | BlockFlags::Breakable;
        desc.material = MaterialProperties{50.0f, 5.0f, 0.1f, 0.2f, 0.0f, "grass"};
        desc.texture_index = 20;
        desc.light_absorption = 0;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Glowing Mushroom (ID 19) - Bioluminescent fungi
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:glowing_mushroom";
        desc.display_name = "Glowing Mushroom";
        desc.flags = BlockFlags::Transparent | BlockFlags::Emissive | BlockFlags::Breakable;
        desc.material = MaterialProperties{20.0f, 2.0f, 0.0f, 0.1f, 0.0f, "grass"};
        desc.texture_index = 21;
        desc.light_emission = 5;
        desc.light_absorption = 0;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Dripstone (ID 20) - Pointed dripstone block
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:dripstone";
        desc.display_name = "Dripstone";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable;
        desc.material = MaterialProperties{2000.0f, 600.0f, 3.5f, 0.6f, 0.0f, "stone"};
        desc.texture_index = 22;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // ========================================================================
    // Tree Types (IDs 21-30)
    // ========================================================================

    // Spruce Log (ID 21)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:spruce_log";
        desc.display_name = "Spruce Log";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable |
                     BlockFlags::Flammable;
        desc.material = MaterialProperties::wood();
        desc.texture_index = 23;   // Side (bark)
        desc.texture_top = 24;     // Top (rings)
        desc.texture_bottom = 24;  // Bottom (rings)
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Spruce Leaves (ID 22)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:spruce_leaves";
        desc.display_name = "Spruce Leaves";
        desc.flags = BlockFlags::Solid | BlockFlags::Transparent | BlockFlags::HasCollision | BlockFlags::Breakable |
                     BlockFlags::Flammable;
        desc.material = MaterialProperties{100.0f, 10.0f, 0.2f, 0.3f, 0.0f, "grass"};
        desc.texture_index = 25;
        desc.light_absorption = 1;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Birch Log (ID 23)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:birch_log";
        desc.display_name = "Birch Log";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable |
                     BlockFlags::Flammable;
        desc.material = MaterialProperties::wood();
        desc.texture_index = 26;   // Side (white bark)
        desc.texture_top = 27;     // Top (rings)
        desc.texture_bottom = 27;  // Bottom (rings)
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Birch Leaves (ID 24)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:birch_leaves";
        desc.display_name = "Birch Leaves";
        desc.flags = BlockFlags::Solid | BlockFlags::Transparent | BlockFlags::HasCollision | BlockFlags::Breakable |
                     BlockFlags::Flammable;
        desc.material = MaterialProperties{100.0f, 10.0f, 0.2f, 0.3f, 0.0f, "grass"};
        desc.texture_index = 28;
        desc.light_absorption = 1;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Jungle Log (ID 25)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:jungle_log";
        desc.display_name = "Jungle Log";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable |
                     BlockFlags::Flammable;
        desc.material = MaterialProperties::wood();
        desc.texture_index = 29;   // Side (bark)
        desc.texture_top = 30;     // Top (rings)
        desc.texture_bottom = 30;  // Bottom (rings)
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Jungle Leaves (ID 26)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:jungle_leaves";
        desc.display_name = "Jungle Leaves";
        desc.flags = BlockFlags::Solid | BlockFlags::Transparent | BlockFlags::HasCollision | BlockFlags::Breakable |
                     BlockFlags::Flammable;
        desc.material = MaterialProperties{100.0f, 10.0f, 0.2f, 0.3f, 0.0f, "grass"};
        desc.texture_index = 31;
        desc.light_absorption = 2;  // Denser foliage
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Acacia Log (ID 27)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:acacia_log";
        desc.display_name = "Acacia Log";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable |
                     BlockFlags::Flammable;
        desc.material = MaterialProperties::wood();
        desc.texture_index = 32;   // Side (gray bark)
        desc.texture_top = 33;     // Top (orange rings)
        desc.texture_bottom = 33;  // Bottom (orange rings)
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Acacia Leaves (ID 28)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:acacia_leaves";
        desc.display_name = "Acacia Leaves";
        desc.flags = BlockFlags::Solid | BlockFlags::Transparent | BlockFlags::HasCollision | BlockFlags::Breakable |
                     BlockFlags::Flammable;
        desc.material = MaterialProperties{100.0f, 10.0f, 0.2f, 0.3f, 0.0f, "grass"};
        desc.texture_index = 34;
        desc.light_absorption = 1;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Palm Log (ID 29) - For beach/desert oasis
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:palm_log";
        desc.display_name = "Palm Log";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable |
                     BlockFlags::Flammable;
        desc.material = MaterialProperties::wood();
        desc.texture_index = 35;
        desc.texture_top = 36;
        desc.texture_bottom = 36;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Palm Leaves (ID 30)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:palm_leaves";
        desc.display_name = "Palm Leaves";
        desc.flags = BlockFlags::Solid | BlockFlags::Transparent | BlockFlags::HasCollision | BlockFlags::Breakable |
                     BlockFlags::Flammable;
        desc.material = MaterialProperties{100.0f, 10.0f, 0.2f, 0.3f, 0.0f, "grass"};
        desc.texture_index = 37;
        desc.light_absorption = 1;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // ========================================================================
    // Ground Cover Vegetation (IDs 31-35)
    // ========================================================================

    // Tall Grass (ID 31)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:tall_grass";
        desc.display_name = "Tall Grass";
        desc.flags = BlockFlags::Transparent | BlockFlags::Replaceable | BlockFlags::Breakable | BlockFlags::Flammable;
        desc.material = MaterialProperties{5.0f, 1.0f, 0.0f, 0.1f, 0.0f, "grass"};
        desc.texture_index = 38;
        desc.light_absorption = 0;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Fern (ID 32)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:fern";
        desc.display_name = "Fern";
        desc.flags = BlockFlags::Transparent | BlockFlags::Replaceable | BlockFlags::Breakable | BlockFlags::Flammable;
        desc.material = MaterialProperties{5.0f, 1.0f, 0.0f, 0.1f, 0.0f, "grass"};
        desc.texture_index = 39;
        desc.light_absorption = 0;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Dead Bush (ID 33)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:dead_bush";
        desc.display_name = "Dead Bush";
        desc.flags = BlockFlags::Transparent | BlockFlags::Replaceable | BlockFlags::Breakable | BlockFlags::Flammable;
        desc.material = MaterialProperties{3.0f, 1.0f, 0.0f, 0.1f, 0.0f, "wood"};
        desc.texture_index = 40;
        desc.light_absorption = 0;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Cactus (ID 34)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:cactus";
        desc.display_name = "Cactus";
        desc.flags = BlockFlags::Solid | BlockFlags::HasCollision | BlockFlags::Breakable;
        desc.material = MaterialProperties{400.0f, 50.0f, 0.4f, 0.3f, 0.0f, "grass"};
        desc.texture_index = 41;   // Side with spines
        desc.texture_top = 42;     // Top
        desc.texture_bottom = 42;  // Bottom
        desc.light_absorption = 0;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Bush (ID 35)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:bush";
        desc.display_name = "Bush";
        desc.flags = BlockFlags::Solid | BlockFlags::Transparent | BlockFlags::HasCollision | BlockFlags::Breakable |
                     BlockFlags::Flammable;
        desc.material = MaterialProperties{50.0f, 10.0f, 0.1f, 0.2f, 0.0f, "grass"};
        desc.texture_index = 43;
        desc.light_absorption = 1;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // ========================================================================
    // Flowers (IDs 36-43)
    // ========================================================================

    // Poppy (ID 36)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:poppy";
        desc.display_name = "Poppy";
        desc.flags = BlockFlags::Transparent | BlockFlags::Replaceable | BlockFlags::Breakable;
        desc.material = MaterialProperties{2.0f, 1.0f, 0.0f, 0.1f, 0.0f, "grass"};
        desc.texture_index = 44;
        desc.light_absorption = 0;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Dandelion (ID 37)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:dandelion";
        desc.display_name = "Dandelion";
        desc.flags = BlockFlags::Transparent | BlockFlags::Replaceable | BlockFlags::Breakable;
        desc.material = MaterialProperties{2.0f, 1.0f, 0.0f, 0.1f, 0.0f, "grass"};
        desc.texture_index = 45;
        desc.light_absorption = 0;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Blue Orchid (ID 38) - Swamp flower
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:blue_orchid";
        desc.display_name = "Blue Orchid";
        desc.flags = BlockFlags::Transparent | BlockFlags::Replaceable | BlockFlags::Breakable;
        desc.material = MaterialProperties{2.0f, 1.0f, 0.0f, 0.1f, 0.0f, "grass"};
        desc.texture_index = 46;
        desc.light_absorption = 0;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Azure Bluet (ID 39)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:azure_bluet";
        desc.display_name = "Azure Bluet";
        desc.flags = BlockFlags::Transparent | BlockFlags::Replaceable | BlockFlags::Breakable;
        desc.material = MaterialProperties{2.0f, 1.0f, 0.0f, 0.1f, 0.0f, "grass"};
        desc.texture_index = 47;
        desc.light_absorption = 0;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Red Tulip (ID 40)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:tulip_red";
        desc.display_name = "Red Tulip";
        desc.flags = BlockFlags::Transparent | BlockFlags::Replaceable | BlockFlags::Breakable;
        desc.material = MaterialProperties{2.0f, 1.0f, 0.0f, 0.1f, 0.0f, "grass"};
        desc.texture_index = 48;
        desc.light_absorption = 0;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Cornflower (ID 41)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:cornflower";
        desc.display_name = "Cornflower";
        desc.flags = BlockFlags::Transparent | BlockFlags::Replaceable | BlockFlags::Breakable;
        desc.material = MaterialProperties{2.0f, 1.0f, 0.0f, 0.1f, 0.0f, "grass"};
        desc.texture_index = 49;
        desc.light_absorption = 0;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Lily Pad (ID 42) - Floats on water
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:lily_pad";
        desc.display_name = "Lily Pad";
        desc.flags = BlockFlags::Transparent | BlockFlags::Replaceable | BlockFlags::Breakable;
        desc.material = MaterialProperties{5.0f, 2.0f, 0.0f, 0.1f, 0.0f, "grass"};
        desc.texture_index = 50;
        desc.light_absorption = 0;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Sunflower (ID 43) - Tall flower
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:sunflower";
        desc.display_name = "Sunflower";
        desc.flags = BlockFlags::Transparent | BlockFlags::Replaceable | BlockFlags::Breakable;
        desc.material = MaterialProperties{5.0f, 2.0f, 0.0f, 0.1f, 0.0f, "grass"};
        desc.texture_index = 51;
        desc.light_absorption = 0;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // ========================================================================
    // Ores (IDs 44-51)
    // ========================================================================

    // Coal Ore (ID 44)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:coal_ore";
        desc.display_name = "Coal Ore";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable;
        desc.material = MaterialProperties{2500.0f, 1000.0f, 3.0f, 0.7f, 0.0f, "stone"};
        desc.texture_index = 52;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Iron Ore (ID 45)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:iron_ore";
        desc.display_name = "Iron Ore";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable;
        desc.material = MaterialProperties{2700.0f, 1200.0f, 4.0f, 0.75f, 0.0f, "stone"};
        desc.texture_index = 53;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Copper Ore (ID 46)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:copper_ore";
        desc.display_name = "Copper Ore";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable;
        desc.material = MaterialProperties{2600.0f, 1100.0f, 3.5f, 0.72f, 0.0f, "stone"};
        desc.texture_index = 54;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Gold Ore (ID 47)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:gold_ore";
        desc.display_name = "Gold Ore";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable;
        desc.material = MaterialProperties{3000.0f, 1300.0f, 4.5f, 0.78f, 0.0f, "stone"};
        desc.texture_index = 55;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Redstone Ore (ID 48) - Emissive
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:redstone_ore";
        desc.display_name = "Redstone Ore";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable |
                     BlockFlags::Emissive;
        desc.material = MaterialProperties{2500.0f, 1100.0f, 4.0f, 0.74f, 0.0f, "stone"};
        desc.texture_index = 56;
        desc.light_emission = 3;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Lapis Ore (ID 49)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:lapis_ore";
        desc.display_name = "Lapis Lazuli Ore";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable;
        desc.material = MaterialProperties{2500.0f, 1100.0f, 4.0f, 0.74f, 0.0f, "stone"};
        desc.texture_index = 57;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Emerald Ore (ID 50) - Mountains only
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:emerald_ore";
        desc.display_name = "Emerald Ore";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable;
        desc.material = MaterialProperties{2800.0f, 1400.0f, 5.0f, 0.8f, 0.0f, "stone"};
        desc.texture_index = 58;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Diamond Ore (ID 51)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:diamond_ore";
        desc.display_name = "Diamond Ore";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable;
        desc.material = MaterialProperties{3200.0f, 1500.0f, 6.0f, 0.85f, 0.0f, "stone"};
        desc.texture_index = 59;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // ========================================================================
    // Structure Blocks (IDs 52-55)
    // ========================================================================

    // Mossy Stone (ID 52)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:mossy_stone";
        desc.display_name = "Mossy Stone";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable;
        desc.material = MaterialProperties::stone();
        desc.texture_index = 60;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Cobblestone (ID 53)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:cobblestone";
        desc.display_name = "Cobblestone";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable;
        desc.material = MaterialProperties::stone();
        desc.texture_index = 61;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Cracked Stone (ID 54)
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:cracked_stone";
        desc.display_name = "Cracked Stone";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable;
        desc.material = MaterialProperties{2300.0f, 900.0f, 4.5f, 0.68f, 0.0f, "stone"};
        desc.texture_index = 62;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Pillar Stone (ID 55) - Natural rock formation
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:pillar_stone";
        desc.display_name = "Pillar Stone";
        desc.flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable;
        desc.material = MaterialProperties::stone();
        desc.texture_index = 63;
        auto id = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(id, desc)));
        impl_->name_to_id[desc.name] = id;
    }

    // Ladder (ID 56) - Climbable block
    {
        BlockTypeDesc desc;
        desc.name = "realcraft:ladder";
        desc.display_name = "Ladder";
        desc.flags = BlockFlags::Transparent | BlockFlags::HasCollision | BlockFlags::Breakable |
                     BlockFlags::Climbable | BlockFlags::HasBlockState;
        desc.material = MaterialProperties::wood();
        desc.material.weight = 20.0f;            // Very light
        desc.material.requires_support = false;  // Can be placed on walls
        desc.texture_index = 64;
        desc.light_absorption = 0;  // Fully transparent
        desc.state_count = 4;       // 4 orientations
        ladder_id_ = static_cast<BlockId>(impl_->blocks.size());
        impl_->blocks.push_back(std::unique_ptr<BlockType>(new BlockType(ladder_id_, desc)));
        impl_->name_to_id[desc.name] = ladder_id_;
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
