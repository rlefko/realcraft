// RealCraft Physics Engine
// structural_integrity.cpp - Structural integrity system implementation

#include <chrono>
#include <queue>
#include <random>
#include <realcraft/physics/physics_world.hpp>
#include <realcraft/physics/structural_integrity.hpp>

namespace realcraft::physics {

// ============================================================================
// Implementation Details
// ============================================================================

struct StructuralIntegritySystem::Impl {
    StructuralIntegrityConfig config;
    bool initialized = false;
    bool enabled = true;

    PhysicsWorld* physics_world = nullptr;
    world::WorldManager* world_manager = nullptr;

    // Support graph for tracking block connectivity
    SupportGraph support_graph;

    // Pending checks (positions to check on next update)
    std::queue<world::WorldBlockPos> pending_checks;

    // Pending collapses (clusters to spawn as debris)
    std::queue<CollapseEvent> pending_collapses;

    // Callback for collapse events
    CollapseCallback collapse_callback;

    // Statistics
    Stats stats;

    // Time tracking
    double current_time = 0.0;
    double cascade_accumulator = 0.0;

    // Random generator for debris variation
    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<double> velocity_dist{-0.5, 0.5};

    // Fragment a large cluster into smaller clusters
    std::vector<BlockCluster> fragment_cluster(const BlockCluster& cluster) {
        std::vector<BlockCluster> fragments;

        if (cluster.size() <= config.max_cluster_size) {
            fragments.push_back(cluster);
            return fragments;
        }

        // Simple fragmentation: split into roughly equal-sized clusters
        size_t num_fragments = (cluster.size() + config.max_cluster_size - 1) / config.max_cluster_size;
        size_t blocks_per_fragment = cluster.size() / num_fragments;

        for (size_t i = 0; i < num_fragments; ++i) {
            BlockCluster fragment;
            size_t start = i * blocks_per_fragment;
            size_t end = (i == num_fragments - 1) ? cluster.size() : (i + 1) * blocks_per_fragment;

            for (size_t j = start; j < end; ++j) {
                fragment.positions.push_back(cluster.positions[j]);
                fragment.block_ids.push_back(cluster.block_ids[j]);
            }

            fragment.compute_properties();
            fragments.push_back(std::move(fragment));
        }

        return fragments;
    }

    // Create a debris rigid body from a cluster
    RigidBodyHandle create_debris(const BlockCluster& cluster) {
        if (!physics_world || cluster.empty()) {
            return INVALID_RIGID_BODY;
        }

        RigidBodyDesc desc;

        // Position at center of mass
        desc.position = cluster.center_of_mass;
        desc.rotation = glm::dquat(1.0, 0.0, 0.0, 0.0);

        // Mass from total block weights
        desc.mass = cluster.total_mass;
        desc.motion_type = MotionType::Dynamic;

        // Use AABB for debris (simpler collision)
        desc.collider_type = ColliderType::AABB;
        desc.half_extents = cluster.bounds.half_extents();

        // Ensure minimum size
        desc.half_extents = glm::max(desc.half_extents, glm::dvec3(0.25));

        // Get physics material from the most common block type
        if (!cluster.block_ids.empty()) {
            const auto* block_type = world::BlockRegistry::instance().get(cluster.block_ids[0]);
            if (block_type) {
                desc.material.friction = block_type->get_material().friction;
                desc.material.restitution = block_type->get_material().restitution;
            }
        }

        // Collision layer for debris
        desc.collision_layer = static_cast<uint32_t>(CollisionLayer::Debris);
        desc.collision_mask = static_cast<uint32_t>(CollisionLayer::Static | CollisionLayer::Debris |
                                                    CollisionLayer::Player | CollisionLayer::Entity);

        // Small random initial velocity for visual variety
        desc.linear_velocity = glm::dvec3(velocity_dist(rng), velocity_dist(rng) * 0.5 - 0.1, velocity_dist(rng));

        return physics_world->create_rigid_body(desc);
    }

    // Remove blocks from world
    void remove_blocks_from_world(const std::vector<world::WorldBlockPos>& positions) {
        if (!world_manager) {
            return;
        }

        for (const auto& pos : positions) {
            // Set block to air without triggering recursive structural checks
            // This is done by directly modifying the chunk
            world::ChunkPos chunk_pos = world::world_to_chunk(pos);
            world::LocalBlockPos local_pos = world::world_to_local(pos);

            auto* chunk = world_manager->get_chunk(chunk_pos);
            if (chunk) {
                chunk->set_block(local_pos, world::BLOCK_AIR, 0);
            }
        }
    }

    // Spawn debris for a collapse event
    void spawn_collapse_debris(const CollapseEvent& event) {
        size_t debris_spawned = 0;

        for (const auto& cluster : event.falling_clusters) {
            if (debris_spawned >= config.max_debris_per_frame) {
                break;
            }

            // Fragment large clusters
            auto fragments = fragment_cluster(cluster);

            for (const auto& fragment : fragments) {
                if (debris_spawned >= config.max_debris_per_frame) {
                    break;
                }

                // Remove blocks from world
                remove_blocks_from_world(fragment.positions);

                // Create rigid body
                RigidBodyHandle handle = create_debris(fragment);
                if (handle != INVALID_RIGID_BODY) {
                    ++debris_spawned;
                    stats.total_debris_spawned++;
                }

                stats.total_blocks_collapsed += fragment.size();
            }
        }
    }
};

// ============================================================================
// StructuralIntegritySystem Implementation
// ============================================================================

StructuralIntegritySystem::StructuralIntegritySystem() : impl_(std::make_unique<Impl>()) {}

StructuralIntegritySystem::~StructuralIntegritySystem() {
    if (impl_->initialized) {
        shutdown();
    }
}

bool StructuralIntegritySystem::initialize(PhysicsWorld* physics_world, world::WorldManager* world_manager,
                                           const StructuralIntegrityConfig& config) {
    if (!physics_world || !world_manager) {
        return false;
    }

    impl_->physics_world = physics_world;
    impl_->world_manager = world_manager;
    impl_->config = config;

    // Initialize support graph
    SupportGraphConfig graph_config;
    graph_config.max_search_blocks = config.max_search_blocks;
    graph_config.ground_level = config.ground_level;
    impl_->support_graph.initialize(graph_config);

    impl_->initialized = true;
    impl_->enabled = true;
    impl_->stats = Stats{};

    return true;
}

void StructuralIntegritySystem::shutdown() {
    impl_->support_graph.shutdown();

    // Clear queues
    while (!impl_->pending_checks.empty()) {
        impl_->pending_checks.pop();
    }
    while (!impl_->pending_collapses.empty()) {
        impl_->pending_collapses.pop();
    }

    impl_->physics_world = nullptr;
    impl_->world_manager = nullptr;
    impl_->initialized = false;
}

bool StructuralIntegritySystem::is_initialized() const {
    return impl_->initialized;
}

void StructuralIntegritySystem::update(double delta_time) {
    if (!impl_->initialized || !impl_->enabled) {
        return;
    }

    impl_->current_time += delta_time;

    auto start_time = std::chrono::high_resolution_clock::now();

    // Process pending checks
    while (!impl_->pending_checks.empty()) {
        // Check time budget
        auto now = std::chrono::high_resolution_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(now - start_time).count();
        if (elapsed_ms >= impl_->config.max_check_time_per_frame_ms) {
            break;
        }

        world::WorldBlockPos pos = impl_->pending_checks.front();
        impl_->pending_checks.pop();

        check_structural_integrity(pos);
    }

    // Process pending collapses
    while (!impl_->pending_collapses.empty()) {
        CollapseEvent event = std::move(impl_->pending_collapses.front());
        impl_->pending_collapses.pop();

        impl_->spawn_collapse_debris(event);

        // Invoke callback
        if (impl_->collapse_callback) {
            impl_->collapse_callback(event);
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    impl_->stats.last_check_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    impl_->stats.tracked_blocks = impl_->support_graph.tracked_block_count();
    impl_->stats.pending_checks = impl_->pending_checks.size();
}

void StructuralIntegritySystem::on_chunk_loaded(const world::ChunkPos& pos, world::Chunk& chunk) {
    if (!impl_->initialized) {
        return;
    }

    // Build support data for the chunk
    impl_->support_graph.build_from_chunk(pos, [&chunk, &pos](const world::WorldBlockPos& world_pos) {
        world::LocalBlockPos local = world::world_to_local(world_pos);
        return chunk.get_block(local);
    });
}

void StructuralIntegritySystem::on_chunk_unloading(const world::ChunkPos& pos, const world::Chunk& chunk) {
    (void)chunk;

    if (!impl_->initialized) {
        return;
    }

    // Remove support data for the chunk
    impl_->support_graph.remove_chunk(pos);
}

void StructuralIntegritySystem::on_block_changed(const world::BlockChangeEvent& event) {
    if (!impl_->initialized || !impl_->enabled) {
        return;
    }

    // Skip changes from world generation
    if (event.from_generation) {
        return;
    }

    // Handle block placement (add to graph)
    if (event.old_entry.block_id == world::BLOCK_AIR && event.new_entry.block_id != world::BLOCK_AIR) {
        impl_->support_graph.add_block(event.position, event.new_entry.block_id);
        return;
    }

    // Handle block replacement (update graph)
    if (event.old_entry.block_id != world::BLOCK_AIR && event.new_entry.block_id != world::BLOCK_AIR) {
        // Remove old, add new
        impl_->support_graph.remove_block(event.position);
        impl_->support_graph.add_block(event.position, event.new_entry.block_id);
        return;
    }

    // Handle block removal (check for collapse)
    if (event.old_entry.block_id != world::BLOCK_AIR && event.new_entry.block_id == world::BLOCK_AIR) {
        // Remove block and get affected neighbors
        auto affected = impl_->support_graph.remove_block(event.position);

        if (affected.empty()) {
            return;
        }

        // Check or queue structural integrity
        if (impl_->config.defer_checks_to_update) {
            for (const auto& neighbor : affected) {
                impl_->pending_checks.push(neighbor);
            }
        } else {
            // Find unsupported clusters
            auto clusters = impl_->support_graph.find_unsupported_clusters(affected);

            if (!clusters.empty()) {
                CollapseEvent collapse_event;
                collapse_event.trigger_position = event.position;
                collapse_event.falling_clusters = std::move(clusters);
                collapse_event.timestamp = impl_->current_time;

                impl_->pending_collapses.push(std::move(collapse_event));
                impl_->stats.total_collapses++;
            }
        }
    }
}

void StructuralIntegritySystem::on_origin_shifted(const world::WorldBlockPos& old_origin,
                                                  const world::WorldBlockPos& new_origin) {
    (void)old_origin;
    (void)new_origin;

    // The support graph uses world coordinates, so we don't need to shift anything
    // The rigid bodies are handled by PhysicsWorld
}

CollapseResult StructuralIntegritySystem::check_structural_integrity(const world::WorldBlockPos& pos) {
    CollapseResult result;

    if (!impl_->initialized || !impl_->enabled) {
        return result;
    }

    // Check if position has a block
    if (!impl_->support_graph.has_block(pos)) {
        return result;
    }

    // Check ground connectivity
    std::vector<world::WorldBlockPos> path;
    if (impl_->support_graph.check_ground_connectivity(pos, path)) {
        // Block is supported
        return result;
    }

    // Block is not supported - find all connected unsupported blocks
    std::vector<world::WorldBlockPos> neighbors;
    neighbors.push_back(pos);

    auto clusters = impl_->support_graph.find_unsupported_clusters(neighbors);

    if (clusters.empty()) {
        return result;
    }

    // Record result
    result.collapse_occurred = true;
    for (const auto& cluster : clusters) {
        for (const auto& block_pos : cluster.positions) {
            result.collapsed_blocks.push_back(block_pos);
        }
    }

    // Queue collapse event
    CollapseEvent event;
    event.trigger_position = pos;
    event.falling_clusters = std::move(clusters);
    event.timestamp = impl_->current_time;

    impl_->pending_collapses.push(std::move(event));
    impl_->stats.total_collapses++;

    return result;
}

bool StructuralIntegritySystem::is_block_supported(const world::WorldBlockPos& pos) const {
    if (!impl_->initialized) {
        return true;  // Assume supported if not initialized
    }

    std::vector<world::WorldBlockPos> path;
    return impl_->support_graph.check_ground_connectivity(pos, path);
}

float StructuralIntegritySystem::get_support_distance(const world::WorldBlockPos& pos) const {
    if (!impl_->initialized) {
        return 0.0f;
    }

    return impl_->support_graph.get_support_distance(pos);
}

const StructuralIntegrityConfig& StructuralIntegritySystem::get_config() const {
    return impl_->config;
}

void StructuralIntegritySystem::set_config(const StructuralIntegrityConfig& config) {
    impl_->config = config;
}

void StructuralIntegritySystem::set_enabled(bool enabled) {
    impl_->enabled = enabled;
}

bool StructuralIntegritySystem::is_enabled() const {
    return impl_->enabled;
}

void StructuralIntegritySystem::set_collapse_callback(CollapseCallback callback) {
    impl_->collapse_callback = std::move(callback);
}

StructuralIntegritySystem::Stats StructuralIntegritySystem::get_stats() const {
    return impl_->stats;
}

}  // namespace realcraft::physics
