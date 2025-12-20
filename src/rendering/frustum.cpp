// RealCraft Rendering System
// frustum.cpp - Frustum culling implementation

#include <realcraft/rendering/frustum.hpp>

namespace realcraft::rendering {

void Frustum::extract_from_matrix(const glm::mat4& vp) {
    // Gribb-Hartmann method for extracting frustum planes
    // Each plane is extracted from rows of the VP matrix

    // Left plane: row3 + row0
    planes_[Left].normal.x = vp[0][3] + vp[0][0];
    planes_[Left].normal.y = vp[1][3] + vp[1][0];
    planes_[Left].normal.z = vp[2][3] + vp[2][0];
    planes_[Left].distance = vp[3][3] + vp[3][0];

    // Right plane: row3 - row0
    planes_[Right].normal.x = vp[0][3] - vp[0][0];
    planes_[Right].normal.y = vp[1][3] - vp[1][0];
    planes_[Right].normal.z = vp[2][3] - vp[2][0];
    planes_[Right].distance = vp[3][3] - vp[3][0];

    // Bottom plane: row3 + row1
    planes_[Bottom].normal.x = vp[0][3] + vp[0][1];
    planes_[Bottom].normal.y = vp[1][3] + vp[1][1];
    planes_[Bottom].normal.z = vp[2][3] + vp[2][1];
    planes_[Bottom].distance = vp[3][3] + vp[3][1];

    // Top plane: row3 - row1
    planes_[Top].normal.x = vp[0][3] - vp[0][1];
    planes_[Top].normal.y = vp[1][3] - vp[1][1];
    planes_[Top].normal.z = vp[2][3] - vp[2][1];
    planes_[Top].distance = vp[3][3] - vp[3][1];

    // Near plane: row3 + row2
    planes_[Near].normal.x = vp[0][3] + vp[0][2];
    planes_[Near].normal.y = vp[1][3] + vp[1][2];
    planes_[Near].normal.z = vp[2][3] + vp[2][2];
    planes_[Near].distance = vp[3][3] + vp[3][2];

    // Far plane: row3 - row2
    planes_[Far].normal.x = vp[0][3] - vp[0][2];
    planes_[Far].normal.y = vp[1][3] - vp[1][2];
    planes_[Far].normal.z = vp[2][3] - vp[2][2];
    planes_[Far].distance = vp[3][3] - vp[3][2];

    // Normalize all planes
    for (auto& plane : planes_) {
        plane.normalize();
    }
}

bool Frustum::is_visible(const AABB& aabb) const {
    for (const auto& plane : planes_) {
        // Find the positive vertex (corner furthest in direction of normal)
        glm::vec3 p = aabb.min;
        if (plane.normal.x >= 0)
            p.x = aabb.max.x;
        if (plane.normal.y >= 0)
            p.y = aabb.max.y;
        if (plane.normal.z >= 0)
            p.z = aabb.max.z;

        // If positive vertex is outside, the entire AABB is outside
        if (plane.signed_distance(p) < 0) {
            return false;
        }
    }
    return true;
}

IntersectResult Frustum::test_aabb(const AABB& aabb) const {
    IntersectResult result = IntersectResult::Inside;

    for (const auto& plane : planes_) {
        // Find positive and negative vertices
        glm::vec3 p = aabb.min;
        glm::vec3 n = aabb.max;

        if (plane.normal.x >= 0) {
            p.x = aabb.max.x;
            n.x = aabb.min.x;
        }
        if (plane.normal.y >= 0) {
            p.y = aabb.max.y;
            n.y = aabb.min.y;
        }
        if (plane.normal.z >= 0) {
            p.z = aabb.max.z;
            n.z = aabb.min.z;
        }

        // Test positive vertex
        if (plane.signed_distance(p) < 0) {
            return IntersectResult::Outside;
        }

        // Test negative vertex
        if (plane.signed_distance(n) < 0) {
            result = IntersectResult::Intersects;
        }
    }

    return result;
}

bool Frustum::is_sphere_visible(const glm::vec3& center, float radius) const {
    for (const auto& plane : planes_) {
        if (plane.signed_distance(center) < -radius) {
            return false;
        }
    }
    return true;
}

bool Frustum::is_point_inside(const glm::vec3& point) const {
    for (const auto& plane : planes_) {
        if (plane.signed_distance(point) < 0) {
            return false;
        }
    }
    return true;
}

void ChunkCuller::update(const glm::mat4& view_projection) {
    frustum_.extract_from_matrix(view_projection);
    reset_stats();
}

bool ChunkCuller::is_chunk_visible(const glm::vec3& chunk_render_pos, int32_t chunk_size_x, int32_t chunk_size_y,
                                   int32_t chunk_size_z) const {
    AABB aabb = AABB::from_chunk(chunk_render_pos, chunk_size_x, chunk_size_y, chunk_size_z);

    bool visible = frustum_.is_visible(aabb);

    if (visible) {
        ++visible_count_;
    } else {
        ++culled_count_;
    }

    return visible;
}

}  // namespace realcraft::rendering
