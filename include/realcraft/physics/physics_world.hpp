// RealCraft Physics Engine
// physics_world.hpp - Main physics world manager with Bullet integration

#pragma once

#include "chunk_collider.hpp"
#include "colliders.hpp"
#include "debris_system.hpp"
#include "ray_caster.hpp"
#include "rigid_body.hpp"
#include "structural_integrity.hpp"
#include "types.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <realcraft/world/world_manager.hpp>
#include <vector>

namespace realcraft::physics {

// ============================================================================
// Physics Configuration
// ============================================================================

struct PhysicsConfig {
    glm::dvec3 gravity = {0.0, -9.81, 0.0};
    double fixed_timestep = 1.0 / 60.0;
    int max_substeps = 4;
    double collision_margin = 0.01;
    bool enable_debug_draw = false;
};

// ============================================================================
// Collision Event
// ============================================================================

struct CollisionEvent {
    ColliderHandle collider_a = INVALID_COLLIDER;
    ColliderHandle collider_b = INVALID_COLLIDER;
    glm::dvec3 contact_point{0.0};
    glm::dvec3 contact_normal{0.0};
    double penetration_depth = 0.0;
};

using CollisionCallback = std::function<void(const CollisionEvent&)>;

// ============================================================================
// Physics World
// ============================================================================

class PhysicsWorld : public world::IWorldObserver {
public:
    PhysicsWorld();
    ~PhysicsWorld() override;

    // Non-copyable, non-movable
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;
    PhysicsWorld(PhysicsWorld&&) = delete;
    PhysicsWorld& operator=(PhysicsWorld&&) = delete;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    bool initialize(world::WorldManager* world_manager, const PhysicsConfig& config = {});
    void shutdown();
    [[nodiscard]] bool is_initialized() const;

    // Fixed timestep update (call from game loop's fixed_update)
    void fixed_update(double fixed_delta);

    // ========================================================================
    // Collider Management
    // ========================================================================

    [[nodiscard]] ColliderHandle create_aabb_collider(const AABBColliderDesc& desc);
    [[nodiscard]] ColliderHandle create_capsule_collider(const CapsuleColliderDesc& desc);
    [[nodiscard]] ColliderHandle create_convex_hull_collider(const ConvexHullColliderDesc& desc);

    void destroy_collider(ColliderHandle handle);
    [[nodiscard]] Collider* get_collider(ColliderHandle handle);
    [[nodiscard]] const Collider* get_collider(ColliderHandle handle) const;

    // ========================================================================
    // Rigid Body Management
    // ========================================================================

    [[nodiscard]] RigidBodyHandle create_rigid_body(const RigidBodyDesc& desc);
    void destroy_rigid_body(RigidBodyHandle handle);
    [[nodiscard]] RigidBody* get_rigid_body(RigidBodyHandle handle);
    [[nodiscard]] const RigidBody* get_rigid_body(RigidBodyHandle handle) const;

    // ========================================================================
    // Interpolation (for rendering between physics steps)
    // ========================================================================

    void set_interpolation_alpha(double alpha);
    [[nodiscard]] double get_interpolation_alpha() const;

    // ========================================================================
    // Ray Casting
    // ========================================================================

    // Cast ray against world voxels (uses DDA voxel traversal)
    [[nodiscard]] std::optional<RayHit> raycast_voxels(const glm::dvec3& origin, const glm::dvec3& direction,
                                                       double max_distance = 100.0) const;

    // Cast ray against all dynamic colliders
    [[nodiscard]] std::optional<RayHit> raycast_colliders(const glm::dvec3& origin, const glm::dvec3& direction,
                                                          double max_distance = 100.0,
                                                          uint32_t layer_mask = 0xFFFFFFFF) const;

    // Cast ray against both voxels and colliders (returns closest hit)
    [[nodiscard]] std::optional<RayHit> raycast(const glm::dvec3& origin, const glm::dvec3& direction,
                                                double max_distance = 100.0, uint32_t layer_mask = 0xFFFFFFFF) const;

    // ========================================================================
    // Collision Queries
    // ========================================================================

    [[nodiscard]] bool point_in_solid(const glm::dvec3& point) const;
    [[nodiscard]] std::vector<ColliderHandle> query_aabb(const AABB& aabb, uint32_t layer_mask = 0xFFFFFFFF) const;
    [[nodiscard]] std::vector<ColliderHandle> query_sphere(const glm::dvec3& center, double radius,
                                                           uint32_t layer_mask = 0xFFFFFFFF) const;

    // Line of sight check (uses voxel raycast)
    [[nodiscard]] bool has_line_of_sight(const glm::dvec3& from, const glm::dvec3& to) const;

    // ========================================================================
    // Collision Callbacks
    // ========================================================================

    void set_collision_callback(CollisionCallback callback);

    // ========================================================================
    // IWorldObserver Implementation
    // ========================================================================

    void on_chunk_loaded(const world::ChunkPos& pos, world::Chunk& chunk) override;
    void on_chunk_unloading(const world::ChunkPos& pos, const world::Chunk& chunk) override;
    void on_block_changed(const world::BlockChangeEvent& event) override;
    void on_origin_shifted(const world::WorldBlockPos& old_origin, const world::WorldBlockPos& new_origin) override;

    // ========================================================================
    // Configuration
    // ========================================================================

    [[nodiscard]] const PhysicsConfig& get_config() const;
    void set_gravity(const glm::dvec3& gravity);
    [[nodiscard]] glm::dvec3 get_gravity() const;

    // ========================================================================
    // Debug / Statistics
    // ========================================================================

    struct DebugStats {
        size_t active_colliders = 0;
        size_t active_rigid_bodies = 0;
        size_t chunk_colliders = 0;
        size_t total_collision_shapes = 0;
        double last_step_time_ms = 0.0;
    };

    [[nodiscard]] DebugStats get_stats() const;

    // ========================================================================
    // Structural Integrity
    // ========================================================================

    [[nodiscard]] StructuralIntegritySystem* get_structural_integrity();
    [[nodiscard]] const StructuralIntegritySystem* get_structural_integrity() const;

    // ========================================================================
    // Debris System
    // ========================================================================

    [[nodiscard]] DebrisSystem* get_debris_system();
    [[nodiscard]] const DebrisSystem* get_debris_system() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::physics
