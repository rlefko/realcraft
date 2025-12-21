// RealCraft Physics Engine
// convex_hull_generator.cpp - Generate collision shapes from voxel clusters

#include <algorithm>
#include <cmath>
#include <limits>
#include <realcraft/physics/convex_hull_generator.hpp>
#include <set>

namespace realcraft::physics {

// ============================================================================
// Main Generation Functions
// ============================================================================

std::vector<glm::vec3> ConvexHullGenerator::generate_hull_vertices(const std::vector<world::WorldBlockPos>& positions,
                                                                   double simplification_tolerance) {
    if (positions.empty()) {
        return {};
    }

    // Extract surface vertices
    auto surface_vertices = extract_surface_vertices(positions);

    if (surface_vertices.empty()) {
        // Fallback: use block centers
        for (const auto& pos : positions) {
            surface_vertices.emplace_back(static_cast<double>(pos.x) + 0.5, static_cast<double>(pos.y) + 0.5,
                                          static_cast<double>(pos.z) + 0.5);
        }
    }

    // Compute convex hull
    auto hull = compute_quickhull(surface_vertices);

    // Simplify if needed
    size_t max_vertices = 16;  // Reasonable limit for physics
    if (hull.size() > max_vertices) {
        hull =
            std::vector<glm::dvec3>(simplify_hull(hull, max_vertices).begin(), simplify_hull(hull, max_vertices).end());
    }

    // Convert to float for Bullet
    std::vector<glm::vec3> result;
    result.reserve(hull.size());

    // Calculate center to make vertices relative
    glm::dvec3 center = compute_center(positions);

    for (const auto& v : hull) {
        result.emplace_back(static_cast<float>(v.x - center.x), static_cast<float>(v.y - center.y),
                            static_cast<float>(v.z - center.z));
    }

    return result;
}

glm::dvec3 ConvexHullGenerator::compute_half_extents(const std::vector<world::WorldBlockPos>& positions) {
    if (positions.empty()) {
        return glm::dvec3(0.5);
    }

    glm::dvec3 min_pos(std::numeric_limits<double>::max());
    glm::dvec3 max_pos(std::numeric_limits<double>::lowest());

    for (const auto& pos : positions) {
        min_pos = glm::min(min_pos, glm::dvec3(pos.x, pos.y, pos.z));
        max_pos = glm::max(max_pos, glm::dvec3(pos.x + 1.0, pos.y + 1.0, pos.z + 1.0));
    }

    return (max_pos - min_pos) * 0.5;
}

glm::dvec3 ConvexHullGenerator::compute_center(const std::vector<world::WorldBlockPos>& positions) {
    if (positions.empty()) {
        return glm::dvec3(0.0);
    }

    glm::dvec3 min_pos(std::numeric_limits<double>::max());
    glm::dvec3 max_pos(std::numeric_limits<double>::lowest());

    for (const auto& pos : positions) {
        min_pos = glm::min(min_pos, glm::dvec3(pos.x, pos.y, pos.z));
        max_pos = glm::max(max_pos, glm::dvec3(pos.x + 1.0, pos.y + 1.0, pos.z + 1.0));
    }

    return (min_pos + max_pos) * 0.5;
}

// ============================================================================
// Decision Helpers
// ============================================================================

bool ConvexHullGenerator::should_use_convex_hull(const std::vector<world::WorldBlockPos>& positions,
                                                 size_t max_vertices) {
    if (positions.size() <= 4) {
        // Small clusters are better with AABB
        return false;
    }

    // Check if shape is roughly cubic (AABB would be efficient)
    auto half_extents = compute_half_extents(positions);
    double max_extent = std::max({half_extents.x, half_extents.y, half_extents.z});
    double min_extent = std::min({half_extents.x, half_extents.y, half_extents.z});

    if (min_extent > 0.0 && max_extent / min_extent < 2.0) {
        // Roughly cubic shape - AABB is fine
        return false;
    }

    // Check fill ratio - how much of the bounding box is filled
    double bbox_volume = half_extents.x * half_extents.y * half_extents.z * 8.0;
    double block_volume = static_cast<double>(positions.size());

    if (bbox_volume > 0.0 && block_volume / bbox_volume > 0.7) {
        // High fill ratio - AABB is fine
        return false;
    }

    // Estimate vertex count
    size_t estimated_vertices = estimate_hull_vertex_count(positions);

    return estimated_vertices <= max_vertices;
}

size_t ConvexHullGenerator::estimate_hull_vertex_count(const std::vector<world::WorldBlockPos>& positions) {
    // Rough estimate: count exposed corner vertices
    std::unordered_set<world::WorldBlockPos> pos_set(positions.begin(), positions.end());

    size_t exposed_faces = 0;
    for (const auto& pos : positions) {
        for (int face = 0; face < 6; ++face) {
            if (is_face_exposed(pos, face, pos_set)) {
                exposed_faces++;
            }
        }
    }

    // Each face has 4 vertices, but they're shared
    // Rough estimate: exposed_faces * 4 / 4 = exposed_faces unique vertices
    // Then convex hull reduces this significantly
    return std::min(exposed_faces, positions.size() * 2);
}

// ============================================================================
// Utility Functions
// ============================================================================

std::vector<glm::dvec3>
ConvexHullGenerator::extract_surface_vertices(const std::vector<world::WorldBlockPos>& positions) {
    std::unordered_set<world::WorldBlockPos> pos_set(positions.begin(), positions.end());

    // Use a set to avoid duplicate vertices
    std::set<std::tuple<double, double, double>> unique_vertices;

    for (const auto& pos : positions) {
        for (int face = 0; face < 6; ++face) {
            if (is_face_exposed(pos, face, pos_set)) {
                auto face_verts = get_face_vertices(pos, face);
                for (const auto& v : face_verts) {
                    unique_vertices.insert({v.x, v.y, v.z});
                }
            }
        }
    }

    std::vector<glm::dvec3> result;
    result.reserve(unique_vertices.size());
    for (const auto& [x, y, z] : unique_vertices) {
        result.emplace_back(x, y, z);
    }

    return result;
}

std::vector<glm::vec3> ConvexHullGenerator::simplify_hull(const std::vector<glm::dvec3>& vertices,
                                                          size_t target_vertices) {
    if (vertices.size() <= target_vertices) {
        std::vector<glm::vec3> result;
        result.reserve(vertices.size());
        for (const auto& v : vertices) {
            result.emplace_back(static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z));
        }
        return result;
    }

    // Simple decimation: remove vertices that are closest to their neighbors
    std::vector<glm::dvec3> working = vertices;

    while (working.size() > target_vertices && working.size() > 3) {
        // Find vertex that contributes least to the hull shape
        size_t min_index = 0;
        double min_importance = std::numeric_limits<double>::max();

        for (size_t i = 0; i < working.size(); ++i) {
            // Calculate importance as distance from the line connecting neighbors
            size_t prev = (i + working.size() - 1) % working.size();
            size_t next = (i + 1) % working.size();

            glm::dvec3 line = glm::normalize(working[next] - working[prev]);
            glm::dvec3 to_point = working[i] - working[prev];
            double projection = glm::dot(to_point, line);
            glm::dvec3 closest = working[prev] + line * projection;
            double distance = glm::length(working[i] - closest);

            if (distance < min_importance) {
                min_importance = distance;
                min_index = i;
            }
        }

        // Remove least important vertex
        working.erase(working.begin() + static_cast<ptrdiff_t>(min_index));
    }

    std::vector<glm::vec3> result;
    result.reserve(working.size());
    for (const auto& v : working) {
        result.emplace_back(static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z));
    }

    return result;
}

bool ConvexHullGenerator::point_in_hull(const glm::dvec3& point, const std::vector<glm::dvec3>& hull_vertices) {
    if (hull_vertices.size() < 4) {
        return false;
    }

    // Simple check: point should be "inside" all face planes
    // This is a simplified version - for complex hulls, use proper algorithm
    glm::dvec3 center(0.0);
    for (const auto& v : hull_vertices) {
        center += v;
    }
    center /= static_cast<double>(hull_vertices.size());

    // Check if point is between center and all vertices (rough approximation)
    for (const auto& v : hull_vertices) {
        glm::dvec3 to_vertex = v - center;
        glm::dvec3 to_point = point - center;

        if (glm::dot(to_point, to_vertex) > glm::dot(to_vertex, to_vertex)) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// Private Helpers
// ============================================================================

bool ConvexHullGenerator::is_face_exposed(const world::WorldBlockPos& pos, int face_index,
                                          const std::unordered_set<world::WorldBlockPos>& positions) {
    // Face indices: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    static const std::array<world::WorldBlockPos, 6> offsets = {
        {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}};

    world::WorldBlockPos neighbor{pos.x + offsets[face_index].x, pos.y + offsets[face_index].y,
                                  pos.z + offsets[face_index].z};

    return positions.find(neighbor) == positions.end();
}

std::array<glm::dvec3, 4> ConvexHullGenerator::get_face_vertices(const world::WorldBlockPos& pos, int face_index) {
    double x = static_cast<double>(pos.x);
    double y = static_cast<double>(pos.y);
    double z = static_cast<double>(pos.z);

    // Return 4 corners of the face
    switch (face_index) {
        case 0:  // +X
            return {{{x + 1, y, z}, {x + 1, y + 1, z}, {x + 1, y + 1, z + 1}, {x + 1, y, z + 1}}};
        case 1:  // -X
            return {{{x, y, z}, {x, y + 1, z}, {x, y + 1, z + 1}, {x, y, z + 1}}};
        case 2:  // +Y
            return {{{x, y + 1, z}, {x + 1, y + 1, z}, {x + 1, y + 1, z + 1}, {x, y + 1, z + 1}}};
        case 3:  // -Y
            return {{{x, y, z}, {x + 1, y, z}, {x + 1, y, z + 1}, {x, y, z + 1}}};
        case 4:  // +Z
            return {{{x, y, z + 1}, {x + 1, y, z + 1}, {x + 1, y + 1, z + 1}, {x, y + 1, z + 1}}};
        case 5:  // -Z
            return {{{x, y, z}, {x + 1, y, z}, {x + 1, y + 1, z}, {x, y + 1, z}}};
        default:
            return {{{x, y, z}, {x + 1, y, z}, {x + 1, y + 1, z}, {x, y + 1, z}}};
    }
}

std::vector<glm::dvec3> ConvexHullGenerator::compute_quickhull(const std::vector<glm::dvec3>& points) {
    if (points.size() <= 4) {
        return points;
    }

    // Find extreme points
    size_t min_x = 0, max_x = 0, min_y = 0, max_y = 0, min_z = 0, max_z = 0;

    for (size_t i = 1; i < points.size(); ++i) {
        if (points[i].x < points[min_x].x)
            min_x = i;
        if (points[i].x > points[max_x].x)
            max_x = i;
        if (points[i].y < points[min_y].y)
            min_y = i;
        if (points[i].y > points[max_y].y)
            max_y = i;
        if (points[i].z < points[min_z].z)
            min_z = i;
        if (points[i].z > points[max_z].z)
            max_z = i;
    }

    // Collect unique extreme points
    std::set<size_t> extreme_indices = {min_x, max_x, min_y, max_y, min_z, max_z};

    std::vector<glm::dvec3> hull;
    for (size_t idx : extreme_indices) {
        hull.push_back(points[idx]);
    }

    // For simplicity, we'll use the extreme points plus points furthest from the center
    glm::dvec3 center(0.0);
    for (const auto& p : points) {
        center += p;
    }
    center /= static_cast<double>(points.size());

    // Add points that are far from center (likely on hull)
    std::vector<std::pair<double, size_t>> distances;
    for (size_t i = 0; i < points.size(); ++i) {
        if (extreme_indices.find(i) == extreme_indices.end()) {
            distances.emplace_back(glm::length(points[i] - center), i);
        }
    }

    std::sort(distances.rbegin(), distances.rend());

    // Add furthest points up to a limit
    size_t max_hull_size = 16;
    for (const auto& [dist, idx] : distances) {
        if (hull.size() >= max_hull_size) {
            break;
        }
        hull.push_back(points[idx]);
    }

    return hull;
}

}  // namespace realcraft::physics
