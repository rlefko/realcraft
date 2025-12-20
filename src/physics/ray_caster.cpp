// RealCraft Physics Engine
// ray_caster.cpp - DDA-based voxel ray casting implementation

#include <cmath>
#include <limits>
#include <realcraft/physics/ray_caster.hpp>
#include <realcraft/world/block.hpp>

namespace realcraft::physics {

// ============================================================================
// Implementation
// ============================================================================

struct VoxelRayCaster::Impl {
    world::WorldManager* world_manager = nullptr;

    // Amanatides-Woo DDA algorithm for voxel traversal
    std::optional<RayHit> cast_dda(const Ray& ray, double max_distance, bool hit_liquids,
                                   const BlockFilter* filter) const {
        if (!world_manager) {
            return std::nullopt;
        }

        // Current voxel position
        int64_t x = static_cast<int64_t>(std::floor(ray.origin.x));
        int64_t y = static_cast<int64_t>(std::floor(ray.origin.y));
        int64_t z = static_cast<int64_t>(std::floor(ray.origin.z));

        // Step direction (+1 or -1)
        int step_x = (ray.direction.x >= 0) ? 1 : -1;
        int step_y = (ray.direction.y >= 0) ? 1 : -1;
        int step_z = (ray.direction.z >= 0) ? 1 : -1;

        // Calculate tMax and tDelta for each axis
        // tMax: distance to next voxel boundary in each direction
        // tDelta: distance between voxel boundaries in each direction
        double t_max_x, t_max_y, t_max_z;
        double t_delta_x, t_delta_y, t_delta_z;

        constexpr double EPSILON = 1e-10;

        if (std::abs(ray.direction.x) < EPSILON) {
            t_max_x = std::numeric_limits<double>::max();
            t_delta_x = std::numeric_limits<double>::max();
        } else {
            double next_x = (step_x > 0) ? static_cast<double>(x + 1) : static_cast<double>(x);
            t_max_x = (next_x - ray.origin.x) / ray.direction.x;
            t_delta_x = static_cast<double>(step_x) / ray.direction.x;
        }

        if (std::abs(ray.direction.y) < EPSILON) {
            t_max_y = std::numeric_limits<double>::max();
            t_delta_y = std::numeric_limits<double>::max();
        } else {
            double next_y = (step_y > 0) ? static_cast<double>(y + 1) : static_cast<double>(y);
            t_max_y = (next_y - ray.origin.y) / ray.direction.y;
            t_delta_y = static_cast<double>(step_y) / ray.direction.y;
        }

        if (std::abs(ray.direction.z) < EPSILON) {
            t_max_z = std::numeric_limits<double>::max();
            t_delta_z = std::numeric_limits<double>::max();
        } else {
            double next_z = (step_z > 0) ? static_cast<double>(z + 1) : static_cast<double>(z);
            t_max_z = (next_z - ray.origin.z) / ray.direction.z;
            t_delta_z = static_cast<double>(step_z) / ray.direction.z;
        }

        // Track which face we entered from
        world::Direction last_face = world::Direction::PosY;  // Default
        double t = 0.0;

        // Maximum iterations to prevent infinite loops
        size_t max_iterations = static_cast<size_t>(max_distance * 2.0) + 100;

        for (size_t i = 0; i < max_iterations && t < max_distance; ++i) {
            // Check current voxel
            world::WorldBlockPos block_pos(x, y, z);
            world::BlockId block_id = world_manager->get_block(block_pos);

            // Check if this block should stop the ray
            if (block_id != world::BLOCK_AIR && block_id != world::BLOCK_INVALID) {
                const world::BlockType* block_type = world::BlockRegistry::instance().get(block_id);

                if (block_type) {
                    bool should_hit = false;

                    if (filter) {
                        should_hit = (*filter)(block_id, block_pos);
                    } else {
                        // Default: hit solid blocks with collision
                        if (block_type->has_collision()) {
                            should_hit = true;
                        }
                        // Optionally hit liquids
                        if (hit_liquids && block_type->is_liquid()) {
                            should_hit = true;
                        }
                    }

                    if (should_hit) {
                        RayHit hit;
                        hit.distance = t;
                        hit.position = ray.point_at(t);
                        hit.block_position = block_pos;
                        hit.block_id = block_id;
                        hit.block_face = last_face;

                        // Calculate normal from face
                        switch (last_face) {
                            case world::Direction::NegX:
                                hit.normal = glm::dvec3(1.0, 0.0, 0.0);
                                break;
                            case world::Direction::PosX:
                                hit.normal = glm::dvec3(-1.0, 0.0, 0.0);
                                break;
                            case world::Direction::NegY:
                                hit.normal = glm::dvec3(0.0, 1.0, 0.0);
                                break;
                            case world::Direction::PosY:
                                hit.normal = glm::dvec3(0.0, -1.0, 0.0);
                                break;
                            case world::Direction::NegZ:
                                hit.normal = glm::dvec3(0.0, 0.0, 1.0);
                                break;
                            case world::Direction::PosZ:
                                hit.normal = glm::dvec3(0.0, 0.0, -1.0);
                                break;
                            default:
                                hit.normal = glm::dvec3(0.0, 1.0, 0.0);
                                break;
                        }

                        return hit;
                    }
                }
            }

            // Advance to next voxel
            if (t_max_x < t_max_y && t_max_x < t_max_z) {
                t = t_max_x;
                x += step_x;
                t_max_x += t_delta_x;
                last_face = (step_x > 0) ? world::Direction::NegX : world::Direction::PosX;
            } else if (t_max_y < t_max_z) {
                t = t_max_y;
                y += step_y;
                t_max_y += t_delta_y;
                last_face = (step_y > 0) ? world::Direction::NegY : world::Direction::PosY;
            } else {
                t = t_max_z;
                z += step_z;
                t_max_z += t_delta_z;
                last_face = (step_z > 0) ? world::Direction::NegZ : world::Direction::PosZ;
            }
        }

        return std::nullopt;
    }

    std::vector<RayHit> cast_all_dda(const Ray& ray, double max_distance, size_t max_hits, bool hit_liquids) const {
        std::vector<RayHit> hits;

        if (!world_manager) {
            return hits;
        }

        // Current voxel position
        int64_t x = static_cast<int64_t>(std::floor(ray.origin.x));
        int64_t y = static_cast<int64_t>(std::floor(ray.origin.y));
        int64_t z = static_cast<int64_t>(std::floor(ray.origin.z));

        // Step direction
        int step_x = (ray.direction.x >= 0) ? 1 : -1;
        int step_y = (ray.direction.y >= 0) ? 1 : -1;
        int step_z = (ray.direction.z >= 0) ? 1 : -1;

        // Calculate tMax and tDelta
        double t_max_x, t_max_y, t_max_z;
        double t_delta_x, t_delta_y, t_delta_z;

        constexpr double EPSILON = 1e-10;

        if (std::abs(ray.direction.x) < EPSILON) {
            t_max_x = std::numeric_limits<double>::max();
            t_delta_x = std::numeric_limits<double>::max();
        } else {
            double next_x = (step_x > 0) ? static_cast<double>(x + 1) : static_cast<double>(x);
            t_max_x = (next_x - ray.origin.x) / ray.direction.x;
            t_delta_x = static_cast<double>(step_x) / ray.direction.x;
        }

        if (std::abs(ray.direction.y) < EPSILON) {
            t_max_y = std::numeric_limits<double>::max();
            t_delta_y = std::numeric_limits<double>::max();
        } else {
            double next_y = (step_y > 0) ? static_cast<double>(y + 1) : static_cast<double>(y);
            t_max_y = (next_y - ray.origin.y) / ray.direction.y;
            t_delta_y = static_cast<double>(step_y) / ray.direction.y;
        }

        if (std::abs(ray.direction.z) < EPSILON) {
            t_max_z = std::numeric_limits<double>::max();
            t_delta_z = std::numeric_limits<double>::max();
        } else {
            double next_z = (step_z > 0) ? static_cast<double>(z + 1) : static_cast<double>(z);
            t_max_z = (next_z - ray.origin.z) / ray.direction.z;
            t_delta_z = static_cast<double>(step_z) / ray.direction.z;
        }

        world::Direction last_face = world::Direction::PosY;
        double t = 0.0;

        size_t max_iterations = static_cast<size_t>(max_distance * 2.0) + 100;

        for (size_t i = 0; i < max_iterations && t < max_distance && hits.size() < max_hits; ++i) {
            world::WorldBlockPos block_pos(x, y, z);
            world::BlockId block_id = world_manager->get_block(block_pos);

            if (block_id != world::BLOCK_AIR && block_id != world::BLOCK_INVALID) {
                const world::BlockType* block_type = world::BlockRegistry::instance().get(block_id);

                if (block_type) {
                    bool should_hit = block_type->has_collision();
                    if (hit_liquids && block_type->is_liquid()) {
                        should_hit = true;
                    }

                    if (should_hit) {
                        RayHit hit;
                        hit.distance = t;
                        hit.position = ray.point_at(t);
                        hit.block_position = block_pos;
                        hit.block_id = block_id;
                        hit.block_face = last_face;

                        switch (last_face) {
                            case world::Direction::NegX:
                                hit.normal = glm::dvec3(1.0, 0.0, 0.0);
                                break;
                            case world::Direction::PosX:
                                hit.normal = glm::dvec3(-1.0, 0.0, 0.0);
                                break;
                            case world::Direction::NegY:
                                hit.normal = glm::dvec3(0.0, 1.0, 0.0);
                                break;
                            case world::Direction::PosY:
                                hit.normal = glm::dvec3(0.0, -1.0, 0.0);
                                break;
                            case world::Direction::NegZ:
                                hit.normal = glm::dvec3(0.0, 0.0, 1.0);
                                break;
                            case world::Direction::PosZ:
                                hit.normal = glm::dvec3(0.0, 0.0, -1.0);
                                break;
                            default:
                                hit.normal = glm::dvec3(0.0, 1.0, 0.0);
                                break;
                        }

                        hits.push_back(hit);
                    }
                }
            }

            // Advance to next voxel
            if (t_max_x < t_max_y && t_max_x < t_max_z) {
                t = t_max_x;
                x += step_x;
                t_max_x += t_delta_x;
                last_face = (step_x > 0) ? world::Direction::NegX : world::Direction::PosX;
            } else if (t_max_y < t_max_z) {
                t = t_max_y;
                y += step_y;
                t_max_y += t_delta_y;
                last_face = (step_y > 0) ? world::Direction::NegY : world::Direction::PosY;
            } else {
                t = t_max_z;
                z += step_z;
                t_max_z += t_delta_z;
                last_face = (step_z > 0) ? world::Direction::NegZ : world::Direction::PosZ;
            }
        }

        return hits;
    }
};

// ============================================================================
// VoxelRayCaster Public API
// ============================================================================

VoxelRayCaster::VoxelRayCaster() : impl_(std::make_unique<Impl>()) {}

VoxelRayCaster::~VoxelRayCaster() = default;

void VoxelRayCaster::set_world_manager(world::WorldManager* world_manager) {
    impl_->world_manager = world_manager;
}

std::optional<RayHit> VoxelRayCaster::cast(const Ray& ray, double max_distance, bool hit_liquids) const {
    return impl_->cast_dda(ray, max_distance, hit_liquids, nullptr);
}

std::vector<RayHit> VoxelRayCaster::cast_all(const Ray& ray, double max_distance, size_t max_hits,
                                             bool hit_liquids) const {
    return impl_->cast_all_dda(ray, max_distance, max_hits, hit_liquids);
}

bool VoxelRayCaster::has_line_of_sight(const glm::dvec3& from, const glm::dvec3& to) const {
    glm::dvec3 direction = to - from;
    double distance = glm::length(direction);

    if (distance < 1e-10) {
        return true;  // Same point
    }

    Ray ray(from, direction);
    auto hit = impl_->cast_dda(ray, distance, false, nullptr);

    return !hit.has_value();
}

std::optional<RayHit> VoxelRayCaster::cast_filtered(const Ray& ray, double max_distance,
                                                    const BlockFilter& filter) const {
    return impl_->cast_dda(ray, max_distance, false, &filter);
}

}  // namespace realcraft::physics
