// RealCraft Physics Engine
// debris_system.cpp - Debris lifecycle management and impact damage

#include <algorithm>
#include <random>
#include <realcraft/physics/convex_hull_generator.hpp>
#include <realcraft/physics/debris_system.hpp>
#include <realcraft/physics/impact_damage.hpp>
#include <realcraft/physics/physics_world.hpp>
#include <unordered_map>

namespace realcraft::physics {

// ============================================================================
// Implementation
// ============================================================================

struct DebrisSystem::Impl {
    PhysicsWorld* physics_world = nullptr;
    world::WorldManager* world_manager = nullptr;
    DebrisConfig config;

    std::unordered_map<RigidBodyHandle, DebrisData> debris_map;
    std::mt19937 rng{std::random_device{}()};

    double current_time = 0.0;
    uint32_t spawned_this_frame = 0;

    IImpactDamageHandler* impact_handler = nullptr;
    DebrisImpactCallback impact_callback;

    Stats stats;
    bool initialized = false;
};

// ============================================================================
// Constructor / Destructor
// ============================================================================

DebrisSystem::DebrisSystem() : impl_(std::make_unique<Impl>()) {}

DebrisSystem::~DebrisSystem() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

bool DebrisSystem::initialize(PhysicsWorld* physics_world, world::WorldManager* world_manager,
                              const DebrisConfig& config) {
    if (!physics_world || !world_manager) {
        return false;
    }

    // PhysicsWorld and WorldManager must outlive the DebrisSystem
    // Caller is responsible for lifetime management
    // NOLINTNEXTLINE(codeql-cpp/local-variable-address-stored-in-non-local-memory)
    impl_->physics_world = physics_world;
    // NOLINTNEXTLINE(codeql-cpp/local-variable-address-stored-in-non-local-memory)
    impl_->world_manager = world_manager;
    impl_->config = config;
    impl_->initialized = true;
    impl_->current_time = 0.0;
    impl_->spawned_this_frame = 0;
    impl_->debris_map.clear();
    impl_->stats = Stats{};

    return true;
}

void DebrisSystem::shutdown() {
    if (!impl_->initialized) {
        return;
    }

    // Remove all debris rigid bodies
    clear_all_debris();

    impl_->physics_world = nullptr;
    impl_->world_manager = nullptr;
    impl_->initialized = false;
}

bool DebrisSystem::is_initialized() const {
    return impl_->initialized;
}

// ============================================================================
// Update
// ============================================================================

void DebrisSystem::update(double delta_time) {
    if (!impl_->initialized) {
        return;
    }

    impl_->current_time += delta_time;
    impl_->spawned_this_frame = 0;
    impl_->stats.impacts_this_frame = 0;

    std::vector<RigidBodyHandle> to_remove;
    size_t sleeping_count = 0;

    for (auto& [handle, data] : impl_->debris_map) {
        // Check lifetime
        double age = impl_->current_time - data.spawn_time;
        if (age >= data.lifetime) {
            data.marked_for_removal = true;
            to_remove.push_back(handle);
            continue;
        }

        // Check sleep state
        if (impl_->config.enable_debris_sleeping) {
            RigidBody* body = impl_->physics_world->get_rigid_body(handle);
            if (body) {
                glm::dvec3 vel = body->get_linear_velocity();
                glm::dvec3 ang_vel = body->get_angular_velocity();
                double speed = glm::length(vel) + glm::length(ang_vel) * 0.5;

                data.is_sleeping = (speed < impl_->config.debris_sleep_threshold);

                if (data.is_sleeping) {
                    ++sleeping_count;
                }
            }
        }
    }

    impl_->stats.sleeping_debris = sleeping_count;

    // Clean up sleeping debris if we're over capacity threshold
    double capacity_ratio =
        static_cast<double>(impl_->debris_map.size()) / static_cast<double>(impl_->config.max_active_debris);

    if (capacity_ratio > impl_->config.sleep_cleanup_threshold) {
        // Remove sleeping debris that's past minimum lifetime
        for (auto& [handle, data] : impl_->debris_map) {
            if (data.is_sleeping && !data.marked_for_removal) {
                double age = impl_->current_time - data.spawn_time;
                if (age >= impl_->config.minimum_lifetime_seconds) {
                    data.marked_for_removal = true;
                    to_remove.push_back(handle);

                    // Stop if we're back under threshold
                    capacity_ratio = static_cast<double>(impl_->debris_map.size() - to_remove.size()) /
                                     static_cast<double>(impl_->config.max_active_debris);
                    if (capacity_ratio <= impl_->config.sleep_cleanup_threshold) {
                        break;
                    }
                }
            }
        }
    }

    // Hard limit enforcement - remove oldest debris
    if (impl_->debris_map.size() - to_remove.size() > impl_->config.max_active_debris) {
        // Find oldest debris not already marked
        std::vector<std::pair<double, RigidBodyHandle>> by_age;
        for (const auto& [handle, data] : impl_->debris_map) {
            if (!data.marked_for_removal) {
                by_age.emplace_back(data.spawn_time, handle);
            }
        }

        std::sort(by_age.begin(), by_age.end());

        size_t to_remove_count = (impl_->debris_map.size() - to_remove.size()) - impl_->config.max_active_debris;
        for (size_t i = 0; i < to_remove_count && i < by_age.size(); ++i) {
            to_remove.push_back(by_age[i].second);
            impl_->debris_map[by_age[i].second].marked_for_removal = true;
        }
    }

    // Remove debris
    for (auto handle : to_remove) {
        remove_debris(handle);
    }
}

// ============================================================================
// Debris Spawning
// ============================================================================

std::vector<RigidBodyHandle> DebrisSystem::spawn_debris_from_cluster(const BlockCluster& cluster) {
    if (!impl_->initialized || cluster.empty()) {
        return {};
    }

    // Check spawn limit
    if (impl_->spawned_this_frame >= impl_->config.max_debris_per_frame) {
        return {};
    }

    // Fragment the cluster
    auto fragments = FragmentationAlgorithm::fragment_cluster(cluster, impl_->config.fragmentation_pattern,
                                                              impl_->config.min_fragment_blocks,
                                                              impl_->config.max_fragment_blocks, impl_->rng);

    std::vector<RigidBodyHandle> handles;
    handles.reserve(fragments.size());

    // Add small random initial velocity for visual variety
    std::uniform_real_distribution<double> vel_dist(-1.0, 1.0);

    for (const auto& fragment : fragments) {
        if (impl_->spawned_this_frame >= impl_->config.max_debris_per_frame) {
            break;
        }

        if (impl_->debris_map.size() >= impl_->config.max_active_debris) {
            break;
        }

        glm::dvec3 random_velocity(vel_dist(impl_->rng), vel_dist(impl_->rng) * 0.5, vel_dist(impl_->rng));

        auto handle = spawn_single_debris(fragment.positions, fragment.block_ids, random_velocity);
        if (handle != INVALID_RIGID_BODY) {
            handles.push_back(handle);
        }
    }

    return handles;
}

RigidBodyHandle DebrisSystem::spawn_single_debris(const std::vector<world::WorldBlockPos>& positions,
                                                  const std::vector<world::BlockId>& block_ids,
                                                  const glm::dvec3& initial_velocity) {
    if (!impl_->initialized || positions.empty()) {
        return INVALID_RIGID_BODY;
    }

    if (impl_->debris_map.size() >= impl_->config.max_active_debris) {
        return INVALID_RIGID_BODY;
    }

    // Create a temporary fragment to compute properties
    Fragment temp_fragment;
    temp_fragment.positions = positions;
    temp_fragment.block_ids = block_ids;
    temp_fragment.compute_properties();

    // Determine collision shape
    RigidBodyDesc desc;
    desc.position = temp_fragment.center_of_mass;
    desc.mass = temp_fragment.total_mass;
    desc.motion_type = MotionType::Dynamic;
    desc.collision_layer = static_cast<uint32_t>(CollisionLayer::Debris);
    desc.collision_mask = static_cast<uint32_t>(CollisionLayer::Static | CollisionLayer::Debris |
                                                CollisionLayer::Player | CollisionLayer::Entity);
    desc.linear_velocity = initial_velocity;

    // Decide between AABB and ConvexHull
    if (impl_->config.use_convex_hulls && ConvexHullGenerator::should_use_convex_hull(positions)) {
        desc.collider_type = ColliderType::ConvexHull;
        // Note: ConvexHull vertices would be passed separately if supported
        // For now, we'll use AABB as Bullet's btConvexHullShape requires
        // the actual RigidBody creation to handle vertices
        desc.collider_type = ColliderType::AABB;  // Fallback for now
        desc.half_extents = temp_fragment.bounds.half_extents();
    } else {
        desc.collider_type = ColliderType::AABB;
        desc.half_extents = temp_fragment.bounds.half_extents();
    }

    // Set physics material based on most common block type
    desc.material.friction = 0.6;
    desc.material.restitution = 0.1 + temp_fragment.average_brittleness * 0.1;  // Brittle = less bouncy
    desc.material.linear_damping = 0.1;
    desc.material.angular_damping = 0.2;

    // Create the rigid body
    RigidBodyHandle handle = impl_->physics_world->create_rigid_body(desc);
    if (handle == INVALID_RIGID_BODY) {
        return INVALID_RIGID_BODY;
    }

    // Create debris data
    DebrisData data;
    data.body_handle = handle;
    data.spawn_time = impl_->current_time;
    data.lifetime = impl_->config.default_lifetime_seconds;
    data.total_mass = temp_fragment.total_mass;
    data.block_ids = block_ids;
    data.block_count = positions.size();
    data.average_brittleness = temp_fragment.average_brittleness;
    data.last_impact_time = 0.0;
    data.impact_count = 0;
    data.is_sleeping = false;
    data.marked_for_removal = false;

    impl_->debris_map[handle] = std::move(data);
    ++impl_->spawned_this_frame;
    ++impl_->stats.total_spawned;
    impl_->stats.active_debris = impl_->debris_map.size();

    return handle;
}

// ============================================================================
// Debris Queries
// ============================================================================

const DebrisData* DebrisSystem::get_debris_data(RigidBodyHandle handle) const {
    auto it = impl_->debris_map.find(handle);
    if (it != impl_->debris_map.end()) {
        return &it->second;
    }
    return nullptr;
}

bool DebrisSystem::is_debris(RigidBodyHandle handle) const {
    return impl_->debris_map.find(handle) != impl_->debris_map.end();
}

size_t DebrisSystem::active_debris_count() const {
    return impl_->debris_map.size();
}

std::vector<RigidBodyHandle> DebrisSystem::get_all_debris_handles() const {
    std::vector<RigidBodyHandle> handles;
    handles.reserve(impl_->debris_map.size());
    for (const auto& [handle, data] : impl_->debris_map) {
        handles.push_back(handle);
    }
    return handles;
}

// ============================================================================
// Manual Debris Removal
// ============================================================================

void DebrisSystem::remove_debris(RigidBodyHandle handle) {
    auto it = impl_->debris_map.find(handle);
    if (it == impl_->debris_map.end()) {
        return;
    }

    // Destroy the rigid body
    if (impl_->physics_world) {
        impl_->physics_world->destroy_rigid_body(handle);
    }

    impl_->debris_map.erase(it);
    ++impl_->stats.total_despawned;
    impl_->stats.active_debris = impl_->debris_map.size();
}

void DebrisSystem::clear_all_debris() {
    for (const auto& [handle, data] : impl_->debris_map) {
        if (impl_->physics_world) {
            impl_->physics_world->destroy_rigid_body(handle);
        }
    }
    impl_->stats.total_despawned += impl_->debris_map.size();
    impl_->debris_map.clear();
    impl_->stats.active_debris = 0;
}

// ============================================================================
// Configuration
// ============================================================================

const DebrisConfig& DebrisSystem::get_config() const {
    return impl_->config;
}

void DebrisSystem::set_config(const DebrisConfig& config) {
    impl_->config = config;
}

// ============================================================================
// Impact Damage Handler
// ============================================================================

void DebrisSystem::set_impact_damage_handler(IImpactDamageHandler* handler) {
    impl_->impact_handler = handler;
}

IImpactDamageHandler* DebrisSystem::get_impact_damage_handler() const {
    return impl_->impact_handler;
}

// ============================================================================
// Callbacks
// ============================================================================

void DebrisSystem::set_impact_callback(DebrisImpactCallback callback) {
    impl_->impact_callback = std::move(callback);
}

// ============================================================================
// Collision Handling
// ============================================================================

void DebrisSystem::on_collision(RigidBodyHandle body_a, RigidBodyHandle body_b, const glm::dvec3& contact_point,
                                const glm::dvec3& contact_normal, double impact_velocity) {
    if (!impl_->initialized || !impl_->config.enable_impact_damage) {
        return;
    }

    // Find which body is debris
    RigidBodyHandle debris_handle = INVALID_RIGID_BODY;
    RigidBodyHandle other_handle = INVALID_RIGID_BODY;

    if (impl_->debris_map.count(body_a)) {
        debris_handle = body_a;
        other_handle = body_b;
    } else if (impl_->debris_map.count(body_b)) {
        debris_handle = body_b;
        other_handle = body_a;
    } else {
        // Neither is debris
        return;
    }

    DebrisData& debris_data = impl_->debris_map[debris_handle];

    // Check impact cooldown
    if (impl_->current_time - debris_data.last_impact_time < impl_->config.impact_cooldown_seconds) {
        return;
    }

    // Check velocity threshold
    if (impact_velocity < impl_->config.impact_damage_threshold) {
        return;
    }

    // Check mass threshold
    if (debris_data.total_mass < impl_->config.min_debris_mass_kg) {
        return;
    }

    // Calculate damage
    // Kinetic energy: E = 0.5 * m * v^2
    double kinetic_energy = 0.5 * debris_data.total_mass * impact_velocity * impact_velocity;

    // Brittleness factor - brittle debris shatters and transfers more energy
    double brittleness_factor = 1.0 + static_cast<double>(debris_data.average_brittleness);

    double damage = kinetic_energy * brittleness_factor * impl_->config.damage_velocity_multiplier;

    // Create impact event
    DebrisImpactEvent event;
    event.debris_handle = debris_handle;
    event.impact_point = contact_point;
    event.impact_normal = contact_normal;
    event.impact_velocity = impact_velocity;
    event.damage_amount = damage;

    // Determine target type
    if (other_handle == INVALID_RIGID_BODY) {
        // Hit static world geometry (chunk collider)
        event.target_type = DebrisImpactEvent::TargetType::Block;

        // Find which block was hit using raycast
        auto hit = impl_->physics_world->raycast_voxels(contact_point + contact_normal * 0.1, -contact_normal, 1.0);
        if (hit && hit->block_position) {
            event.block_position = hit->block_position;
            event.block_id = hit->block_id;
        } else {
            // Approximate block position from contact point
            world::WorldBlockPos block_pos{static_cast<int64_t>(std::floor(contact_point.x - contact_normal.x * 0.5)),
                                           static_cast<int64_t>(std::floor(contact_point.y - contact_normal.y * 0.5)),
                                           static_cast<int64_t>(std::floor(contact_point.z - contact_normal.z * 0.5))};
            event.block_position = block_pos;

            // Get block type from world
            world::ChunkPos chunk_pos = world::world_to_chunk(block_pos);
            world::Chunk* chunk = impl_->world_manager->get_chunk(chunk_pos);
            if (chunk) {
                auto local_pos = world::world_to_local(block_pos);
                event.block_id = chunk->get_block(local_pos);
            }
        }
    } else if (impl_->debris_map.count(other_handle)) {
        event.target_type = DebrisImpactEvent::TargetType::OtherDebris;
    } else {
        // Could be player or entity - for now mark as unknown
        // Phase 8 will add entity detection
        event.target_type = DebrisImpactEvent::TargetType::Unknown;
    }

    // Update debris data
    debris_data.last_impact_time = impl_->current_time;
    ++debris_data.impact_count;
    ++impl_->stats.impacts_this_frame;
    impl_->stats.total_damage_dealt += damage;

    // Handle block damage
    if (event.target_type == DebrisImpactEvent::TargetType::Block && event.block_position.has_value()) {
        if (impl_->impact_handler) {
            bool destroyed = impl_->impact_handler->on_block_impact(
                *event.block_position, event.block_id.value_or(world::BLOCK_AIR), damage, -contact_normal);

            if (destroyed) {
                ++impl_->stats.blocks_destroyed;
            }
        }
    }

    // Invoke callback
    if (impl_->impact_callback) {
        impl_->impact_callback(event);
    }
}

// ============================================================================
// Statistics
// ============================================================================

DebrisSystem::Stats DebrisSystem::get_stats() const {
    return impl_->stats;
}

void DebrisSystem::reset_stats() {
    impl_->stats = Stats{};
    impl_->stats.active_debris = impl_->debris_map.size();
}

}  // namespace realcraft::physics
