// RealCraft Physics Engine
// physics_world.cpp - Main physics world manager implementation

#include <btBulletDynamicsCommon.h>
#include <chrono>
#include <mutex>
#include <realcraft/core/logger.hpp>
#include <realcraft/physics/physics_world.hpp>
#include <realcraft/world/block.hpp>
#include <unordered_map>

namespace realcraft::physics {

// ============================================================================
// Implementation
// ============================================================================

struct PhysicsWorld::Impl {
    PhysicsConfig config;
    world::WorldManager* world_manager = nullptr;
    bool initialized = false;

    // Bullet core components
    std::unique_ptr<btDefaultCollisionConfiguration> collision_config;
    std::unique_ptr<btCollisionDispatcher> dispatcher;
    std::unique_ptr<btDbvtBroadphase> broadphase;
    std::unique_ptr<btSequentialImpulseConstraintSolver> solver;
    std::unique_ptr<btDiscreteDynamicsWorld> dynamics_world;

    // Chunk collision
    std::unordered_map<world::ChunkPos, std::unique_ptr<ChunkCollider>> chunk_colliders;
    mutable std::mutex chunk_mutex;

    // Dynamic colliders
    std::unordered_map<ColliderHandle, std::unique_ptr<Collider>> colliders;
    ColliderHandle next_handle = 1;
    mutable std::mutex collider_mutex;

    // Voxel ray caster
    VoxelRayCaster ray_caster;

    // Origin offset for coordinate conversion
    glm::dvec3 origin_offset{0.0};

    // Collision callback
    CollisionCallback collision_callback;

    // Stats
    double last_step_time_ms = 0.0;

    bool initialize_bullet() {
        // Create collision configuration
        collision_config = std::make_unique<btDefaultCollisionConfiguration>();

        // Create dispatcher
        dispatcher = std::make_unique<btCollisionDispatcher>(collision_config.get());

        // Create broadphase
        broadphase = std::make_unique<btDbvtBroadphase>();

        // Create solver
        solver = std::make_unique<btSequentialImpulseConstraintSolver>();

        // Create dynamics world
        dynamics_world = std::make_unique<btDiscreteDynamicsWorld>(dispatcher.get(), broadphase.get(), solver.get(),
                                                                   collision_config.get());

        // Set gravity
        dynamics_world->setGravity(btVector3(static_cast<btScalar>(config.gravity.x),
                                             static_cast<btScalar>(config.gravity.y),
                                             static_cast<btScalar>(config.gravity.z)));

        return true;
    }

    void shutdown_bullet() {
        // Remove all collision objects first
        {
            std::lock_guard<std::mutex> lock(chunk_mutex);
            for (auto& [pos, chunk_collider] : chunk_colliders) {
                if (dynamics_world && chunk_collider->get_collision_object()) {
                    dynamics_world->removeCollisionObject(chunk_collider->get_collision_object());
                }
            }
            chunk_colliders.clear();
        }

        {
            std::lock_guard<std::mutex> lock(collider_mutex);
            for (auto& [handle, collider] : colliders) {
                if (dynamics_world && collider->get_bullet_object()) {
                    dynamics_world->removeCollisionObject(collider->get_bullet_object());
                }
            }
            colliders.clear();
        }

        // Destroy Bullet components in reverse order
        dynamics_world.reset();
        solver.reset();
        broadphase.reset();
        dispatcher.reset();
        collision_config.reset();
    }

    ColliderHandle allocate_handle() { return next_handle++; }
};

// ============================================================================
// PhysicsWorld Public API
// ============================================================================

PhysicsWorld::PhysicsWorld() : impl_(std::make_unique<Impl>()) {}

PhysicsWorld::~PhysicsWorld() {
    shutdown();
}

bool PhysicsWorld::initialize(world::WorldManager* world_manager, const PhysicsConfig& config) {
    if (impl_->initialized) {
        REALCRAFT_LOG_WARN(core::log_category::PHYSICS, "PhysicsWorld already initialized");
        return false;
    }

    impl_->config = config;
    // The WorldManager must outlive the PhysicsWorld - caller is responsible for lifetime management
    // NOLINTNEXTLINE(codeql-cpp/local-variable-address-stored-in-non-local-memory)
    impl_->world_manager = world_manager;

    if (!impl_->initialize_bullet()) {
        REALCRAFT_LOG_ERROR(core::log_category::PHYSICS, "Failed to initialize Bullet Physics");
        return false;
    }

    // Setup ray caster
    impl_->ray_caster.set_world_manager(world_manager);

    // Register as world observer
    if (world_manager) {
        world_manager->add_observer(this);
    }

    impl_->initialized = true;
    REALCRAFT_LOG_INFO(core::log_category::PHYSICS, "PhysicsWorld initialized");

    return true;
}

void PhysicsWorld::shutdown() {
    if (!impl_->initialized) {
        return;
    }

    // Unregister from world manager
    if (impl_->world_manager) {
        impl_->world_manager->remove_observer(this);
    }

    impl_->shutdown_bullet();
    impl_->world_manager = nullptr;
    impl_->initialized = false;

    REALCRAFT_LOG_INFO(core::log_category::PHYSICS, "PhysicsWorld shutdown");
}

bool PhysicsWorld::is_initialized() const {
    return impl_->initialized;
}

void PhysicsWorld::fixed_update([[maybe_unused]] double fixed_delta) {
    if (!impl_->initialized || !impl_->dynamics_world) {
        return;
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Update collider transforms in Bullet world
    {
        std::lock_guard<std::mutex> lock(impl_->collider_mutex);
        for (auto& [handle, collider] : impl_->colliders) {
            auto* bullet_obj = collider->get_bullet_object();
            if (bullet_obj) {
                // Transform already updated via set_position/set_rotation on collider
            }
        }
    }

    // Perform collision detection (no simulation step for collision-only phase)
    impl_->dynamics_world->performDiscreteCollisionDetection();

    // Process collision callbacks
    if (impl_->collision_callback) {
        int num_manifolds = impl_->dispatcher->getNumManifolds();
        for (int i = 0; i < num_manifolds; ++i) {
            btPersistentManifold* manifold = impl_->dispatcher->getManifoldByIndexInternal(i);
            if (!manifold) {
                continue;
            }

            const btCollisionObject* obj_a = manifold->getBody0();
            const btCollisionObject* obj_b = manifold->getBody1();

            for (int j = 0; j < manifold->getNumContacts(); ++j) {
                const btManifoldPoint& pt = manifold->getContactPoint(j);
                if (pt.getDistance() < 0.0f) {
                    CollisionEvent event;
                    event.collider_a = static_cast<ColliderHandle>(obj_a->getUserIndex());
                    event.collider_b = static_cast<ColliderHandle>(obj_b->getUserIndex());
                    event.contact_point = glm::dvec3(pt.getPositionWorldOnA().getX(), pt.getPositionWorldOnA().getY(),
                                                     pt.getPositionWorldOnA().getZ());
                    event.contact_normal =
                        glm::dvec3(pt.m_normalWorldOnB.getX(), pt.m_normalWorldOnB.getY(), pt.m_normalWorldOnB.getZ());
                    event.penetration_depth = static_cast<double>(-pt.getDistance());

                    impl_->collision_callback(event);
                }
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    impl_->last_step_time_ms = std::chrono::duration<double, std::milli>(end - start).count();
}

// ============================================================================
// Collider Management
// ============================================================================

ColliderHandle PhysicsWorld::create_aabb_collider(const AABBColliderDesc& desc) {
    if (!impl_->initialized) {
        return INVALID_COLLIDER;
    }

    std::lock_guard<std::mutex> lock(impl_->collider_mutex);

    ColliderHandle handle = impl_->allocate_handle();
    auto collider = std::make_unique<AABBCollider>(handle, desc);

    // Add to Bullet world
    impl_->dynamics_world->addCollisionObject(collider->get_bullet_object());

    impl_->colliders[handle] = std::move(collider);

    REALCRAFT_LOG_DEBUG(core::log_category::PHYSICS, "Created AABB collider {}", handle);

    return handle;
}

ColliderHandle PhysicsWorld::create_capsule_collider(const CapsuleColliderDesc& desc) {
    if (!impl_->initialized) {
        return INVALID_COLLIDER;
    }

    std::lock_guard<std::mutex> lock(impl_->collider_mutex);

    ColliderHandle handle = impl_->allocate_handle();
    auto collider = std::make_unique<CapsuleCollider>(handle, desc);

    impl_->dynamics_world->addCollisionObject(collider->get_bullet_object());

    impl_->colliders[handle] = std::move(collider);

    REALCRAFT_LOG_DEBUG(core::log_category::PHYSICS, "Created Capsule collider {}", handle);

    return handle;
}

ColliderHandle PhysicsWorld::create_convex_hull_collider(const ConvexHullColliderDesc& desc) {
    if (!impl_->initialized) {
        return INVALID_COLLIDER;
    }

    std::lock_guard<std::mutex> lock(impl_->collider_mutex);

    ColliderHandle handle = impl_->allocate_handle();
    auto collider = std::make_unique<ConvexHullCollider>(handle, desc);

    impl_->dynamics_world->addCollisionObject(collider->get_bullet_object());

    impl_->colliders[handle] = std::move(collider);

    REALCRAFT_LOG_DEBUG(core::log_category::PHYSICS, "Created ConvexHull collider {}", handle);

    return handle;
}

void PhysicsWorld::destroy_collider(ColliderHandle handle) {
    if (!impl_->initialized || handle == INVALID_COLLIDER) {
        return;
    }

    std::lock_guard<std::mutex> lock(impl_->collider_mutex);

    auto it = impl_->colliders.find(handle);
    if (it != impl_->colliders.end()) {
        impl_->dynamics_world->removeCollisionObject(it->second->get_bullet_object());
        impl_->colliders.erase(it);
        REALCRAFT_LOG_DEBUG(core::log_category::PHYSICS, "Destroyed collider {}", handle);
    }
}

Collider* PhysicsWorld::get_collider(ColliderHandle handle) {
    std::lock_guard<std::mutex> lock(impl_->collider_mutex);
    auto it = impl_->colliders.find(handle);
    return (it != impl_->colliders.end()) ? it->second.get() : nullptr;
}

const Collider* PhysicsWorld::get_collider(ColliderHandle handle) const {
    std::lock_guard<std::mutex> lock(impl_->collider_mutex);
    auto it = impl_->colliders.find(handle);
    return (it != impl_->colliders.end()) ? it->second.get() : nullptr;
}

// ============================================================================
// Ray Casting
// ============================================================================

std::optional<RayHit> PhysicsWorld::raycast_voxels(const glm::dvec3& origin, const glm::dvec3& direction,
                                                   double max_distance) const {
    Ray ray(origin, direction);
    return impl_->ray_caster.cast(ray, max_distance);
}

std::optional<RayHit> PhysicsWorld::raycast_colliders(const glm::dvec3& origin, const glm::dvec3& direction,
                                                      double max_distance, uint32_t layer_mask) const {
    if (!impl_->initialized || !impl_->dynamics_world) {
        return std::nullopt;
    }

    glm::dvec3 norm_dir = glm::normalize(direction);
    btVector3 from(static_cast<btScalar>(origin.x), static_cast<btScalar>(origin.y), static_cast<btScalar>(origin.z));
    btVector3 to = from + btVector3(static_cast<btScalar>(norm_dir.x), static_cast<btScalar>(norm_dir.y),
                                    static_cast<btScalar>(norm_dir.z)) *
                              static_cast<btScalar>(max_distance);

    btCollisionWorld::ClosestRayResultCallback callback(from, to);

    // Filter by layer mask
    callback.m_collisionFilterMask = static_cast<short>(layer_mask);

    impl_->dynamics_world->rayTest(from, to, callback);

    if (callback.hasHit()) {
        RayHit hit;
        hit.position = glm::dvec3(callback.m_hitPointWorld.getX(), callback.m_hitPointWorld.getY(),
                                  callback.m_hitPointWorld.getZ());
        hit.normal = glm::dvec3(callback.m_hitNormalWorld.getX(), callback.m_hitNormalWorld.getY(),
                                callback.m_hitNormalWorld.getZ());
        hit.distance = glm::length(hit.position - origin);

        // Get collider handle from user index
        if (callback.m_collisionObject) {
            hit.collider = static_cast<ColliderHandle>(callback.m_collisionObject->getUserIndex());
        }

        return hit;
    }

    return std::nullopt;
}

std::optional<RayHit> PhysicsWorld::raycast(const glm::dvec3& origin, const glm::dvec3& direction, double max_distance,
                                            uint32_t layer_mask) const {
    auto voxel_hit = raycast_voxels(origin, direction, max_distance);
    auto collider_hit = raycast_colliders(origin, direction, max_distance, layer_mask);

    if (voxel_hit && collider_hit) {
        // Return closest hit
        return (voxel_hit->distance < collider_hit->distance) ? voxel_hit : collider_hit;
    } else if (voxel_hit) {
        return voxel_hit;
    } else if (collider_hit) {
        return collider_hit;
    }

    return std::nullopt;
}

// ============================================================================
// Collision Queries
// ============================================================================

bool PhysicsWorld::point_in_solid(const glm::dvec3& point) const {
    if (!impl_->world_manager) {
        return false;
    }

    world::WorldBlockPos block_pos = world::world_to_block(point);
    world::BlockId block_id = impl_->world_manager->get_block(block_pos);

    if (block_id == world::BLOCK_AIR || block_id == world::BLOCK_INVALID) {
        return false;
    }

    const world::BlockType* block_type = world::BlockRegistry::instance().get(block_id);
    return block_type && block_type->has_collision();
}

std::vector<ColliderHandle> PhysicsWorld::query_aabb(const AABB& aabb, uint32_t layer_mask) const {
    std::vector<ColliderHandle> results;

    std::lock_guard<std::mutex> lock(impl_->collider_mutex);
    for (const auto& [handle, collider] : impl_->colliders) {
        if ((collider->get_collision_layer() & layer_mask) != 0) {
            if (aabb.intersects(collider->get_aabb())) {
                results.push_back(handle);
            }
        }
    }

    return results;
}

std::vector<ColliderHandle> PhysicsWorld::query_sphere(const glm::dvec3& center, double radius,
                                                       uint32_t layer_mask) const {
    std::vector<ColliderHandle> results;

    AABB sphere_aabb = AABB::from_center_extents(center, glm::dvec3(radius));

    std::lock_guard<std::mutex> lock(impl_->collider_mutex);
    for (const auto& [handle, collider] : impl_->colliders) {
        if ((collider->get_collision_layer() & layer_mask) != 0) {
            AABB collider_aabb = collider->get_aabb();
            if (sphere_aabb.intersects(collider_aabb)) {
                // Rough check passed, could add more precise sphere test here
                results.push_back(handle);
            }
        }
    }

    return results;
}

bool PhysicsWorld::has_line_of_sight(const glm::dvec3& from, const glm::dvec3& to) const {
    return impl_->ray_caster.has_line_of_sight(from, to);
}

// ============================================================================
// Collision Callbacks
// ============================================================================

void PhysicsWorld::set_collision_callback(CollisionCallback callback) {
    impl_->collision_callback = std::move(callback);
}

// ============================================================================
// IWorldObserver Implementation
// ============================================================================

void PhysicsWorld::on_chunk_loaded(const world::ChunkPos& pos, world::Chunk& chunk) {
    std::lock_guard<std::mutex> lock(impl_->chunk_mutex);

    auto collider = std::make_unique<ChunkCollider>(pos, chunk);
    collider->set_world_offset(impl_->origin_offset);

    if (!collider->is_empty() && impl_->dynamics_world) {
        impl_->dynamics_world->addCollisionObject(collider->get_collision_object());
    }

    impl_->chunk_colliders[pos] = std::move(collider);

    REALCRAFT_LOG_DEBUG(core::log_category::PHYSICS, "Loaded chunk collision: ({}, {})", pos.x, pos.y);
}

void PhysicsWorld::on_chunk_unloading(const world::ChunkPos& pos, const world::Chunk& chunk) {
    (void)chunk;

    std::lock_guard<std::mutex> lock(impl_->chunk_mutex);

    auto it = impl_->chunk_colliders.find(pos);
    if (it != impl_->chunk_colliders.end()) {
        if (impl_->dynamics_world && it->second->get_collision_object()) {
            impl_->dynamics_world->removeCollisionObject(it->second->get_collision_object());
        }
        impl_->chunk_colliders.erase(it);
        REALCRAFT_LOG_DEBUG(core::log_category::PHYSICS, "Unloaded chunk collision: ({}, {})", pos.x, pos.y);
    }
}

void PhysicsWorld::on_block_changed(const world::BlockChangeEvent& event) {
    // Skip changes from world generation
    if (event.from_generation) {
        return;
    }

    world::ChunkPos chunk_pos = world::world_to_chunk(event.position);
    world::LocalBlockPos local_pos = world::world_to_local(event.position);

    std::lock_guard<std::mutex> lock(impl_->chunk_mutex);

    auto it = impl_->chunk_colliders.find(chunk_pos);
    if (it != impl_->chunk_colliders.end()) {
        bool success = it->second->update_block(local_pos, event.old_entry.block_id, event.new_entry.block_id);

        if (!success) {
            // Incremental update failed, need full rebuild
            // Get chunk from world manager
            if (impl_->world_manager) {
                world::Chunk* chunk = impl_->world_manager->get_chunk(chunk_pos);
                if (chunk) {
                    // Remove old from Bullet world
                    if (impl_->dynamics_world && it->second->get_collision_object()) {
                        impl_->dynamics_world->removeCollisionObject(it->second->get_collision_object());
                    }

                    // Rebuild
                    it->second->rebuild(*chunk);
                    it->second->set_world_offset(impl_->origin_offset);

                    // Re-add to Bullet world
                    if (impl_->dynamics_world && !it->second->is_empty()) {
                        impl_->dynamics_world->addCollisionObject(it->second->get_collision_object());
                    }
                }
            }
        }
    }
}

void PhysicsWorld::on_origin_shifted(const world::WorldBlockPos& old_origin, const world::WorldBlockPos& new_origin) {
    glm::dvec3 delta(static_cast<double>(new_origin.x - old_origin.x), static_cast<double>(new_origin.y - old_origin.y),
                     static_cast<double>(new_origin.z - old_origin.z));

    impl_->origin_offset += delta;

    // Update all chunk collider transforms
    std::lock_guard<std::mutex> lock(impl_->chunk_mutex);
    for (auto& [pos, collider] : impl_->chunk_colliders) {
        collider->set_world_offset(impl_->origin_offset);
    }

    REALCRAFT_LOG_DEBUG(core::log_category::PHYSICS, "Origin shifted, new offset: ({}, {}, {})", impl_->origin_offset.x,
                        impl_->origin_offset.y, impl_->origin_offset.z);
}

// ============================================================================
// Configuration
// ============================================================================

const PhysicsConfig& PhysicsWorld::get_config() const {
    return impl_->config;
}

void PhysicsWorld::set_gravity(const glm::dvec3& gravity) {
    impl_->config.gravity = gravity;
    if (impl_->dynamics_world) {
        impl_->dynamics_world->setGravity(btVector3(static_cast<btScalar>(gravity.x), static_cast<btScalar>(gravity.y),
                                                    static_cast<btScalar>(gravity.z)));
    }
}

glm::dvec3 PhysicsWorld::get_gravity() const {
    return impl_->config.gravity;
}

// ============================================================================
// Debug / Statistics
// ============================================================================

PhysicsWorld::DebugStats PhysicsWorld::get_stats() const {
    DebugStats stats;

    {
        std::lock_guard<std::mutex> lock(impl_->collider_mutex);
        stats.active_colliders = impl_->colliders.size();
    }

    {
        std::lock_guard<std::mutex> lock(impl_->chunk_mutex);
        stats.chunk_colliders = impl_->chunk_colliders.size();

        size_t total_shapes = 0;
        for (const auto& [pos, collider] : impl_->chunk_colliders) {
            total_shapes += collider->get_block_count();
        }
        stats.total_collision_shapes = total_shapes;
    }

    stats.last_step_time_ms = impl_->last_step_time_ms;

    return stats;
}

}  // namespace realcraft::physics
