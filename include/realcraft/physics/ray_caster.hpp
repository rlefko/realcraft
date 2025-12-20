// RealCraft Physics Engine
// ray_caster.hpp - DDA-based voxel ray casting

#pragma once

#include "types.hpp"

#include <memory>
#include <optional>
#include <realcraft/world/world_manager.hpp>
#include <vector>

namespace realcraft::physics {

// ============================================================================
// Voxel Ray Caster
// ============================================================================

// DDA (Digital Differential Analyzer) based voxel ray traversal
// Optimized for voxel grids, more efficient than Bullet for block targeting
class VoxelRayCaster {
public:
    VoxelRayCaster();
    ~VoxelRayCaster();

    VoxelRayCaster(const VoxelRayCaster&) = delete;
    VoxelRayCaster& operator=(const VoxelRayCaster&) = delete;

    void set_world_manager(world::WorldManager* world_manager);

    // Cast ray through voxel world
    // Returns first solid block hit
    [[nodiscard]] std::optional<RayHit> cast(const Ray& ray, double max_distance = 100.0,
                                             bool hit_liquids = false) const;

    // Cast ray and return all blocks hit (for line-of-sight checks, etc.)
    [[nodiscard]] std::vector<RayHit> cast_all(const Ray& ray, double max_distance = 100.0, size_t max_hits = 64,
                                               bool hit_liquids = false) const;

    // Fast visibility check (cheaper than full cast)
    [[nodiscard]] bool has_line_of_sight(const glm::dvec3& from, const glm::dvec3& to) const;

    // Cast with custom filter function
    using BlockFilter = std::function<bool(world::BlockId, const world::WorldBlockPos&)>;
    [[nodiscard]] std::optional<RayHit> cast_filtered(const Ray& ray, double max_distance,
                                                      const BlockFilter& filter) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::physics
