// RealCraft Gameplay System
// item_entity.hpp - Dropped item entities in the world

#pragma once

#include <glm/vec3.hpp>

#include <memory>
#include <realcraft/gameplay/inventory.hpp>
#include <realcraft/gameplay/item.hpp>
#include <realcraft/world/types.hpp>
#include <vector>

namespace realcraft::physics {
class PhysicsWorld;
}

namespace realcraft::world {
class WorldManager;
}

namespace realcraft::gameplay {

// ============================================================================
// Item Entity (dropped item in the world)
// ============================================================================

struct ItemEntity {
    uint32_t id = 0;           // Unique entity ID
    glm::dvec3 position{0.0};  // World position
    glm::dvec3 velocity{0.0};  // Current velocity
    ItemStack item;            // The item(s) in this entity
    double spawn_time = 0.0;   // When the entity was created
    double lifetime = 300.0;   // How long before despawn (5 minutes)
    float rotation = 0.0f;     // Visual rotation (radians)
    float bob_offset = 0.0f;   // Vertical bob animation offset
    bool on_ground = false;    // Is the item resting on ground
    bool can_pickup = false;   // Can be picked up (false during pickup delay)
    bool marked_for_removal = false;

    // Physics constants
    static constexpr double PICKUP_DELAY = 0.5;     // Seconds before pickup is allowed
    static constexpr double PICKUP_RADIUS = 2.0;    // Distance for auto-pickup
    static constexpr double MERGE_RADIUS = 0.5;     // Distance to merge with other items
    static constexpr double GRAVITY = 20.0;         // m/s^2
    static constexpr double GROUND_FRICTION = 0.8;  // Friction when on ground
    static constexpr double AIR_RESISTANCE = 0.02;  // Air drag
    static constexpr double BOUNCE_FACTOR = 0.3;    // How bouncy items are
    static constexpr float ROTATION_SPEED = 1.0f;   // Radians per second
    static constexpr float BOB_SPEED = 2.0f;        // Bob animation speed
    static constexpr float BOB_AMPLITUDE = 0.1f;    // Bob height in blocks
};

// ============================================================================
// Item Entity Manager Configuration
// ============================================================================

struct ItemEntityConfig {
    size_t max_entities = 500;        // Maximum active item entities
    double default_lifetime = 300.0;  // Default despawn time (5 minutes)
    double pickup_radius = 2.0;       // Distance for auto-pickup
    double pickup_delay = 0.5;        // Delay before items can be picked up
    bool enable_merging = true;       // Merge nearby identical items
    bool enable_physics = true;       // Enable gravity and collisions
};

// ============================================================================
// Item Entity Manager
// ============================================================================

class ItemEntityManager {
public:
    ItemEntityManager();
    ~ItemEntityManager();

    // Non-copyable, movable
    ItemEntityManager(const ItemEntityManager&) = delete;
    ItemEntityManager& operator=(const ItemEntityManager&) = delete;
    ItemEntityManager(ItemEntityManager&&) noexcept;
    ItemEntityManager& operator=(ItemEntityManager&&) noexcept;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    bool initialize(physics::PhysicsWorld* physics_world, world::WorldManager* world_manager,
                    Inventory* player_inventory, const ItemEntityConfig& config = {});
    void shutdown();
    [[nodiscard]] bool is_initialized() const { return initialized_; }

    // ========================================================================
    // Update
    // ========================================================================

    // Update all item entities (physics, pickup checks, despawn)
    void update(double dt, const glm::dvec3& player_position);

    // ========================================================================
    // Spawning
    // ========================================================================

    // Spawn a single item entity
    // Returns the entity ID, or 0 if failed
    uint32_t spawn_item(const glm::dvec3& position, const ItemStack& item,
                        const glm::dvec3& velocity = {0.0, 0.0, 0.0});

    // Spawn drops for a broken block
    void spawn_block_drops(const glm::dvec3& position, world::BlockId block_id);

    // Spawn with random velocity spread (for block breaking)
    uint32_t spawn_item_scattered(const glm::dvec3& position, const ItemStack& item);

    // ========================================================================
    // Queries
    // ========================================================================

    // Get number of active entities
    [[nodiscard]] size_t entity_count() const;

    // Get all entities (for rendering)
    [[nodiscard]] const std::vector<ItemEntity>& get_entities() const;

    // Find entity by ID
    [[nodiscard]] const ItemEntity* find_entity(uint32_t id) const;

    // ========================================================================
    // Manual Control
    // ========================================================================

    // Force pickup of a specific entity
    bool pickup_entity(uint32_t id);

    // Remove a specific entity
    void remove_entity(uint32_t id);

    // Clear all entities
    void clear();

    // ========================================================================
    // Configuration
    // ========================================================================

    [[nodiscard]] const ItemEntityConfig& get_config() const { return config_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    physics::PhysicsWorld* physics_world_ = nullptr;
    world::WorldManager* world_manager_ = nullptr;
    Inventory* player_inventory_ = nullptr;

    ItemEntityConfig config_;
    bool initialized_ = false;

    // Internal methods
    void update_physics(ItemEntity& entity, double dt);
    void update_pickup(ItemEntity& entity, const glm::dvec3& player_position, double current_time);
    void update_merging();
    void cleanup_entities(double current_time);
    [[nodiscard]] bool check_ground_collision(ItemEntity& entity);
    [[nodiscard]] bool is_position_solid(const glm::dvec3& position) const;
};

}  // namespace realcraft::gameplay
