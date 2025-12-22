// RealCraft Gameplay System
// block_interaction.hpp - Block targeting, breaking, and placement

#pragma once

#include <realcraft/physics/physics_world.hpp>
#include <realcraft/physics/player_controller.hpp>
#include <realcraft/rendering/camera.hpp>
#include <realcraft/world/block.hpp>
#include <realcraft/world/world_manager.hpp>

namespace realcraft::gameplay {

// Forward declarations
class Inventory;
class ItemEntityManager;

// ============================================================================
// Target Information
// ============================================================================

struct TargetInfo {
    bool has_target = false;
    world::WorldBlockPos block_position{0, 0, 0};
    world::Direction hit_face = world::Direction::PosY;
    world::BlockId block_id = world::BLOCK_AIR;
    double distance = 0.0;
    glm::dvec3 hit_point{0.0};

    // Get position for block placement (adjacent to hit face)
    [[nodiscard]] world::WorldBlockPos placement_position() const {
        if (!has_target) {
            return block_position;
        }
        glm::ivec3 offset = world::direction_offset(hit_face);
        return world::WorldBlockPos(block_position.x + offset.x, block_position.y + offset.y,
                                    block_position.z + offset.z);
    }
};

// ============================================================================
// Mining Progress
// ============================================================================

struct MiningProgress {
    bool is_mining = false;
    world::WorldBlockPos target{0, 0, 0};
    double progress = 0.0;       // 0.0 to 1.0
    double start_time = 0.0;     // When mining started
    double required_time = 0.0;  // Total time to break block

    void reset() {
        is_mining = false;
        progress = 0.0;
        start_time = 0.0;
        required_time = 0.0;
    }
};

// ============================================================================
// Block Interaction Configuration
// ============================================================================

struct BlockInteractionConfig {
    double reach_distance = 5.0;    // Max block interaction distance
    double base_break_speed = 1.0;  // Mining speed multiplier (1.0 = hand)
    bool instant_break = false;     // For creative mode
    bool enable_placement = true;   // Allow block placement
    bool enable_breaking = true;    // Allow block breaking
};

// ============================================================================
// Block Interaction System
// ============================================================================

class BlockInteractionSystem {
public:
    BlockInteractionSystem();
    ~BlockInteractionSystem();

    // Non-copyable, movable
    BlockInteractionSystem(const BlockInteractionSystem&) = delete;
    BlockInteractionSystem& operator=(const BlockInteractionSystem&) = delete;
    BlockInteractionSystem(BlockInteractionSystem&&) noexcept;
    BlockInteractionSystem& operator=(BlockInteractionSystem&&) noexcept;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    bool initialize(physics::PhysicsWorld* physics_world, physics::PlayerController* player_controller,
                    world::WorldManager* world_manager, rendering::Camera* camera,
                    const BlockInteractionConfig& config = {});
    void shutdown();
    [[nodiscard]] bool is_initialized() const;

    // ========================================================================
    // Update (call each frame from update callback)
    // ========================================================================

    void update(double delta_time);

    // ========================================================================
    // Input Handling
    // ========================================================================

    // Called when primary action (left click) state changes
    void on_primary_action(bool pressed);

    // Called when secondary action (right click) is triggered
    // Note: Placement happens on just_pressed, not while held
    void on_secondary_action(bool just_pressed);

    // ========================================================================
    // State Queries
    // ========================================================================

    [[nodiscard]] const TargetInfo& get_target() const { return target_; }
    [[nodiscard]] const MiningProgress& get_mining_progress() const { return mining_; }
    [[nodiscard]] bool is_mining() const { return mining_.is_mining; }
    [[nodiscard]] bool has_target() const { return target_.has_target; }

    // ========================================================================
    // Inventory Integration
    // ========================================================================

    // Set the player inventory (required for getting held block)
    void set_inventory(Inventory* inventory);

    // Set the item entity manager (required for spawning drops)
    void set_item_entity_manager(ItemEntityManager* item_manager);

    // Get the held block from inventory (returns AIR if no block held)
    [[nodiscard]] world::BlockId get_held_block() const;

    // ========================================================================
    // Legacy Block Selection (for backward compatibility / testing)
    // ========================================================================

    // Manually override the held block (bypasses inventory)
    void set_held_block_override(world::BlockId block_id);

    // Clear the manual override (use inventory again)
    void clear_held_block_override();

    // Cycle to next/previous block type (for testing without inventory)
    void cycle_held_block(bool forward = true);

    // ========================================================================
    // Configuration
    // ========================================================================

    void set_config(const BlockInteractionConfig& config) { config_ = config; }
    [[nodiscard]] const BlockInteractionConfig& get_config() const { return config_; }

private:
    physics::PhysicsWorld* physics_world_ = nullptr;
    physics::PlayerController* player_controller_ = nullptr;
    world::WorldManager* world_manager_ = nullptr;
    rendering::Camera* camera_ = nullptr;

    // Inventory integration
    Inventory* inventory_ = nullptr;
    ItemEntityManager* item_entity_manager_ = nullptr;

    BlockInteractionConfig config_;
    TargetInfo target_;
    MiningProgress mining_;

    // Override held block (for testing without inventory)
    world::BlockId held_block_override_ = world::BLOCK_AIR;
    bool use_held_block_override_ = false;

    bool primary_pressed_ = false;
    bool initialized_ = false;
    double current_time_ = 0.0;

    // Internal methods
    void update_target();
    void update_mining(double delta_time);
    void start_mining(const world::WorldBlockPos& pos);
    void reset_mining();
    void break_block(const world::WorldBlockPos& pos);
    void place_block(world::BlockId block_id);
    [[nodiscard]] bool can_place_block(const world::WorldBlockPos& pos, world::BlockId block_id) const;
    [[nodiscard]] double calculate_break_time(world::BlockId block_id) const;
};

}  // namespace realcraft::gameplay
