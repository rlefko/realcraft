// RealCraft Physics Engine
// debris_system.hpp - Debris lifecycle management and impact damage

#pragma once

#include "fragmentation.hpp"
#include "rigid_body.hpp"
#include "support_graph.hpp"
#include "types.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <realcraft/world/types.hpp>
#include <realcraft/world/world_manager.hpp>
#include <vector>

namespace realcraft::physics {

// Forward declarations
class PhysicsWorld;
class IImpactDamageHandler;

// ============================================================================
// Debris Configuration
// ============================================================================

struct DebrisConfig {
    // Lifecycle
    double default_lifetime_seconds = 30.0;  // Base lifetime before despawn
    double minimum_lifetime_seconds = 5.0;   // Minimum time before eligible for cleanup
    uint32_t max_active_debris = 100;        // Maximum concurrent debris objects
    uint32_t max_debris_per_frame = 8;       // Spawn limit per frame

    // Physics
    double impact_damage_threshold = 10.0;    // Minimum velocity for damage (m/s)
    double damage_velocity_multiplier = 0.5;  // damage = velocity * mass * multiplier
    double min_debris_mass_kg = 10.0;         // Debris below this mass deals no damage

    // Fragmentation
    uint32_t min_fragment_blocks = 1;         // Minimum blocks per fragment
    uint32_t max_fragment_blocks = 32;        // Maximum blocks per fragment
    bool use_convex_hulls = true;             // Use convex hull vs AABB for shapes
    double convex_hull_simplification = 0.1;  // Hull simplification tolerance
    FragmentationPattern fragmentation_pattern = FragmentationPattern::Brittle;

    // Performance
    bool enable_debris_sleeping = true;    // Allow debris to sleep after settling
    double debris_sleep_threshold = 0.5;   // Velocity below which debris is considered sleeping
    double sleep_cleanup_threshold = 0.8;  // Fraction of max_active_debris before cleaning sleeping debris

    // Impact detection
    bool enable_impact_damage = true;      // Whether debris impacts damage blocks
    double impact_cooldown_seconds = 0.1;  // Cooldown between impact damage events per debris
};

// ============================================================================
// Debris Data
// ============================================================================

struct DebrisData {
    RigidBodyHandle body_handle = INVALID_RIGID_BODY;
    double spawn_time = 0.0;
    double lifetime = 30.0;
    double total_mass = 0.0;

    // Block composition
    std::vector<world::BlockId> block_ids;
    size_t block_count = 0;

    // Material-based properties
    float average_brittleness = 0.0f;

    // Impact tracking
    double last_impact_time = 0.0;
    uint32_t impact_count = 0;

    // State
    bool is_sleeping = false;
    bool marked_for_removal = false;
};

// ============================================================================
// Debris Impact Event
// ============================================================================

struct DebrisImpactEvent {
    RigidBodyHandle debris_handle = INVALID_RIGID_BODY;
    glm::dvec3 impact_point{0.0};
    glm::dvec3 impact_normal{0.0};
    double impact_velocity = 0.0;
    double damage_amount = 0.0;

    // What was hit
    enum class TargetType { Block, Entity, Player, OtherDebris, Unknown };
    TargetType target_type = TargetType::Unknown;

    // Block target info (if target_type == Block)
    std::optional<world::WorldBlockPos> block_position;
    std::optional<world::BlockId> block_id;

    // Entity/Player target info (future use - Phase 8)
    uint32_t entity_id = 0;
};

using DebrisImpactCallback = std::function<void(const DebrisImpactEvent&)>;

// ============================================================================
// Debris System
// ============================================================================

class DebrisSystem {
public:
    DebrisSystem();
    ~DebrisSystem();

    // Non-copyable, non-movable
    DebrisSystem(const DebrisSystem&) = delete;
    DebrisSystem& operator=(const DebrisSystem&) = delete;
    DebrisSystem(DebrisSystem&&) = delete;
    DebrisSystem& operator=(DebrisSystem&&) = delete;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    bool initialize(PhysicsWorld* physics_world, world::WorldManager* world_manager, const DebrisConfig& config = {});
    void shutdown();
    [[nodiscard]] bool is_initialized() const;

    // Update (call each frame with delta time)
    void update(double delta_time);

    // ========================================================================
    // Debris Spawning
    // ========================================================================

    // Spawn debris from a structural collapse cluster
    // Returns handles to all spawned debris rigid bodies
    std::vector<RigidBodyHandle> spawn_debris_from_cluster(const BlockCluster& cluster);

    // Spawn a single debris object from specific positions
    RigidBodyHandle spawn_single_debris(const std::vector<world::WorldBlockPos>& positions,
                                        const std::vector<world::BlockId>& block_ids,
                                        const glm::dvec3& initial_velocity = glm::dvec3(0.0));

    // ========================================================================
    // Debris Queries
    // ========================================================================

    // Get debris data for a rigid body handle
    [[nodiscard]] const DebrisData* get_debris_data(RigidBodyHandle handle) const;

    // Check if a rigid body handle is debris
    [[nodiscard]] bool is_debris(RigidBodyHandle handle) const;

    // Get current number of active debris objects
    [[nodiscard]] size_t active_debris_count() const;

    // Get all active debris handles
    [[nodiscard]] std::vector<RigidBodyHandle> get_all_debris_handles() const;

    // ========================================================================
    // Manual Debris Removal
    // ========================================================================

    // Remove a specific debris object
    void remove_debris(RigidBodyHandle handle);

    // Remove all debris objects
    void clear_all_debris();

    // ========================================================================
    // Configuration
    // ========================================================================

    [[nodiscard]] const DebrisConfig& get_config() const;
    void set_config(const DebrisConfig& config);

    // ========================================================================
    // Impact Damage Handler
    // ========================================================================

    void set_impact_damage_handler(IImpactDamageHandler* handler);
    [[nodiscard]] IImpactDamageHandler* get_impact_damage_handler() const;

    // ========================================================================
    // Callbacks
    // ========================================================================

    void set_impact_callback(DebrisImpactCallback callback);

    // ========================================================================
    // Collision Handling (called by PhysicsWorld)
    // ========================================================================

    // Process a collision involving debris
    void on_collision(RigidBodyHandle body_a, RigidBodyHandle body_b, const glm::dvec3& contact_point,
                      const glm::dvec3& contact_normal, double impact_velocity);

    // ========================================================================
    // Statistics
    // ========================================================================

    struct Stats {
        size_t active_debris = 0;
        size_t sleeping_debris = 0;
        size_t total_spawned = 0;
        size_t total_despawned = 0;
        size_t impacts_this_frame = 0;
        size_t blocks_destroyed = 0;
        double total_damage_dealt = 0.0;
    };

    [[nodiscard]] Stats get_stats() const;
    void reset_stats();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::physics
