// RealCraft Physics Engine
// types.hpp - Core physics types: AABB, Ray, RayHit, ColliderHandle

#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <optional>
#include <realcraft/world/types.hpp>

namespace realcraft::physics {

// ============================================================================
// Collider Handle
// ============================================================================

using ColliderHandle = uint32_t;
inline constexpr ColliderHandle INVALID_COLLIDER = 0;

// ============================================================================
// Collision Layers
// ============================================================================

enum class CollisionLayer : uint32_t {
    None = 0,
    Default = 1 << 0,
    Static = 1 << 1,      // World geometry (chunks)
    Player = 1 << 2,      // Player collider
    Entity = 1 << 3,      // Creatures, NPCs
    Projectile = 1 << 4,  // Arrows, thrown items
    Debris = 1 << 5,      // Falling debris
    Trigger = 1 << 6,     // Non-blocking detection zones
    All = 0xFFFFFFFF
};

inline CollisionLayer operator|(CollisionLayer a, CollisionLayer b) {
    return static_cast<CollisionLayer>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline CollisionLayer operator&(CollisionLayer a, CollisionLayer b) {
    return static_cast<CollisionLayer>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline CollisionLayer& operator|=(CollisionLayer& a, CollisionLayer b) {
    a = a | b;
    return a;
}

[[nodiscard]] inline bool has_layer(uint32_t mask, CollisionLayer layer) {
    return (mask & static_cast<uint32_t>(layer)) != 0;
}

// ============================================================================
// Collider Type
// ============================================================================

enum class ColliderType : uint8_t { AABB, Capsule, ConvexHull };

// ============================================================================
// Axis-Aligned Bounding Box
// ============================================================================

struct AABB {
    glm::dvec3 min{0.0};
    glm::dvec3 max{0.0};

    AABB() = default;
    AABB(const glm::dvec3& min_point, const glm::dvec3& max_point) : min(min_point), max(max_point) {}

    [[nodiscard]] glm::dvec3 center() const { return (min + max) * 0.5; }

    [[nodiscard]] glm::dvec3 extents() const { return max - min; }

    [[nodiscard]] glm::dvec3 half_extents() const { return extents() * 0.5; }

    [[nodiscard]] bool contains(const glm::dvec3& point) const {
        return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y && point.z >= min.z &&
               point.z <= max.z;
    }

    [[nodiscard]] bool intersects(const AABB& other) const {
        return min.x <= other.max.x && max.x >= other.min.x && min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }

    [[nodiscard]] AABB expanded(double margin) const {
        return AABB(min - glm::dvec3(margin), max + glm::dvec3(margin));
    }

    [[nodiscard]] static AABB from_center_extents(const glm::dvec3& center, const glm::dvec3& half_ext) {
        return AABB(center - half_ext, center + half_ext);
    }

    [[nodiscard]] static AABB from_block_pos(const world::WorldBlockPos& pos) {
        return AABB(glm::dvec3(static_cast<double>(pos.x), static_cast<double>(pos.y), static_cast<double>(pos.z)),
                    glm::dvec3(static_cast<double>(pos.x) + 1.0, static_cast<double>(pos.y) + 1.0,
                               static_cast<double>(pos.z) + 1.0));
    }
};

// ============================================================================
// Ray
// ============================================================================

struct Ray {
    glm::dvec3 origin{0.0};
    glm::dvec3 direction{0.0, 0.0, -1.0};  // Normalized

    Ray() = default;
    Ray(const glm::dvec3& orig, const glm::dvec3& dir) : origin(orig), direction(glm::normalize(dir)) {}

    [[nodiscard]] glm::dvec3 point_at(double t) const { return origin + direction * t; }
};

// ============================================================================
// Ray Hit Result
// ============================================================================

struct RayHit {
    glm::dvec3 position{0.0};          // World position of hit
    glm::dvec3 normal{0.0, 1.0, 0.0};  // Surface normal at hit
    double distance = 0.0;             // Distance from ray origin

    // Block info (if hit voxel)
    std::optional<world::WorldBlockPos> block_position;
    std::optional<world::BlockId> block_id;
    std::optional<world::Direction> block_face;

    // Collider info (if hit collider)
    ColliderHandle collider = INVALID_COLLIDER;

    [[nodiscard]] bool hit_block() const { return block_position.has_value(); }

    [[nodiscard]] bool hit_collider() const { return collider != INVALID_COLLIDER; }
};

// ============================================================================
// Ray-AABB Intersection
// ============================================================================

// Returns distance to intersection, or negative if no hit
[[nodiscard]] inline double ray_aabb_intersection(const Ray& ray, const AABB& aabb) {
    double tmin = 0.0;
    double tmax = std::numeric_limits<double>::max();

    for (int i = 0; i < 3; ++i) {
        if (std::abs(ray.direction[i]) < 1e-10) {
            // Ray is parallel to slab
            if (ray.origin[i] < aabb.min[i] || ray.origin[i] > aabb.max[i]) {
                return -1.0;
            }
        } else {
            double inv_d = 1.0 / ray.direction[i];
            double t1 = (aabb.min[i] - ray.origin[i]) * inv_d;
            double t2 = (aabb.max[i] - ray.origin[i]) * inv_d;

            if (t1 > t2) {
                std::swap(t1, t2);
            }

            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);

            if (tmin > tmax) {
                return -1.0;
            }
        }
    }

    return tmin >= 0.0 ? tmin : tmax;
}

}  // namespace realcraft::physics
