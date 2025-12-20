// RealCraft Physics Engine
// structural_integrity.hpp - Structural integrity system for block collapse

#pragma once

#include "rigid_body.hpp"
#include "support_graph.hpp"
#include "types.hpp"

#include <functional>
#include <memory>
#include <queue>
#include <realcraft/world/world_manager.hpp>
#include <vector>

namespace realcraft::physics {

// Forward declarations
class PhysicsWorld;

// ============================================================================
// Structural Integrity Configuration
// ============================================================================

struct StructuralIntegrityConfig {
    // Search limits
    uint32_t max_search_blocks = 10000;  // Maximum blocks to search in BFS
    uint32_t max_cluster_size = 64;      // Maximum blocks in a single debris cluster
    uint32_t max_debris_per_frame = 8;   // Maximum debris to spawn per frame

    // Cascade settings
    bool enable_cascade_collapse = true;
    uint32_t max_cascade_depth = 3;
    double cascade_delay_seconds = 0.1;

    // Performance
    bool defer_checks_to_update = false;  // Batch checks in update() call
    double max_check_time_per_frame_ms = 2.0;

    // Ground level
    int32_t ground_level = 0;
};

// ============================================================================
// Collapse Result
// ============================================================================

struct CollapseResult {
    bool collapse_occurred = false;
    std::vector<world::WorldBlockPos> collapsed_blocks;
    std::vector<RigidBodyHandle> spawned_debris;
    uint32_t cascade_depth = 0;
};

// ============================================================================
// Collapse Event (for callbacks)
// ============================================================================

struct CollapseEvent {
    world::WorldBlockPos trigger_position;
    std::vector<BlockCluster> falling_clusters;
    double timestamp = 0.0;
};

using CollapseCallback = std::function<void(const CollapseEvent&)>;

// ============================================================================
// Structural Integrity System
// ============================================================================

class StructuralIntegritySystem : public world::IWorldObserver {
public:
    StructuralIntegritySystem();
    ~StructuralIntegritySystem() override;

    // Non-copyable, non-movable
    StructuralIntegritySystem(const StructuralIntegritySystem&) = delete;
    StructuralIntegritySystem& operator=(const StructuralIntegritySystem&) = delete;
    StructuralIntegritySystem(StructuralIntegritySystem&&) = delete;
    StructuralIntegritySystem& operator=(StructuralIntegritySystem&&) = delete;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    bool initialize(PhysicsWorld* physics_world, world::WorldManager* world_manager,
                    const StructuralIntegrityConfig& config = {});
    void shutdown();
    [[nodiscard]] bool is_initialized() const;

    // ========================================================================
    // Update (for deferred/batched checks)
    // ========================================================================

    void update(double delta_time);

    // ========================================================================
    // IWorldObserver Implementation
    // ========================================================================

    void on_chunk_loaded(const world::ChunkPos& pos, world::Chunk& chunk) override;
    void on_chunk_unloading(const world::ChunkPos& pos, const world::Chunk& chunk) override;
    void on_block_changed(const world::BlockChangeEvent& event) override;
    void on_origin_shifted(const world::WorldBlockPos& old_origin, const world::WorldBlockPos& new_origin) override;

    // ========================================================================
    // Manual Integrity Checking
    // ========================================================================

    // Check structural integrity starting from a position
    CollapseResult check_structural_integrity(const world::WorldBlockPos& pos);

    // ========================================================================
    // Query Functions
    // ========================================================================

    // Check if a block at the given position is supported
    [[nodiscard]] bool is_block_supported(const world::WorldBlockPos& pos) const;

    // Get the support distance of a block to ground
    [[nodiscard]] float get_support_distance(const world::WorldBlockPos& pos) const;

    // ========================================================================
    // Configuration
    // ========================================================================

    [[nodiscard]] const StructuralIntegrityConfig& get_config() const;
    void set_config(const StructuralIntegrityConfig& config);

    // Toggle structural integrity (for Creative mode)
    void set_enabled(bool enabled);
    [[nodiscard]] bool is_enabled() const;

    // ========================================================================
    // Callbacks
    // ========================================================================

    void set_collapse_callback(CollapseCallback callback);

    // ========================================================================
    // Statistics
    // ========================================================================

    struct Stats {
        size_t tracked_blocks = 0;
        size_t pending_checks = 0;
        size_t total_collapses = 0;
        size_t total_blocks_collapsed = 0;
        size_t total_debris_spawned = 0;
        double last_check_time_ms = 0.0;
    };

    [[nodiscard]] Stats get_stats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::physics
