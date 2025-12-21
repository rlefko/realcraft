// RealCraft Physics Engine
// fragmentation.hpp - Material-aware cluster fragmentation for debris

#pragma once

#include "support_graph.hpp"
#include "types.hpp"

#include <random>
#include <realcraft/world/block.hpp>
#include <realcraft/world/types.hpp>
#include <unordered_set>
#include <vector>

namespace realcraft::physics {

// ============================================================================
// Fragmentation Pattern
// ============================================================================

enum class FragmentationPattern {
    Uniform,       // Equal-sized pieces (simple splitting)
    Brittle,       // Many small pieces based on material brittleness
    Structural,    // Breaks along weak points (single-block connections)
    MaterialBased  // Respects material boundaries between block types
};

// ============================================================================
// Fragment
// ============================================================================

struct Fragment {
    std::vector<world::WorldBlockPos> positions;
    std::vector<world::BlockId> block_ids;

    // Computed properties
    glm::dvec3 center_of_mass{0.0};
    double total_mass = 0.0;
    AABB bounds;
    float average_brittleness = 0.0f;

    // Check if fragment is empty
    [[nodiscard]] bool empty() const { return positions.empty(); }

    // Get fragment size (block count)
    [[nodiscard]] size_t size() const { return positions.size(); }

    // Compute all derived properties from positions and block_ids
    void compute_properties();
};

// ============================================================================
// Fragmentation Algorithm
// ============================================================================

class FragmentationAlgorithm {
public:
    // ========================================================================
    // Main Fragmentation Entry Point
    // ========================================================================

    // Fragment a cluster into smaller pieces based on the specified pattern
    static std::vector<Fragment> fragment_cluster(const BlockCluster& cluster, FragmentationPattern pattern,
                                                  uint32_t min_fragment_size, uint32_t max_fragment_size,
                                                  std::mt19937& rng);

    // ========================================================================
    // Specific Fragmentation Patterns
    // ========================================================================

    // Uniform fragmentation - splits cluster into roughly equal pieces
    static std::vector<Fragment> fragment_uniform(const BlockCluster& cluster, uint32_t min_fragment_size,
                                                  uint32_t max_fragment_size, std::mt19937& rng);

    // Brittleness-based fragmentation - more fragments for brittle materials
    static std::vector<Fragment> fragment_by_brittleness(const BlockCluster& cluster, uint32_t min_fragment_size,
                                                         uint32_t max_fragment_size, std::mt19937& rng);

    // Structural fragmentation - breaks at weak connection points
    static std::vector<Fragment> fragment_structural(const BlockCluster& cluster, uint32_t min_fragment_size,
                                                     uint32_t max_fragment_size, std::mt19937& rng);

    // Material-based fragmentation - respects different block type boundaries
    static std::vector<Fragment> fragment_by_material(const BlockCluster& cluster, uint32_t min_fragment_size,
                                                      uint32_t max_fragment_size, std::mt19937& rng);

    // ========================================================================
    // Utility Functions
    // ========================================================================

    // Compute average brittleness for a set of blocks
    static float compute_average_brittleness(const std::vector<world::WorldBlockPos>& positions,
                                             const std::vector<world::BlockId>& block_ids);

    // Find weak connection points in a cluster (blocks with few connections)
    static std::vector<world::WorldBlockPos>
    find_weak_points(const std::unordered_set<world::WorldBlockPos>& positions);

    // Find connected components within a set of positions
    static std::vector<std::vector<world::WorldBlockPos>>
    find_connected_components(const std::unordered_set<world::WorldBlockPos>& positions);

    // Count face-adjacent neighbors within the cluster
    static int count_neighbors(const world::WorldBlockPos& pos,
                               const std::unordered_set<world::WorldBlockPos>& positions);

private:
    // Helper: Create a Fragment from a set of positions
    static Fragment
    create_fragment_from_positions(const std::vector<world::WorldBlockPos>& positions,
                                   const std::unordered_map<world::WorldBlockPos, world::BlockId>& pos_to_block);

    // Helper: Split a set of positions at a specified cut point
    static std::pair<std::unordered_set<world::WorldBlockPos>, std::unordered_set<world::WorldBlockPos>>
    split_at_position(const std::unordered_set<world::WorldBlockPos>& positions, const world::WorldBlockPos& cut_point,
                      std::mt19937& rng);

    // Helper: Recursive splitting for uniform fragmentation
    static void split_recursive(std::unordered_set<world::WorldBlockPos>& positions,
                                const std::unordered_map<world::WorldBlockPos, world::BlockId>& pos_to_block,
                                uint32_t max_size, std::vector<Fragment>& results, std::mt19937& rng);
};

}  // namespace realcraft::physics
