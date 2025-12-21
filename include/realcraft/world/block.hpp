// RealCraft World System
// block.hpp - Block type system and registry

#pragma once

#include "types.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace realcraft::world {

// ============================================================================
// Material Properties (for physics integration)
// ============================================================================

struct MaterialProperties {
    float weight = 1.0f;               // kg per block
    float strength = 100.0f;           // Support capacity (N)
    float hardness = 1.0f;             // Mining difficulty (0-10)
    float friction = 0.6f;             // Surface friction coefficient
    float restitution = 0.0f;          // Bounciness (0-1)
    std::string sound_type = "stone";  // Sound category for footsteps, mining, etc.

    // Structural integrity properties
    float max_support_distance = 64.0f;  // Max blocks from ground before collapse
    float horizontal_cost = 1.5f;        // Support distance cost for horizontal spans
    float brittleness = 0.0f;            // 0.0 = stable, 1.0 = fragile under stress
    bool requires_support = true;        // False for fluids, decorations, etc.

    // Common material presets
    static MaterialProperties stone() {
        MaterialProperties m;
        m.weight = 2500.0f;
        m.strength = 1000.0f;
        m.hardness = 5.0f;
        m.friction = 0.7f;
        m.restitution = 0.0f;
        m.sound_type = "stone";
        m.max_support_distance = 64.0f;
        m.horizontal_cost = 1.2f;
        m.brittleness = 0.0f;
        m.requires_support = true;
        return m;
    }

    static MaterialProperties dirt() {
        MaterialProperties m;
        m.weight = 1500.0f;
        m.strength = 200.0f;
        m.hardness = 1.5f;
        m.friction = 0.6f;
        m.restitution = 0.0f;
        m.sound_type = "dirt";
        m.max_support_distance = 16.0f;
        m.horizontal_cost = 2.0f;
        m.brittleness = 0.1f;
        m.requires_support = true;
        return m;
    }

    static MaterialProperties wood() {
        MaterialProperties m;
        m.weight = 600.0f;
        m.strength = 400.0f;
        m.hardness = 2.0f;
        m.friction = 0.5f;
        m.restitution = 0.0f;
        m.sound_type = "wood";
        m.max_support_distance = 32.0f;
        m.horizontal_cost = 1.5f;
        m.brittleness = 0.05f;
        m.requires_support = true;
        return m;
    }

    static MaterialProperties sand() {
        MaterialProperties m;
        m.weight = 1600.0f;
        m.strength = 50.0f;
        m.hardness = 0.5f;
        m.friction = 0.4f;
        m.restitution = 0.0f;
        m.sound_type = "sand";
        m.max_support_distance = 2.0f;
        m.horizontal_cost = 3.0f;
        m.brittleness = 0.3f;
        m.requires_support = true;
        return m;
    }

    static MaterialProperties glass() {
        MaterialProperties m;
        m.weight = 2500.0f;
        m.strength = 50.0f;
        m.hardness = 0.3f;
        m.friction = 0.5f;
        m.restitution = 0.0f;
        m.sound_type = "glass";
        m.max_support_distance = 8.0f;
        m.horizontal_cost = 2.0f;
        m.brittleness = 0.5f;
        m.requires_support = true;
        return m;
    }

    static MaterialProperties water() {
        MaterialProperties m;
        m.weight = 1000.0f;
        m.strength = 0.0f;
        m.hardness = 0.0f;
        m.friction = 0.0f;
        m.restitution = 0.0f;
        m.sound_type = "water";
        m.max_support_distance = 0.0f;
        m.horizontal_cost = 0.0f;
        m.brittleness = 0.0f;
        m.requires_support = false;
        return m;
    }
};

// ============================================================================
// Block Type Descriptor (for registration)
// ============================================================================

struct BlockTypeDesc {
    std::string name;          // Unique identifier "realcraft:stone"
    std::string display_name;  // Human-readable name (empty = derive from name)
    BlockFlags flags = BlockFlags::Solid | BlockFlags::FullCube | BlockFlags::HasCollision | BlockFlags::Breakable;
    MaterialProperties material;

    // Rendering
    uint16_t texture_index = 0;   // Index into texture atlas
    uint16_t texture_top = 0;     // Override for top face (0 = use texture_index)
    uint16_t texture_bottom = 0;  // Override for bottom face (0 = use texture_index)
    uint16_t texture_side = 0;    // Override for side faces (0 = use texture_index)

    // Light
    uint8_t light_emission = 0;     // 0-15 light level
    uint8_t light_absorption = 15;  // How much light is blocked (0-15)

    // Block states
    uint8_t state_count = 1;  // Number of variant states

    std::string debug_name;
};

// ============================================================================
// Block Type (immutable, registered)
// ============================================================================

class BlockType {
public:
    [[nodiscard]] BlockId get_id() const { return id_; }
    [[nodiscard]] const std::string& get_name() const { return name_; }
    [[nodiscard]] const std::string& get_display_name() const { return display_name_; }
    [[nodiscard]] BlockFlags get_flags() const { return flags_; }
    [[nodiscard]] const MaterialProperties& get_material() const { return material_; }

    // Flag checks
    [[nodiscard]] bool is_solid() const { return has_flag(flags_, BlockFlags::Solid); }
    [[nodiscard]] bool is_transparent() const { return has_flag(flags_, BlockFlags::Transparent); }
    [[nodiscard]] bool is_liquid() const { return has_flag(flags_, BlockFlags::Liquid); }
    [[nodiscard]] bool is_emissive() const { return has_flag(flags_, BlockFlags::Emissive); }
    [[nodiscard]] bool has_gravity() const { return has_flag(flags_, BlockFlags::Gravity); }
    [[nodiscard]] bool is_flammable() const { return has_flag(flags_, BlockFlags::Flammable); }
    [[nodiscard]] bool is_replaceable() const { return has_flag(flags_, BlockFlags::Replaceable); }
    [[nodiscard]] bool is_full_cube() const { return has_flag(flags_, BlockFlags::FullCube); }
    [[nodiscard]] bool has_collision() const { return has_flag(flags_, BlockFlags::HasCollision); }
    [[nodiscard]] bool is_breakable() const { return has_flag(flags_, BlockFlags::Breakable); }
    [[nodiscard]] bool has_block_state() const { return has_flag(flags_, BlockFlags::HasBlockState); }
    [[nodiscard]] bool is_air() const { return id_ == BLOCK_AIR; }

    // Rendering
    [[nodiscard]] uint16_t get_texture_index(Direction face) const;
    [[nodiscard]] uint8_t get_light_emission() const { return light_emission_; }
    [[nodiscard]] uint8_t get_light_absorption() const { return light_absorption_; }
    [[nodiscard]] uint8_t get_state_count() const { return state_count_; }

private:
    friend class BlockRegistry;
    BlockType(BlockId id, const BlockTypeDesc& desc);

    BlockId id_;
    std::string name_;
    std::string display_name_;
    BlockFlags flags_;
    MaterialProperties material_;
    uint16_t texture_index_;
    uint16_t texture_top_;
    uint16_t texture_bottom_;
    uint16_t texture_side_;
    uint8_t light_emission_;
    uint8_t light_absorption_;
    uint8_t state_count_;
};

// ============================================================================
// Block Registry (singleton)
// ============================================================================

class BlockRegistry {
public:
    // Singleton access
    [[nodiscard]] static BlockRegistry& instance();

    // Non-copyable, non-movable
    BlockRegistry(const BlockRegistry&) = delete;
    BlockRegistry& operator=(const BlockRegistry&) = delete;
    BlockRegistry(BlockRegistry&&) = delete;
    BlockRegistry& operator=(BlockRegistry&&) = delete;

    // Registration
    BlockId register_block(const BlockTypeDesc& desc);
    void register_defaults();

    // Lookup
    [[nodiscard]] const BlockType* get(BlockId id) const;
    [[nodiscard]] const BlockType* get(std::string_view name) const;
    [[nodiscard]] std::optional<BlockId> find_id(std::string_view name) const;

    // Iteration
    [[nodiscard]] size_t count() const;
    void for_each(const std::function<void(const BlockType&)>& callback) const;

    // Behavior hooks (called by game systems)
    using BlockPlacedCallback = std::function<void(const WorldBlockPos&, BlockId, BlockStateId)>;
    using BlockBrokenCallback = std::function<void(const WorldBlockPos&, BlockId)>;
    using BlockTickCallback = std::function<void(const WorldBlockPos&, BlockId, double dt)>;

    void set_placed_callback(BlockPlacedCallback callback);
    void set_broken_callback(BlockBrokenCallback callback);
    void set_tick_callback(BlockId id, BlockTickCallback callback);

    // Invoke callbacks (called internally by world systems)
    void on_block_placed(const WorldBlockPos& pos, BlockId id, BlockStateId state) const;
    void on_block_broken(const WorldBlockPos& pos, BlockId id) const;
    void on_block_tick(const WorldBlockPos& pos, BlockId id, double dt) const;

    // Common block IDs (set after register_defaults)
    [[nodiscard]] BlockId air_id() const { return BLOCK_AIR; }
    [[nodiscard]] BlockId stone_id() const { return stone_id_; }
    [[nodiscard]] BlockId dirt_id() const { return dirt_id_; }
    [[nodiscard]] BlockId grass_id() const { return grass_id_; }
    [[nodiscard]] BlockId water_id() const { return water_id_; }
    [[nodiscard]] BlockId sand_id() const { return sand_id_; }

private:
    BlockRegistry();
    ~BlockRegistry();

    struct Impl;
    std::unique_ptr<Impl> impl_;

    // Cache common block IDs
    BlockId stone_id_ = BLOCK_INVALID;
    BlockId dirt_id_ = BLOCK_INVALID;
    BlockId grass_id_ = BLOCK_INVALID;
    BlockId water_id_ = BLOCK_INVALID;
    BlockId sand_id_ = BLOCK_INVALID;
};

}  // namespace realcraft::world
