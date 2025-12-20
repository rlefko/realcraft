// RealCraft Physics Engine
// support_graph.hpp - Per-chunk block support tracking for structural integrity

#pragma once

#include "types.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <realcraft/world/block.hpp>
#include <realcraft/world/types.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace realcraft::physics {

// ============================================================================
// Block Cluster (result of collapse detection)
// ============================================================================

struct BlockCluster {
    std::vector<world::WorldBlockPos> positions;
    std::vector<world::BlockId> block_ids;

    // Computed properties (call compute_properties() to populate)
    glm::dvec3 center_of_mass{0.0};
    double total_mass = 0.0;
    AABB bounds;

    // Compute mass, center of mass, and bounds from block positions
    void compute_properties();

    // Check if cluster is empty
    [[nodiscard]] bool empty() const { return positions.empty(); }

    // Get cluster size
    [[nodiscard]] size_t size() const { return positions.size(); }
};

// ============================================================================
// Support Graph Configuration
// ============================================================================

struct SupportGraphConfig {
    // Search limits
    uint32_t max_search_blocks = 10000;  // Maximum blocks to search in BFS

    // Ground level (blocks at or below this Y are always supported)
    int32_t ground_level = 0;

    // Whether to include diagonal (edge/corner) connections
    bool include_edge_connections = false;
    bool include_corner_connections = false;
};

// ============================================================================
// Support Graph
// ============================================================================

class SupportGraph {
public:
    SupportGraph();
    ~SupportGraph();

    // Non-copyable, non-movable
    SupportGraph(const SupportGraph&) = delete;
    SupportGraph& operator=(const SupportGraph&) = delete;
    SupportGraph(SupportGraph&&) = delete;
    SupportGraph& operator=(SupportGraph&&) = delete;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    void initialize(const SupportGraphConfig& config = {});
    void shutdown();
    [[nodiscard]] bool is_initialized() const;

    // ========================================================================
    // Block Management
    // ========================================================================

    // Add a solid block to tracking
    void add_block(const world::WorldBlockPos& pos, world::BlockId block_id);

    // Remove a block from tracking (returns neighbors that need checking)
    std::vector<world::WorldBlockPos> remove_block(const world::WorldBlockPos& pos);

    // Check if a block is being tracked
    [[nodiscard]] bool has_block(const world::WorldBlockPos& pos) const;

    // Get block ID at position (or BLOCK_AIR if not tracked)
    [[nodiscard]] world::BlockId get_block(const world::WorldBlockPos& pos) const;

    // ========================================================================
    // Support Queries
    // ========================================================================

    // Check if a block can reach ground through the support graph
    // Returns true if supported, false if not (and fills unsupported_cluster)
    [[nodiscard]] bool check_ground_connectivity(const world::WorldBlockPos& pos,
                                                 std::vector<world::WorldBlockPos>& path_to_ground) const;

    // Get the support distance from a block to ground (infinity if not connected)
    [[nodiscard]] float get_support_distance(const world::WorldBlockPos& pos) const;

    // Check if a position is at ground level
    [[nodiscard]] bool is_ground_level(const world::WorldBlockPos& pos) const;

    // ========================================================================
    // Collapse Detection
    // ========================================================================

    // Find all blocks that lost support after a block was removed
    // The removed block should already be removed via remove_block()
    // affected_neighbors are the neighbors returned by remove_block()
    std::vector<BlockCluster> find_unsupported_clusters(const std::vector<world::WorldBlockPos>& affected_neighbors);

    // ========================================================================
    // Chunk Management
    // ========================================================================

    // Build support data for an entire chunk
    void build_from_chunk(const world::ChunkPos& chunk_pos,
                          const std::function<world::BlockId(const world::WorldBlockPos&)>& get_block);

    // Remove all blocks in a chunk
    void remove_chunk(const world::ChunkPos& chunk_pos);

    // ========================================================================
    // Statistics
    // ========================================================================

    [[nodiscard]] size_t tracked_block_count() const;

    // ========================================================================
    // Configuration
    // ========================================================================

    [[nodiscard]] const SupportGraphConfig& get_config() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::physics
