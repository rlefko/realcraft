// RealCraft Rendering System
// frustum.hpp - View frustum culling

#pragma once

#include <glm/glm.hpp>

#include <array>
#include <cstdint>

namespace realcraft::rendering {

// Axis-Aligned Bounding Box
struct AABB {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};

    [[nodiscard]] glm::vec3 get_center() const { return (min + max) * 0.5f; }
    [[nodiscard]] glm::vec3 get_extents() const { return (max - min) * 0.5f; }
    [[nodiscard]] glm::vec3 get_size() const { return max - min; }

    // Create AABB for a chunk at the given render-space origin
    static AABB from_chunk(const glm::vec3& origin, int32_t size_x, int32_t size_y, int32_t size_z) {
        AABB aabb;
        aabb.min = origin;
        aabb.max =
            origin + glm::vec3(static_cast<float>(size_x), static_cast<float>(size_y), static_cast<float>(size_z));
        return aabb;
    }
};

// Frustum plane (ax + by + cz + d = 0)
struct Plane {
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    float distance = 0.0f;

    // Signed distance from point to plane
    // Positive = in front (inside frustum side)
    // Negative = behind (outside frustum side)
    [[nodiscard]] float signed_distance(const glm::vec3& point) const { return glm::dot(normal, point) + distance; }

    // Normalize the plane
    void normalize() {
        float length = glm::length(normal);
        if (length > 0.0001f) {
            normal /= length;
            distance /= length;
        }
    }
};

// Intersection result for more precise culling
enum class IntersectResult { Outside, Inside, Intersects };

// View frustum for culling
class Frustum {
public:
    Frustum() = default;

    // Extract frustum planes from view-projection matrix
    // Uses Gribb-Hartmann method
    void extract_from_matrix(const glm::mat4& view_projection);

    // Test if AABB is visible (quick test)
    [[nodiscard]] bool is_visible(const AABB& aabb) const;

    // More detailed intersection test
    [[nodiscard]] IntersectResult test_aabb(const AABB& aabb) const;

    // Test if sphere is visible
    [[nodiscard]] bool is_sphere_visible(const glm::vec3& center, float radius) const;

    // Test if point is inside frustum
    [[nodiscard]] bool is_point_inside(const glm::vec3& point) const;

    // Plane indices
    enum PlaneIndex { Left = 0, Right = 1, Bottom = 2, Top = 3, Near = 4, Far = 5 };

    // Get planes for debugging
    [[nodiscard]] const std::array<Plane, 6>& get_planes() const { return planes_; }

private:
    std::array<Plane, 6> planes_;
};

// Chunk visibility tracker with statistics
class ChunkCuller {
public:
    ChunkCuller() = default;

    // Update frustum for new frame
    void update(const glm::mat4& view_projection);

    // Test chunk visibility
    // chunk_render_pos: chunk origin in render coordinates (after origin shift)
    [[nodiscard]] bool is_chunk_visible(const glm::vec3& chunk_render_pos, int32_t chunk_size_x, int32_t chunk_size_y,
                                        int32_t chunk_size_z) const;

    // Statistics (reset each frame)
    [[nodiscard]] uint32_t get_visible_count() const { return visible_count_; }
    [[nodiscard]] uint32_t get_culled_count() const { return culled_count_; }
    void reset_stats() {
        visible_count_ = 0;
        culled_count_ = 0;
    }

private:
    Frustum frustum_;
    mutable uint32_t visible_count_ = 0;
    mutable uint32_t culled_count_ = 0;
};

}  // namespace realcraft::rendering
