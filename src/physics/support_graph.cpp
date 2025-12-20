// RealCraft Physics Engine
// support_graph.cpp - Per-chunk block support tracking implementation

#include <algorithm>
#include <limits>
#include <queue>
#include <realcraft/physics/support_graph.hpp>
#include <unordered_set>

namespace realcraft::physics {

// ============================================================================
// BlockCluster Implementation
// ============================================================================

void BlockCluster::compute_properties() {
    if (positions.empty()) {
        center_of_mass = glm::dvec3(0.0);
        total_mass = 0.0;
        bounds = AABB();
        return;
    }

    const auto& registry = world::BlockRegistry::instance();

    glm::dvec3 weighted_sum(0.0);
    total_mass = 0.0;

    glm::dvec3 min_pos(std::numeric_limits<double>::max());
    glm::dvec3 max_pos(std::numeric_limits<double>::lowest());

    for (size_t i = 0; i < positions.size(); ++i) {
        const world::BlockType* type = registry.get(block_ids[i]);
        double block_mass = type ? type->get_material().weight : 1.0;

        // Block center is at position + 0.5
        glm::dvec3 block_center(static_cast<double>(positions[i].x) + 0.5, static_cast<double>(positions[i].y) + 0.5,
                                static_cast<double>(positions[i].z) + 0.5);

        weighted_sum += block_center * block_mass;
        total_mass += block_mass;

        // Update bounds (block extends from pos to pos+1)
        min_pos = glm::min(min_pos, glm::dvec3(positions[i]));
        max_pos = glm::max(max_pos, glm::dvec3(positions[i]) + 1.0);
    }

    if (total_mass > 0.0) {
        center_of_mass = weighted_sum / total_mass;
    } else {
        center_of_mass = (min_pos + max_pos) * 0.5;
    }

    bounds = AABB(min_pos, max_pos);
}

// ============================================================================
// SupportGraph Implementation
// ============================================================================

struct SupportGraph::Impl {
    SupportGraphConfig config;
    bool initialized = false;

    // Map from world position to block ID
    std::unordered_map<world::WorldBlockPos, world::BlockId> blocks;

    // Get all 6-connected neighbors of a position
    std::vector<world::WorldBlockPos> get_neighbors(const world::WorldBlockPos& pos) const {
        std::vector<world::WorldBlockPos> neighbors;
        neighbors.reserve(6);

        for (int dir = 0; dir < 6; ++dir) {
            glm::ivec3 offset = world::DIRECTION_OFFSETS[dir];
            world::WorldBlockPos neighbor_pos(pos.x + offset.x, pos.y + offset.y, pos.z + offset.z);

            // Skip positions below world
            if (neighbor_pos.y < 0) {
                continue;
            }

            neighbors.push_back(neighbor_pos);
        }

        return neighbors;
    }

    // Check if position is at or below ground level
    bool is_at_ground(const world::WorldBlockPos& pos) const { return pos.y <= config.ground_level; }

    // Get direction cost for support distance calculation
    float get_direction_cost(const world::WorldBlockPos& from, const world::WorldBlockPos& to,
                             const world::BlockType* block_type) const {
        // Vertical movement (up/down) has cost of 1.0
        if (from.x == to.x && from.z == to.z) {
            return 1.0f;
        }
        // Horizontal movement uses the material's horizontal cost
        return block_type ? block_type->get_material().horizontal_cost : 1.5f;
    }
};

SupportGraph::SupportGraph() : impl_(std::make_unique<Impl>()) {}

SupportGraph::~SupportGraph() = default;

void SupportGraph::initialize(const SupportGraphConfig& config) {
    impl_->config = config;
    impl_->blocks.clear();
    impl_->initialized = true;
}

void SupportGraph::shutdown() {
    impl_->blocks.clear();
    impl_->initialized = false;
}

bool SupportGraph::is_initialized() const {
    return impl_->initialized;
}

void SupportGraph::add_block(const world::WorldBlockPos& pos, world::BlockId block_id) {
    if (!impl_->initialized || block_id == world::BLOCK_AIR) {
        return;
    }

    // Check if block requires support
    const auto* block_type = world::BlockRegistry::instance().get(block_id);
    if (!block_type || !block_type->get_material().requires_support) {
        return;
    }

    impl_->blocks[pos] = block_id;
}

std::vector<world::WorldBlockPos> SupportGraph::remove_block(const world::WorldBlockPos& pos) {
    std::vector<world::WorldBlockPos> affected_neighbors;

    auto it = impl_->blocks.find(pos);
    if (it == impl_->blocks.end()) {
        return affected_neighbors;
    }

    // Remove the block
    impl_->blocks.erase(it);

    // Find all solid neighbors that might be affected
    for (int dir = 0; dir < 6; ++dir) {
        glm::ivec3 offset = world::DIRECTION_OFFSETS[dir];
        world::WorldBlockPos neighbor_pos(pos.x + offset.x, pos.y + offset.y, pos.z + offset.z);

        if (impl_->blocks.count(neighbor_pos) > 0) {
            affected_neighbors.push_back(neighbor_pos);
        }
    }

    return affected_neighbors;
}

bool SupportGraph::has_block(const world::WorldBlockPos& pos) const {
    return impl_->blocks.count(pos) > 0;
}

world::BlockId SupportGraph::get_block(const world::WorldBlockPos& pos) const {
    auto it = impl_->blocks.find(pos);
    return it != impl_->blocks.end() ? it->second : world::BLOCK_AIR;
}

bool SupportGraph::check_ground_connectivity(const world::WorldBlockPos& pos,
                                             std::vector<world::WorldBlockPos>& path_to_ground) const {
    path_to_ground.clear();

    if (!impl_->blocks.count(pos)) {
        return false;  // Block not tracked
    }

    // Get the block type for support distance limit
    world::BlockId block_id = impl_->blocks.at(pos);
    const auto* start_block_type = world::BlockRegistry::instance().get(block_id);
    float max_distance = start_block_type ? start_block_type->get_material().max_support_distance : 64.0f;

    // BFS to find path to ground
    struct SearchNode {
        world::WorldBlockPos pos;
        float distance;
    };

    std::queue<SearchNode> frontier;
    std::unordered_map<world::WorldBlockPos, world::WorldBlockPos> came_from;
    std::unordered_map<world::WorldBlockPos, float> distances;

    frontier.push({pos, 0.0f});
    came_from[pos] = pos;  // Self-reference marks start
    distances[pos] = 0.0f;

    while (!frontier.empty() && distances.size() < impl_->config.max_search_blocks) {
        SearchNode current = frontier.front();
        frontier.pop();

        // Check if we reached ground
        if (impl_->is_at_ground(current.pos)) {
            // Reconstruct path
            world::WorldBlockPos trace = current.pos;
            while (came_from[trace] != trace) {
                path_to_ground.push_back(trace);
                trace = came_from[trace];
            }
            path_to_ground.push_back(trace);
            std::reverse(path_to_ground.begin(), path_to_ground.end());
            return true;
        }

        // Explore neighbors
        auto neighbors = impl_->get_neighbors(current.pos);
        for (const auto& neighbor : neighbors) {
            // Must be a tracked solid block
            auto neighbor_it = impl_->blocks.find(neighbor);
            if (neighbor_it == impl_->blocks.end()) {
                continue;
            }

            // Get block type for cost calculation
            const auto* block_type = world::BlockRegistry::instance().get(neighbor_it->second);
            float cost = impl_->get_direction_cost(current.pos, neighbor, block_type);
            float new_distance = current.distance + cost;

            // Check if within support distance
            float block_max_distance = block_type ? block_type->get_material().max_support_distance : 64.0f;
            if (new_distance > std::min(max_distance, block_max_distance)) {
                continue;
            }

            // Check if we found a better path
            auto dist_it = distances.find(neighbor);
            if (dist_it == distances.end() || new_distance < dist_it->second) {
                distances[neighbor] = new_distance;
                came_from[neighbor] = current.pos;
                frontier.push({neighbor, new_distance});
            }
        }
    }

    // No path to ground found
    return false;
}

float SupportGraph::get_support_distance(const world::WorldBlockPos& pos) const {
    std::vector<world::WorldBlockPos> path;
    if (check_ground_connectivity(pos, path)) {
        // Calculate actual distance along path
        float distance = 0.0f;
        for (size_t i = 1; i < path.size(); ++i) {
            const auto* block_type = world::BlockRegistry::instance().get(get_block(path[i]));
            distance += impl_->get_direction_cost(path[i - 1], path[i], block_type);
        }
        return distance;
    }
    return std::numeric_limits<float>::infinity();
}

bool SupportGraph::is_ground_level(const world::WorldBlockPos& pos) const {
    return impl_->is_at_ground(pos);
}

std::vector<BlockCluster>
SupportGraph::find_unsupported_clusters(const std::vector<world::WorldBlockPos>& affected_neighbors) {
    std::vector<BlockCluster> clusters;
    std::unordered_set<world::WorldBlockPos> processed;
    std::unordered_set<world::WorldBlockPos> all_unsupported;

    // First pass: identify all unsupported blocks
    for (const auto& neighbor : affected_neighbors) {
        if (processed.count(neighbor) > 0) {
            continue;
        }

        std::vector<world::WorldBlockPos> path;
        if (!check_ground_connectivity(neighbor, path)) {
            // This block is unsupported - flood fill to find all connected unsupported blocks
            std::queue<world::WorldBlockPos> flood_queue;
            flood_queue.push(neighbor);
            processed.insert(neighbor);

            while (!flood_queue.empty()) {
                world::WorldBlockPos current = flood_queue.front();
                flood_queue.pop();

                // Check if this block is unsupported
                std::vector<world::WorldBlockPos> current_path;
                if (!check_ground_connectivity(current, current_path)) {
                    all_unsupported.insert(current);

                    // Add neighbors to flood fill
                    auto neighbors = impl_->get_neighbors(current);
                    for (const auto& n : neighbors) {
                        if (impl_->blocks.count(n) > 0 && processed.count(n) == 0) {
                            processed.insert(n);
                            flood_queue.push(n);
                        }
                    }
                }
            }
        } else {
            processed.insert(neighbor);
        }
    }

    // Second pass: group unsupported blocks into connected clusters
    std::unordered_set<world::WorldBlockPos> assigned;
    for (const auto& block_pos : all_unsupported) {
        if (assigned.count(block_pos) > 0) {
            continue;
        }

        BlockCluster cluster;
        std::queue<world::WorldBlockPos> cluster_queue;
        cluster_queue.push(block_pos);
        assigned.insert(block_pos);

        while (!cluster_queue.empty()) {
            world::WorldBlockPos current = cluster_queue.front();
            cluster_queue.pop();

            cluster.positions.push_back(current);
            cluster.block_ids.push_back(get_block(current));

            // Find connected unsupported neighbors
            auto neighbors = impl_->get_neighbors(current);
            for (const auto& n : neighbors) {
                if (all_unsupported.count(n) > 0 && assigned.count(n) == 0) {
                    assigned.insert(n);
                    cluster_queue.push(n);
                }
            }
        }

        // Compute cluster properties
        cluster.compute_properties();
        clusters.push_back(std::move(cluster));
    }

    // Remove unsupported blocks from the graph
    for (const auto& block_pos : all_unsupported) {
        impl_->blocks.erase(block_pos);
    }

    return clusters;
}

void SupportGraph::build_from_chunk(const world::ChunkPos& chunk_pos,
                                    const std::function<world::BlockId(const world::WorldBlockPos&)>& get_block_fn) {
    // Iterate through all positions in the chunk
    for (int y = 0; y < world::CHUNK_SIZE_Y; ++y) {
        for (int z = 0; z < world::CHUNK_SIZE_Z; ++z) {
            for (int x = 0; x < world::CHUNK_SIZE_X; ++x) {
                world::LocalBlockPos local(x, y, z);
                world::WorldBlockPos world_pos = world::local_to_world(chunk_pos, local);
                world::BlockId block_id = get_block_fn(world_pos);

                if (block_id != world::BLOCK_AIR) {
                    add_block(world_pos, block_id);
                }
            }
        }
    }
}

void SupportGraph::remove_chunk(const world::ChunkPos& chunk_pos) {
    // Calculate world coordinate range for this chunk
    int64_t min_x = static_cast<int64_t>(chunk_pos.x) * world::CHUNK_SIZE_X;
    int64_t max_x = min_x + world::CHUNK_SIZE_X;
    int64_t min_z = static_cast<int64_t>(chunk_pos.y) * world::CHUNK_SIZE_Z;
    int64_t max_z = min_z + world::CHUNK_SIZE_Z;

    // Remove all blocks in this chunk
    for (auto it = impl_->blocks.begin(); it != impl_->blocks.end();) {
        const auto& pos = it->first;
        if (pos.x >= min_x && pos.x < max_x && pos.z >= min_z && pos.z < max_z) {
            it = impl_->blocks.erase(it);
        } else {
            ++it;
        }
    }
}

size_t SupportGraph::tracked_block_count() const {
    return impl_->blocks.size();
}

const SupportGraphConfig& SupportGraph::get_config() const {
    return impl_->config;
}

}  // namespace realcraft::physics
