// RealCraft Physics Engine
// physics_world.cpp - Main physics world manager implementation

#include <btBulletDynamicsCommon.h>
#include <chrono>
#include <mutex>
#include <realcraft/core/logger.hpp>
#include <realcraft/physics/impact_damage.hpp>
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

    // Dynamic colliders (collision-only objects)
    std::unordered_map<ColliderHandle, std::unique_ptr<Collider>> colliders;
    ColliderHandle next_handle = 1;
    mutable std::mutex collider_mutex;

    // Rigid bodies (simulated physics objects)
    std::unordered_map<RigidBodyHandle, std::unique_ptr<RigidBody>> rigid_bodies;
    RigidBodyHandle next_rigid_body_handle = 1;
    mutable std::mutex rigid_body_mutex;

    // Voxel ray caster
    VoxelRayCaster ray_caster;

    // Origin offset for coordinate conversion
    glm::dvec3 origin_offset{0.0};

    // Collision callback
    CollisionCallback collision_callback;

    // Interpolation alpha for rendering
    double interpolation_alpha = 0.0;

    // Stats
    double last_step_time_ms = 0.0;

    // Structural integrity system
    std::unique_ptr<StructuralIntegritySystem> structural_integrity;

    // Debris system
    std::unique_ptr<DebrisSystem> debris_system;

    // Fluid simulation system
    std::unique_ptr<FluidSimulation> fluid_simulation;

    // Impact damage handler
    std::unique_ptr<DefaultImpactDamageHandler> impact_handler;

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

        // Remove all rigid bodies
        {
            std::lock_guard<std::mutex> lock(rigid_body_mutex);
            for (auto& [handle, body] : rigid_bodies) {
                if (dynamics_world && body->get_bullet_body()) {
                    dynamics_world->removeRigidBody(body->get_bullet_body());
                }
            }
            rigid_bodies.clear();
        }

        // Destroy Bullet components in reverse order
        dynamics_world.reset();
        solver.reset();
        broadphase.reset();
        dispatcher.reset();
        collision_config.reset();
    }

    ColliderHandle allocate_handle() { return next_handle++; }
    RigidBodyHandle allocate_rigid_body_handle() { return next_rigid_body_handle++; }
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

    // Initialize structural integrity system
    impl_->structural_integrity = std::make_unique<StructuralIntegritySystem>();
    StructuralIntegrityConfig integrity_config;
    impl_->structural_integrity->initialize(this, world_manager, integrity_config);

    // Register structural integrity as world observer
    if (world_manager) {
        world_manager->add_observer(impl_->structural_integrity.get());
    }

    // Initialize debris system
    impl_->debris_system = std::make_unique<DebrisSystem>();
    DebrisConfig debris_config;
    impl_->debris_system->initialize(this, world_manager, debris_config);

    // Initialize default impact damage handler
    impl_->impact_handler = std::make_unique<DefaultImpactDamageHandler>(world_manager);
    impl_->debris_system->set_impact_damage_handler(impl_->impact_handler.get());

    // Initialize fluid simulation system
    impl_->fluid_simulation = std::make_unique<FluidSimulation>();
    FluidSimulationConfig fluid_config;
    impl_->fluid_simulation->initialize(this, world_manager, fluid_config);

    // Register fluid simulation as world observer
    if (world_manager) {
        world_manager->add_observer(impl_->fluid_simulation.get());
    }

    impl_->initialized = true;
    REALCRAFT_LOG_INFO(core::log_category::PHYSICS, "PhysicsWorld initialized");

    return true;
}

void PhysicsWorld::shutdown() {
    if (!impl_->initialized) {
        return;
    }

    // Shutdown fluid simulation system
    if (impl_->fluid_simulation) {
        if (impl_->world_manager) {
            impl_->world_manager->remove_observer(impl_->fluid_simulation.get());
        }
        impl_->fluid_simulation->shutdown();
        impl_->fluid_simulation.reset();
    }

    // Shutdown debris system
    if (impl_->debris_system) {
        impl_->debris_system->shutdown();
        impl_->debris_system.reset();
    }

    // Shutdown impact handler
    impl_->impact_handler.reset();

    // Shutdown structural integrity system
    if (impl_->structural_integrity) {
        if (impl_->world_manager) {
            impl_->world_manager->remove_observer(impl_->structural_integrity.get());
        }
        impl_->structural_integrity->shutdown();
        impl_->structural_integrity.reset();
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

void PhysicsWorld::fixed_update(double fixed_delta) {
    if (!impl_->initialized || !impl_->dynamics_world) {
        return;
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Store previous transforms for all rigid bodies (for interpolation)
    {
        std::lock_guard<std::mutex> lock(impl_->rigid_body_mutex);
        for (auto& [handle, body] : impl_->rigid_bodies) {
            body->store_previous_transform();
        }
    }

    // Step the physics simulation
    // This applies gravity, forces, collision response, and constraint solving
    impl_->dynamics_world->stepSimulation(static_cast<btScalar>(fixed_delta), impl_->config.max_substeps,
                                          static_cast<btScalar>(impl_->config.fixed_timestep));

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

    // Update fluid simulation system
    if (impl_->fluid_simulation && impl_->fluid_simulation->is_enabled()) {
        impl_->fluid_simulation->update(fixed_delta);

        // Apply buoyancy and drag forces to all dynamic rigid bodies
        std::lock_guard<std::mutex> lock(impl_->rigid_body_mutex);
        for (auto& [handle, body] : impl_->rigid_bodies) {
            if (body->get_motion_type() != MotionType::Dynamic) {
                continue;
            }

            // Calculate and apply buoyancy force
            glm::dvec3 buoyancy = impl_->fluid_simulation->calculate_buoyancy_force(*body);
            if (glm::length(buoyancy) > 0.01) {
                body->apply_central_force(buoyancy);
            }

            // Calculate and apply drag force for submerged objects
            glm::dvec3 velocity = body->get_linear_velocity();
            if (glm::length(velocity) > 0.01) {
                AABB bounds = body->get_aabb();
                double cross_section = bounds.half_extents().x * bounds.half_extents().z * 4.0;
                glm::dvec3 drag =
                    impl_->fluid_simulation->calculate_drag_force(velocity, cross_section, body->get_position());
                if (glm::length(drag) > 0.01) {
                    body->apply_central_force(drag);
                }
            }

            // Apply current push force
            double submerged = impl_->fluid_simulation->get_submerged_fraction(body->get_aabb());
            if (submerged > 0.1) {
                AABB bounds = body->get_aabb();
                double cross_section = bounds.half_extents().x * bounds.half_extents().z * 4.0;
                glm::dvec3 current =
                    impl_->fluid_simulation->calculate_current_force(body->get_position(), cross_section * submerged);
                if (glm::length(current) > 0.01) {
                    body->apply_central_force(current);
                }
            }
        }
    }

    // Update structural integrity system
    if (impl_->structural_integrity) {
        impl_->structural_integrity->update(fixed_delta);
    }

    // Update debris system
    if (impl_->debris_system) {
        impl_->debris_system->update(fixed_delta);
    }

    // Process debris collisions for impact damage
    if (impl_->debris_system) {
        int num_manifolds = impl_->dispatcher->getNumManifolds();
        for (int i = 0; i < num_manifolds; ++i) {
            btPersistentManifold* manifold = impl_->dispatcher->getManifoldByIndexInternal(i);
            if (!manifold) {
                continue;
            }

            const btCollisionObject* obj_a = manifold->getBody0();
            const btCollisionObject* obj_b = manifold->getBody1();

            // Get rigid body handles
            RigidBodyHandle handle_a = static_cast<RigidBodyHandle>(obj_a->getUserIndex());
            RigidBodyHandle handle_b = static_cast<RigidBodyHandle>(obj_b->getUserIndex());

            // Check if either is debris
            bool a_is_debris = impl_->debris_system->is_debris(handle_a);
            bool b_is_debris = impl_->debris_system->is_debris(handle_b);

            if (!a_is_debris && !b_is_debris) {
                continue;
            }

            for (int j = 0; j < manifold->getNumContacts(); ++j) {
                const btManifoldPoint& pt = manifold->getContactPoint(j);
                if (pt.getDistance() < 0.0f) {
                    glm::dvec3 contact_point(pt.getPositionWorldOnA().getX(), pt.getPositionWorldOnA().getY(),
                                             pt.getPositionWorldOnA().getZ());
                    glm::dvec3 contact_normal(pt.m_normalWorldOnB.getX(), pt.m_normalWorldOnB.getY(),
                                              pt.m_normalWorldOnB.getZ());

                    // Calculate impact velocity
                    double impact_velocity = 0.0;
                    const btRigidBody* body_a = btRigidBody::upcast(obj_a);
                    const btRigidBody* body_b = btRigidBody::upcast(obj_b);

                    if (body_a && body_b) {
                        btVector3 rel_vel = body_a->getLinearVelocity() - body_b->getLinearVelocity();
                        impact_velocity = static_cast<double>(std::abs(rel_vel.dot(
                            btVector3(static_cast<btScalar>(contact_normal.x), static_cast<btScalar>(contact_normal.y),
                                      static_cast<btScalar>(contact_normal.z)))));
                    } else if (body_a) {
                        impact_velocity = static_cast<double>(std::abs(body_a->getLinearVelocity().dot(
                            btVector3(static_cast<btScalar>(contact_normal.x), static_cast<btScalar>(contact_normal.y),
                                      static_cast<btScalar>(contact_normal.z)))));
                    } else if (body_b) {
                        impact_velocity = static_cast<double>(std::abs(body_b->getLinearVelocity().dot(
                            btVector3(static_cast<btScalar>(contact_normal.x), static_cast<btScalar>(contact_normal.y),
                                      static_cast<btScalar>(contact_normal.z)))));
                    }

                    impl_->debris_system->on_collision(handle_a, handle_b, contact_point, contact_normal,
                                                       impact_velocity);
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
// Rigid Body Management
// ============================================================================

RigidBodyHandle PhysicsWorld::create_rigid_body(const RigidBodyDesc& desc) {
    if (!impl_->initialized) {
        return INVALID_RIGID_BODY;
    }

    std::lock_guard<std::mutex> lock(impl_->rigid_body_mutex);

    RigidBodyHandle handle = impl_->allocate_rigid_body_handle();
    auto body = std::make_unique<RigidBody>(handle, desc);

    // Add to Bullet world
    impl_->dynamics_world->addRigidBody(body->get_bullet_body(), static_cast<int>(desc.collision_layer),
                                        static_cast<int>(desc.collision_mask));

    impl_->rigid_bodies[handle] = std::move(body);

    REALCRAFT_LOG_DEBUG(core::log_category::PHYSICS, "Created rigid body {}", handle);

    return handle;
}

void PhysicsWorld::destroy_rigid_body(RigidBodyHandle handle) {
    if (!impl_->initialized || handle == INVALID_RIGID_BODY) {
        return;
    }

    std::lock_guard<std::mutex> lock(impl_->rigid_body_mutex);

    auto it = impl_->rigid_bodies.find(handle);
    if (it != impl_->rigid_bodies.end()) {
        impl_->dynamics_world->removeRigidBody(it->second->get_bullet_body());
        impl_->rigid_bodies.erase(it);
        REALCRAFT_LOG_DEBUG(core::log_category::PHYSICS, "Destroyed rigid body {}", handle);
    }
}

RigidBody* PhysicsWorld::get_rigid_body(RigidBodyHandle handle) {
    std::lock_guard<std::mutex> lock(impl_->rigid_body_mutex);
    auto it = impl_->rigid_bodies.find(handle);
    return (it != impl_->rigid_bodies.end()) ? it->second.get() : nullptr;
}

const RigidBody* PhysicsWorld::get_rigid_body(RigidBodyHandle handle) const {
    std::lock_guard<std::mutex> lock(impl_->rigid_body_mutex);
    auto it = impl_->rigid_bodies.find(handle);
    return (it != impl_->rigid_bodies.end()) ? it->second.get() : nullptr;
}

// ============================================================================
// Interpolation
// ============================================================================

void PhysicsWorld::set_interpolation_alpha(double alpha) {
    impl_->interpolation_alpha = alpha;
}

double PhysicsWorld::get_interpolation_alpha() const {
    return impl_->interpolation_alpha;
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
    {
        std::lock_guard<std::mutex> lock(impl_->chunk_mutex);
        for (auto& [pos, collider] : impl_->chunk_colliders) {
            collider->set_world_offset(impl_->origin_offset);
        }
    }

    // Update all rigid body positions
    {
        std::lock_guard<std::mutex> lock(impl_->rigid_body_mutex);
        for (auto& [handle, body] : impl_->rigid_bodies) {
            body->apply_origin_shift(delta);
        }
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
        std::lock_guard<std::mutex> lock(impl_->rigid_body_mutex);
        stats.active_rigid_bodies = impl_->rigid_bodies.size();
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

// ============================================================================
// Structural Integrity
// ============================================================================

StructuralIntegritySystem* PhysicsWorld::get_structural_integrity() {
    return impl_->structural_integrity.get();
}

const StructuralIntegritySystem* PhysicsWorld::get_structural_integrity() const {
    return impl_->structural_integrity.get();
}

// ============================================================================
// Debris System
// ============================================================================

DebrisSystem* PhysicsWorld::get_debris_system() {
    return impl_->debris_system.get();
}

const DebrisSystem* PhysicsWorld::get_debris_system() const {
    return impl_->debris_system.get();
}

// ============================================================================
// Fluid Simulation
// ============================================================================

FluidSimulation* PhysicsWorld::get_fluid_simulation() {
    return impl_->fluid_simulation.get();
}

const FluidSimulation* PhysicsWorld::get_fluid_simulation() const {
    return impl_->fluid_simulation.get();
}

}  // namespace realcraft::physics
