// RealCraft Physics Engine
// convex_hull_generator.hpp - Generate collision shapes from voxel clusters

#pragma once

#include "types.hpp"

#include <glm/glm.hpp>

#include <realcraft/world/types.hpp>
#include <unordered_set>
#include <vector>

namespace realcraft::physics {

// ============================================================================
// Convex Hull Generator
// ============================================================================

class ConvexHullGenerator {
public:
    // ========================================================================
    // Main Generation Functions
    // ========================================================================

    // Generate vertices for a convex hull collision shape from block positions
    // Returns vertices suitable for btConvexHullShape
    static std::vector<glm::vec3> generate_hull_vertices(const std::vector<world::WorldBlockPos>& positions,
                                                         double simplification_tolerance = 0.1);

    // Compute AABB half-extents for a simpler collision shape
    static glm::dvec3 compute_half_extents(const std::vector<world::WorldBlockPos>& positions);

    // Compute center position for collision shape
    static glm::dvec3 compute_center(const std::vector<world::WorldBlockPos>& positions);

    // ========================================================================
    // Decision Helpers
    // ========================================================================

    // Determine if convex hull is worth the complexity vs simple AABB
    // Returns true if hull would provide better collision fidelity
    static bool should_use_convex_hull(const std::vector<world::WorldBlockPos>& positions, size_t max_vertices = 16);

    // Estimate the number of hull vertices needed
    static size_t estimate_hull_vertex_count(const std::vector<world::WorldBlockPos>& positions);

    // ========================================================================
    // Utility Functions
    // ========================================================================

    // Extract surface vertices from voxel positions (corners of exposed faces)
    static std::vector<glm::dvec3> extract_surface_vertices(const std::vector<world::WorldBlockPos>& positions);

    // Simplify hull by reducing vertex count while preserving shape
    static std::vector<glm::vec3> simplify_hull(const std::vector<glm::dvec3>& vertices, size_t target_vertices);

    // Check if a point is inside the convex hull formed by vertices
    static bool point_in_hull(const glm::dvec3& point, const std::vector<glm::dvec3>& hull_vertices);

private:
    // Helper: Check if a block face is exposed (not adjacent to another block in set)
    static bool is_face_exposed(const world::WorldBlockPos& pos, int face_index,
                                const std::unordered_set<world::WorldBlockPos>& positions);

    // Helper: Get the 4 corner vertices of a block face
    static std::array<glm::dvec3, 4> get_face_vertices(const world::WorldBlockPos& pos, int face_index);

    // Helper: Quickhull algorithm for 3D convex hull
    static std::vector<glm::dvec3> compute_quickhull(const std::vector<glm::dvec3>& points);
};

}  // namespace realcraft::physics
