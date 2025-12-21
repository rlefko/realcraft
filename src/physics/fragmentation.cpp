// RealCraft Physics Engine
// fragmentation.cpp - Material-aware cluster fragmentation for debris

#include <algorithm>
#include <queue>
#include <realcraft/physics/fragmentation.hpp>
#include <realcraft/world/block.hpp>

namespace realcraft::physics {

// ============================================================================
// Fragment Implementation
// ============================================================================

void Fragment::compute_properties() {
    if (positions.empty()) {
        center_of_mass = glm::dvec3(0.0);
        total_mass = 0.0;
        bounds = AABB();
        average_brittleness = 0.0f;
        return;
    }

    const auto& registry = world::BlockRegistry::instance();

    // Initialize bounds
    glm::dvec3 min_pos(std::numeric_limits<double>::max());
    glm::dvec3 max_pos(std::numeric_limits<double>::lowest());

    // Compute weighted center of mass and total mass
    glm::dvec3 weighted_sum(0.0);
    total_mass = 0.0;
    float total_brittleness = 0.0f;
    size_t brittleness_count = 0;

    world::BlockId default_block = registry.stone_id();
    for (size_t i = 0; i < positions.size(); ++i) {
        const auto& pos = positions[i];
        world::BlockId block_id = (i < block_ids.size()) ? block_ids[i] : default_block;

        // Get block properties
        const world::BlockType* block_type = registry.get(block_id);
        double block_mass = 1000.0;  // Default 1000 kg
        float brittleness = 0.0f;

        if (block_type) {
            const auto& material = block_type->get_material();
            block_mass = static_cast<double>(material.weight);
            brittleness = material.brittleness;
        }

        // Block center is at pos + 0.5
        glm::dvec3 block_center(static_cast<double>(pos.x) + 0.5, static_cast<double>(pos.y) + 0.5,
                                static_cast<double>(pos.z) + 0.5);

        weighted_sum += block_center * block_mass;
        total_mass += block_mass;
        total_brittleness += brittleness;
        ++brittleness_count;

        // Update bounds
        min_pos = glm::min(min_pos, glm::dvec3(pos.x, pos.y, pos.z));
        max_pos = glm::max(max_pos, glm::dvec3(pos.x + 1.0, pos.y + 1.0, pos.z + 1.0));
    }

    if (total_mass > 0.0) {
        center_of_mass = weighted_sum / total_mass;
    } else {
        center_of_mass = (min_pos + max_pos) * 0.5;
    }

    bounds = AABB(min_pos, max_pos);
    average_brittleness = (brittleness_count > 0) ? (total_brittleness / static_cast<float>(brittleness_count)) : 0.0f;
}

// ============================================================================
// FragmentationAlgorithm Implementation
// ============================================================================

std::vector<Fragment> FragmentationAlgorithm::fragment_cluster(const BlockCluster& cluster,
                                                               FragmentationPattern pattern, uint32_t min_fragment_size,
                                                               uint32_t max_fragment_size, std::mt19937& rng) {
    if (cluster.empty()) {
        return {};
    }

    // If cluster is already within limits, return as single fragment
    if (cluster.size() <= max_fragment_size) {
        Fragment frag;
        frag.positions = cluster.positions;
        frag.block_ids = cluster.block_ids;
        frag.compute_properties();
        return {frag};
    }

    switch (pattern) {
        case FragmentationPattern::Uniform:
            return fragment_uniform(cluster, min_fragment_size, max_fragment_size, rng);
        case FragmentationPattern::Brittle:
            return fragment_by_brittleness(cluster, min_fragment_size, max_fragment_size, rng);
        case FragmentationPattern::Structural:
            return fragment_structural(cluster, min_fragment_size, max_fragment_size, rng);
        case FragmentationPattern::MaterialBased:
            return fragment_by_material(cluster, min_fragment_size, max_fragment_size, rng);
        default:
            return fragment_uniform(cluster, min_fragment_size, max_fragment_size, rng);
    }
}

std::vector<Fragment> FragmentationAlgorithm::fragment_uniform(const BlockCluster& cluster, uint32_t min_fragment_size,
                                                               uint32_t max_fragment_size, std::mt19937& rng) {
    // Build position-to-block mapping
    const auto& registry = world::BlockRegistry::instance();
    world::BlockId default_block = registry.stone_id();
    std::unordered_map<world::WorldBlockPos, world::BlockId> pos_to_block;
    for (size_t i = 0; i < cluster.positions.size(); ++i) {
        world::BlockId block_id = (i < cluster.block_ids.size()) ? cluster.block_ids[i] : default_block;
        pos_to_block[cluster.positions[i]] = block_id;
    }

    // Start with all positions
    std::unordered_set<world::WorldBlockPos> remaining(cluster.positions.begin(), cluster.positions.end());

    std::vector<Fragment> results;
    split_recursive(remaining, pos_to_block, max_fragment_size, results, rng);

    // Merge fragments that are too small
    std::vector<Fragment> final_results;
    Fragment merge_buffer;

    for (auto& frag : results) {
        if (frag.size() < min_fragment_size) {
            // Add to merge buffer
            merge_buffer.positions.insert(merge_buffer.positions.end(), frag.positions.begin(), frag.positions.end());
            merge_buffer.block_ids.insert(merge_buffer.block_ids.end(), frag.block_ids.begin(), frag.block_ids.end());

            if (merge_buffer.size() >= min_fragment_size) {
                merge_buffer.compute_properties();
                final_results.push_back(std::move(merge_buffer));
                merge_buffer = Fragment();
            }
        } else {
            final_results.push_back(std::move(frag));
        }
    }

    // Handle remaining merge buffer
    if (!merge_buffer.empty()) {
        if (!final_results.empty()) {
            // Merge with last fragment
            auto& last = final_results.back();
            last.positions.insert(last.positions.end(), merge_buffer.positions.begin(), merge_buffer.positions.end());
            last.block_ids.insert(last.block_ids.end(), merge_buffer.block_ids.begin(), merge_buffer.block_ids.end());
            last.compute_properties();
        } else {
            merge_buffer.compute_properties();
            final_results.push_back(std::move(merge_buffer));
        }
    }

    return final_results;
}

std::vector<Fragment> FragmentationAlgorithm::fragment_by_brittleness(const BlockCluster& cluster,
                                                                      uint32_t min_fragment_size,
                                                                      uint32_t max_fragment_size, std::mt19937& rng) {
    // Calculate average brittleness
    float avg_brittleness = compute_average_brittleness(cluster.positions, cluster.block_ids);

    // Determine target fragment count based on brittleness
    // High brittleness = more fragments
    // brittleness 0.0 -> 1 fragment per max_size blocks
    // brittleness 1.0 -> 1 fragment per min_size blocks
    float brittleness_factor = 1.0f + avg_brittleness * 3.0f;  // Range 1.0 to 4.0
    size_t ideal_fragment_size = static_cast<size_t>(static_cast<float>(max_fragment_size) / brittleness_factor);
    ideal_fragment_size =
        std::clamp(ideal_fragment_size, static_cast<size_t>(min_fragment_size), static_cast<size_t>(max_fragment_size));

    // Build position-to-block mapping
    const auto& registry = world::BlockRegistry::instance();
    world::BlockId default_block = registry.stone_id();
    std::unordered_map<world::WorldBlockPos, world::BlockId> pos_to_block;
    for (size_t i = 0; i < cluster.positions.size(); ++i) {
        world::BlockId block_id = (i < cluster.block_ids.size()) ? cluster.block_ids[i] : default_block;
        pos_to_block[cluster.positions[i]] = block_id;
    }

    std::unordered_set<world::WorldBlockPos> positions_set(cluster.positions.begin(), cluster.positions.end());

    // Find weak points to use as break locations
    auto weak_points = find_weak_points(positions_set);

    std::vector<Fragment> results;
    std::queue<std::unordered_set<world::WorldBlockPos>> work_queue;
    work_queue.push(positions_set);

    while (!work_queue.empty()) {
        auto current = std::move(work_queue.front());
        work_queue.pop();

        if (current.size() <= ideal_fragment_size) {
            // Create fragment
            std::vector<world::WorldBlockPos> pos_vec(current.begin(), current.end());
            results.push_back(create_fragment_from_positions(pos_vec, pos_to_block));
        } else {
            // Find a weak point to split at
            world::WorldBlockPos split_point{0, 0, 0};
            bool found_weak = false;

            for (const auto& weak : weak_points) {
                if (current.count(weak)) {
                    split_point = weak;
                    found_weak = true;
                    break;
                }
            }

            if (!found_weak) {
                // Pick a random position to split at
                std::uniform_int_distribution<size_t> dist(0, current.size() - 1);
                auto it = current.begin();
                std::advance(it, dist(rng));
                split_point = *it;
            }

            auto [left, right] = split_at_position(current, split_point, rng);

            if (!left.empty()) {
                work_queue.push(std::move(left));
            }
            if (!right.empty()) {
                work_queue.push(std::move(right));
            }
        }
    }

    return results;
}

std::vector<Fragment> FragmentationAlgorithm::fragment_structural(const BlockCluster& cluster,
                                                                  uint32_t min_fragment_size,
                                                                  uint32_t max_fragment_size, std::mt19937& rng) {
    // Build position-to-block mapping
    const auto& registry = world::BlockRegistry::instance();
    world::BlockId default_block = registry.stone_id();
    std::unordered_map<world::WorldBlockPos, world::BlockId> pos_to_block;
    for (size_t i = 0; i < cluster.positions.size(); ++i) {
        world::BlockId block_id = (i < cluster.block_ids.size()) ? cluster.block_ids[i] : default_block;
        pos_to_block[cluster.positions[i]] = block_id;
    }

    std::unordered_set<world::WorldBlockPos> positions_set(cluster.positions.begin(), cluster.positions.end());

    // Find all weak points (blocks with 1-2 neighbors)
    auto weak_points = find_weak_points(positions_set);

    if (weak_points.empty()) {
        // No weak points, fall back to uniform
        return fragment_uniform(cluster, min_fragment_size, max_fragment_size, rng);
    }

    // Remove weak points to create natural break lines
    std::unordered_set<world::WorldBlockPos> remaining = positions_set;
    for (const auto& weak : weak_points) {
        remaining.erase(weak);
    }

    // Find connected components after removing weak points
    auto components = find_connected_components(remaining);

    // Add weak points back to the nearest component
    for (const auto& weak : weak_points) {
        // Find which component this weak point should belong to
        int best_component = -1;
        int best_neighbor_count = 0;

        for (size_t i = 0; i < components.size(); ++i) {
            std::unordered_set<world::WorldBlockPos> comp_set(components[i].begin(), components[i].end());
            int neighbor_count = count_neighbors(weak, comp_set);
            if (neighbor_count > best_neighbor_count) {
                best_neighbor_count = neighbor_count;
                best_component = static_cast<int>(i);
            }
        }

        if (best_component >= 0) {
            components[static_cast<size_t>(best_component)].push_back(weak);
        } else if (!components.empty()) {
            // Add to first component if no neighbors found
            components[0].push_back(weak);
        }
    }

    // Create fragments from components
    std::vector<Fragment> results;
    for (const auto& component : components) {
        if (component.size() > max_fragment_size) {
            // Recursively split large components
            BlockCluster sub_cluster;
            sub_cluster.positions = component;
            for (const auto& pos : component) {
                auto it = pos_to_block.find(pos);
                sub_cluster.block_ids.push_back(it != pos_to_block.end() ? it->second : default_block);
            }
            auto sub_fragments = fragment_uniform(sub_cluster, min_fragment_size, max_fragment_size, rng);
            results.insert(results.end(), sub_fragments.begin(), sub_fragments.end());
        } else {
            results.push_back(create_fragment_from_positions(component, pos_to_block));
        }
    }

    return results;
}

std::vector<Fragment> FragmentationAlgorithm::fragment_by_material(const BlockCluster& cluster,
                                                                   uint32_t min_fragment_size,
                                                                   uint32_t max_fragment_size, std::mt19937& rng) {
    // Group positions by block type
    const auto& registry = world::BlockRegistry::instance();
    world::BlockId default_block = registry.stone_id();
    std::unordered_map<world::BlockId, std::vector<world::WorldBlockPos>> by_type;

    for (size_t i = 0; i < cluster.positions.size(); ++i) {
        world::BlockId block_id = (i < cluster.block_ids.size()) ? cluster.block_ids[i] : default_block;
        by_type[block_id].push_back(cluster.positions[i]);
    }

    std::vector<Fragment> results;

    for (auto& [block_id, positions] : by_type) {
        // Find connected components within this material type
        std::unordered_set<world::WorldBlockPos> pos_set(positions.begin(), positions.end());
        auto components = find_connected_components(pos_set);

        for (const auto& component : components) {
            if (component.size() > max_fragment_size) {
                // Split large components
                BlockCluster sub_cluster;
                sub_cluster.positions = component;
                sub_cluster.block_ids.resize(component.size(), block_id);
                auto sub_fragments = fragment_uniform(sub_cluster, min_fragment_size, max_fragment_size, rng);
                results.insert(results.end(), sub_fragments.begin(), sub_fragments.end());
            } else {
                Fragment frag;
                frag.positions = component;
                frag.block_ids.resize(component.size(), block_id);
                frag.compute_properties();
                results.push_back(std::move(frag));
            }
        }
    }

    return results;
}

float FragmentationAlgorithm::compute_average_brittleness(const std::vector<world::WorldBlockPos>& positions,
                                                          const std::vector<world::BlockId>& block_ids) {
    if (positions.empty()) {
        return 0.0f;
    }

    const auto& registry = world::BlockRegistry::instance();
    world::BlockId default_block = registry.stone_id();
    float total_brittleness = 0.0f;
    size_t count = 0;

    for (size_t i = 0; i < positions.size(); ++i) {
        world::BlockId block_id = (i < block_ids.size()) ? block_ids[i] : default_block;
        const world::BlockType* block_type = registry.get(block_id);

        if (block_type) {
            total_brittleness += block_type->get_material().brittleness;
            ++count;
        }
    }

    return (count > 0) ? (total_brittleness / static_cast<float>(count)) : 0.0f;
}

std::vector<world::WorldBlockPos>
FragmentationAlgorithm::find_weak_points(const std::unordered_set<world::WorldBlockPos>& positions) {
    std::vector<world::WorldBlockPos> weak_points;

    for (const auto& pos : positions) {
        int neighbor_count = count_neighbors(pos, positions);

        // A weak point has 1-2 neighbors (bridge block or corner)
        if (neighbor_count <= 2) {
            weak_points.push_back(pos);
        }
    }

    // Sort by neighbor count (fewest neighbors first - weakest points)
    std::sort(weak_points.begin(), weak_points.end(), [&positions](const auto& a, const auto& b) {
        return count_neighbors(a, positions) < count_neighbors(b, positions);
    });

    return weak_points;
}

std::vector<std::vector<world::WorldBlockPos>>
FragmentationAlgorithm::find_connected_components(const std::unordered_set<world::WorldBlockPos>& positions) {
    std::vector<std::vector<world::WorldBlockPos>> components;
    std::unordered_set<world::WorldBlockPos> visited;

    for (const auto& start : positions) {
        if (visited.count(start)) {
            continue;
        }

        // BFS to find connected component
        std::vector<world::WorldBlockPos> component;
        std::queue<world::WorldBlockPos> queue;
        queue.push(start);
        visited.insert(start);

        while (!queue.empty()) {
            auto current = queue.front();
            queue.pop();
            component.push_back(current);

            // Check 6 face-adjacent neighbors
            static const std::array<world::WorldBlockPos, 6> offsets = {
                {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}};

            for (const auto& offset : offsets) {
                world::WorldBlockPos neighbor{current.x + offset.x, current.y + offset.y, current.z + offset.z};

                if (positions.count(neighbor) && !visited.count(neighbor)) {
                    visited.insert(neighbor);
                    queue.push(neighbor);
                }
            }
        }

        if (!component.empty()) {
            components.push_back(std::move(component));
        }
    }

    return components;
}

int FragmentationAlgorithm::count_neighbors(const world::WorldBlockPos& pos,
                                            const std::unordered_set<world::WorldBlockPos>& positions) {
    static const std::array<world::WorldBlockPos, 6> offsets = {
        {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}};

    int count = 0;
    for (const auto& offset : offsets) {
        world::WorldBlockPos neighbor{pos.x + offset.x, pos.y + offset.y, pos.z + offset.z};
        if (positions.count(neighbor)) {
            ++count;
        }
    }
    return count;
}

Fragment FragmentationAlgorithm::create_fragment_from_positions(
    const std::vector<world::WorldBlockPos>& positions,
    const std::unordered_map<world::WorldBlockPos, world::BlockId>& pos_to_block) {
    Fragment frag;
    frag.positions = positions;
    frag.block_ids.reserve(positions.size());

    const auto& registry = world::BlockRegistry::instance();
    world::BlockId default_block = registry.stone_id();
    for (const auto& pos : positions) {
        auto it = pos_to_block.find(pos);
        frag.block_ids.push_back(it != pos_to_block.end() ? it->second : default_block);
    }

    frag.compute_properties();
    return frag;
}

std::pair<std::unordered_set<world::WorldBlockPos>, std::unordered_set<world::WorldBlockPos>>
FragmentationAlgorithm::split_at_position(const std::unordered_set<world::WorldBlockPos>& positions,
                                          const world::WorldBlockPos& cut_point, std::mt19937& rng) {
    // Remove the cut point and find resulting components
    std::unordered_set<world::WorldBlockPos> remaining = positions;
    remaining.erase(cut_point);

    if (remaining.empty()) {
        return {{cut_point}, {}};
    }

    auto components = find_connected_components(remaining);

    if (components.size() <= 1) {
        // Cutting didn't separate - split by coordinate instead
        // Find median coordinate and split there
        std::vector<int64_t> x_coords, y_coords, z_coords;
        for (const auto& pos : positions) {
            x_coords.push_back(pos.x);
            y_coords.push_back(pos.y);
            z_coords.push_back(pos.z);
        }

        std::sort(x_coords.begin(), x_coords.end());
        std::sort(y_coords.begin(), y_coords.end());
        std::sort(z_coords.begin(), z_coords.end());

        // Choose axis with largest spread
        int64_t x_spread = x_coords.back() - x_coords.front();
        int64_t y_spread = y_coords.back() - y_coords.front();
        int64_t z_spread = z_coords.back() - z_coords.front();

        std::unordered_set<world::WorldBlockPos> left, right;

        if (x_spread >= y_spread && x_spread >= z_spread) {
            int64_t median = x_coords[x_coords.size() / 2];
            for (const auto& pos : positions) {
                if (pos.x < median) {
                    left.insert(pos);
                } else {
                    right.insert(pos);
                }
            }
        } else if (y_spread >= z_spread) {
            int64_t median = y_coords[y_coords.size() / 2];
            for (const auto& pos : positions) {
                if (pos.y < median) {
                    left.insert(pos);
                } else {
                    right.insert(pos);
                }
            }
        } else {
            int64_t median = z_coords[z_coords.size() / 2];
            for (const auto& pos : positions) {
                if (pos.z < median) {
                    left.insert(pos);
                } else {
                    right.insert(pos);
                }
            }
        }

        // Handle edge case where all blocks are on one side
        if (left.empty() || right.empty()) {
            left.clear();
            right.clear();
            bool add_to_left = true;
            for (const auto& pos : positions) {
                if (add_to_left) {
                    left.insert(pos);
                } else {
                    right.insert(pos);
                }
                add_to_left = !add_to_left;
            }
        }

        return {left, right};
    }

    // Multiple components - assign cut point to random component
    std::uniform_int_distribution<size_t> dist(0, components.size() - 1);
    size_t target = dist(rng);
    components[target].push_back(cut_point);

    // Convert to sets and return first two (merge others into second)
    std::unordered_set<world::WorldBlockPos> first(components[0].begin(), components[0].end());
    std::unordered_set<world::WorldBlockPos> second;
    for (size_t i = 1; i < components.size(); ++i) {
        second.insert(components[i].begin(), components[i].end());
    }

    return {first, second};
}

void FragmentationAlgorithm::split_recursive(
    std::unordered_set<world::WorldBlockPos>& positions,
    const std::unordered_map<world::WorldBlockPos, world::BlockId>& pos_to_block, uint32_t max_size,
    std::vector<Fragment>& results, std::mt19937& rng) {
    if (positions.size() <= max_size) {
        std::vector<world::WorldBlockPos> pos_vec(positions.begin(), positions.end());
        results.push_back(create_fragment_from_positions(pos_vec, pos_to_block));
        return;
    }

    // Find a split point
    auto weak_points = find_weak_points(positions);
    world::WorldBlockPos split_point{0, 0, 0};

    if (!weak_points.empty()) {
        split_point = weak_points[0];
    } else {
        std::uniform_int_distribution<size_t> dist(0, positions.size() - 1);
        auto it = positions.begin();
        std::advance(it, dist(rng));
        split_point = *it;
    }

    auto [left, right] = split_at_position(positions, split_point, rng);

    if (!left.empty()) {
        split_recursive(left, pos_to_block, max_size, results, rng);
    }
    if (!right.empty()) {
        split_recursive(right, pos_to_block, max_size, results, rng);
    }
}

}  // namespace realcraft::physics
