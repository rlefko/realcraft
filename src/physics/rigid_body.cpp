// RealCraft Physics Engine
// rigid_body.cpp - Rigid body dynamics implementation

#include <btBulletDynamicsCommon.h>
#include <realcraft/physics/rigid_body.hpp>

namespace realcraft::physics {

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

btTransform glm_to_bullet_transform(const glm::dvec3& position, const glm::dquat& rotation) {
    btTransform transform;
    transform.setOrigin(btVector3(static_cast<btScalar>(position.x), static_cast<btScalar>(position.y),
                                  static_cast<btScalar>(position.z)));
    transform.setRotation(btQuaternion(static_cast<btScalar>(rotation.x), static_cast<btScalar>(rotation.y),
                                       static_cast<btScalar>(rotation.z), static_cast<btScalar>(rotation.w)));
    return transform;
}

glm::dvec3 bullet_to_glm_position(const btVector3& v) {
    return glm::dvec3(static_cast<double>(v.getX()), static_cast<double>(v.getY()), static_cast<double>(v.getZ()));
}

glm::dquat bullet_to_glm_rotation(const btQuaternion& q) {
    return glm::dquat(static_cast<double>(q.getW()), static_cast<double>(q.getX()), static_cast<double>(q.getY()),
                      static_cast<double>(q.getZ()));
}

btVector3 glm_to_bullet_vec(const glm::dvec3& v) {
    return btVector3(static_cast<btScalar>(v.x), static_cast<btScalar>(v.y), static_cast<btScalar>(v.z));
}

}  // namespace

// ============================================================================
// RigidBody Implementation
// ============================================================================

RigidBody::RigidBody(RigidBodyHandle handle, const RigidBodyDesc& desc)
    : handle_(handle), motion_type_(desc.motion_type), collider_type_(desc.collider_type),
      mass_(desc.motion_type == MotionType::Static ? 0.0 : desc.mass), gravity_scale_(desc.gravity_scale),
      material_(desc.material), collision_layer_(desc.collision_layer), collision_mask_(desc.collision_mask),
      user_data_(desc.user_data), previous_position_(desc.position), previous_rotation_(desc.rotation) {
    create_shape(desc);
    create_rigid_body(desc);
}

RigidBody::~RigidBody() {
    // Bullet objects are destroyed via unique_ptr in reverse order
    rigid_body_.reset();
    motion_state_.reset();
    shape_.reset();
}

RigidBody::RigidBody(RigidBody&& other) noexcept
    : handle_(other.handle_), motion_type_(other.motion_type_), collider_type_(other.collider_type_),
      mass_(other.mass_), gravity_scale_(other.gravity_scale_), material_(other.material_),
      collision_layer_(other.collision_layer_), collision_mask_(other.collision_mask_), user_data_(other.user_data_),
      previous_position_(other.previous_position_), previous_rotation_(other.previous_rotation_),
      shape_(std::move(other.shape_)), motion_state_(std::move(other.motion_state_)),
      rigid_body_(std::move(other.rigid_body_)), base_gravity_(other.base_gravity_) {
    other.handle_ = INVALID_RIGID_BODY;
}

RigidBody& RigidBody::operator=(RigidBody&& other) noexcept {
    if (this != &other) {
        rigid_body_.reset();
        motion_state_.reset();
        shape_.reset();

        handle_ = other.handle_;
        motion_type_ = other.motion_type_;
        collider_type_ = other.collider_type_;
        mass_ = other.mass_;
        gravity_scale_ = other.gravity_scale_;
        material_ = other.material_;
        collision_layer_ = other.collision_layer_;
        collision_mask_ = other.collision_mask_;
        user_data_ = other.user_data_;
        previous_position_ = other.previous_position_;
        previous_rotation_ = other.previous_rotation_;
        shape_ = std::move(other.shape_);
        motion_state_ = std::move(other.motion_state_);
        rigid_body_ = std::move(other.rigid_body_);
        base_gravity_ = other.base_gravity_;

        other.handle_ = INVALID_RIGID_BODY;
    }
    return *this;
}

void RigidBody::create_shape(const RigidBodyDesc& desc) {
    switch (desc.collider_type) {
        case ColliderType::AABB: {
            btVector3 bt_half_extents(static_cast<btScalar>(desc.half_extents.x),
                                      static_cast<btScalar>(desc.half_extents.y),
                                      static_cast<btScalar>(desc.half_extents.z));
            shape_ = std::make_unique<btBoxShape>(bt_half_extents);
            break;
        }
        case ColliderType::Capsule: {
            double cylinder_height = desc.height - 2.0 * desc.radius;
            if (cylinder_height < 0.0) {
                cylinder_height = 0.0;
            }
            shape_ = std::make_unique<btCapsuleShape>(static_cast<btScalar>(desc.radius),
                                                      static_cast<btScalar>(cylinder_height));
            break;
        }
        case ColliderType::ConvexHull: {
            // For ConvexHull without explicit vertices, create a box shape as fallback
            btVector3 bt_half_extents(static_cast<btScalar>(desc.half_extents.x),
                                      static_cast<btScalar>(desc.half_extents.y),
                                      static_cast<btScalar>(desc.half_extents.z));
            shape_ = std::make_unique<btBoxShape>(bt_half_extents);
            break;
        }
    }
}

void RigidBody::create_rigid_body(const RigidBodyDesc& desc) {
    // Create default motion state
    btTransform start_transform = glm_to_bullet_transform(desc.position, desc.rotation);
    motion_state_ = std::make_unique<btDefaultMotionState>(start_transform);

    // Calculate local inertia for dynamic bodies
    btVector3 local_inertia(0, 0, 0);
    btScalar bt_mass = static_cast<btScalar>(mass_);

    if (motion_type_ == MotionType::Dynamic && mass_ > 0.0) {
        shape_->calculateLocalInertia(bt_mass, local_inertia);
    }

    // Create rigid body construction info
    btRigidBody::btRigidBodyConstructionInfo rb_info(bt_mass, motion_state_.get(), shape_.get(), local_inertia);

    // Set material properties
    rb_info.m_friction = static_cast<btScalar>(material_.friction);
    rb_info.m_restitution = static_cast<btScalar>(material_.restitution);
    rb_info.m_linearDamping = static_cast<btScalar>(material_.linear_damping);
    rb_info.m_angularDamping = static_cast<btScalar>(material_.angular_damping);

    // Create rigid body
    rigid_body_ = std::make_unique<btRigidBody>(rb_info);

    // Set collision flags based on motion type
    switch (motion_type_) {
        case MotionType::Dynamic:
            // Dynamic bodies are the default, no special flags needed
            break;
        case MotionType::Kinematic:
            rigid_body_->setCollisionFlags(rigid_body_->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
            rigid_body_->setActivationState(DISABLE_DEACTIVATION);
            break;
        case MotionType::Static:
            rigid_body_->setCollisionFlags(rigid_body_->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
            break;
    }

    // Apply rotation constraints
    btVector3 angular_factor(desc.lock_rotation_x ? 0.0f : 1.0f, desc.lock_rotation_y ? 0.0f : 1.0f,
                             desc.lock_rotation_z ? 0.0f : 1.0f);
    rigid_body_->setAngularFactor(angular_factor);

    // Set initial velocities
    if (motion_type_ == MotionType::Dynamic) {
        rigid_body_->setLinearVelocity(glm_to_bullet_vec(desc.linear_velocity));
        rigid_body_->setAngularVelocity(glm_to_bullet_vec(desc.angular_velocity));
    }

    // Store handle and pointer for callbacks
    rigid_body_->setUserIndex(static_cast<int>(handle_));
    rigid_body_->setUserPointer(this);

    // Initial gravity will be set when added to world
    update_gravity();
}

// ============================================================================
// Transform
// ============================================================================

glm::dvec3 RigidBody::get_position() const {
    if (!rigid_body_) {
        return glm::dvec3{0.0};
    }
    return bullet_to_glm_position(rigid_body_->getWorldTransform().getOrigin());
}

glm::dquat RigidBody::get_rotation() const {
    if (!rigid_body_) {
        return glm::dquat{1.0, 0.0, 0.0, 0.0};
    }
    return bullet_to_glm_rotation(rigid_body_->getWorldTransform().getRotation());
}

void RigidBody::set_position(const glm::dvec3& position) {
    if (!rigid_body_) {
        return;
    }
    btTransform transform = rigid_body_->getWorldTransform();
    transform.setOrigin(glm_to_bullet_vec(position));
    rigid_body_->setWorldTransform(transform);

    // Also update motion state for kinematic bodies
    if (motion_type_ == MotionType::Kinematic && motion_state_) {
        motion_state_->setWorldTransform(transform);
    }

    activate();
}

void RigidBody::set_rotation(const glm::dquat& rotation) {
    if (!rigid_body_) {
        return;
    }
    btTransform transform = rigid_body_->getWorldTransform();
    transform.setRotation(btQuaternion(static_cast<btScalar>(rotation.x), static_cast<btScalar>(rotation.y),
                                       static_cast<btScalar>(rotation.z), static_cast<btScalar>(rotation.w)));
    rigid_body_->setWorldTransform(transform);

    if (motion_type_ == MotionType::Kinematic && motion_state_) {
        motion_state_->setWorldTransform(transform);
    }

    activate();
}

void RigidBody::set_transform(const glm::dvec3& position, const glm::dquat& rotation) {
    if (!rigid_body_) {
        return;
    }
    btTransform transform = glm_to_bullet_transform(position, rotation);
    rigid_body_->setWorldTransform(transform);

    if (motion_type_ == MotionType::Kinematic && motion_state_) {
        motion_state_->setWorldTransform(transform);
    }

    activate();
}

// ============================================================================
// Interpolation
// ============================================================================

void RigidBody::store_previous_transform() {
    previous_position_ = get_position();
    previous_rotation_ = get_rotation();
}

glm::dvec3 RigidBody::get_interpolated_position(double alpha) const {
    glm::dvec3 current = get_position();
    return glm::mix(previous_position_, current, alpha);
}

glm::dquat RigidBody::get_interpolated_rotation(double alpha) const {
    glm::dquat current = get_rotation();
    return glm::slerp(previous_rotation_, current, alpha);
}

// ============================================================================
// Velocity
// ============================================================================

glm::dvec3 RigidBody::get_linear_velocity() const {
    if (!rigid_body_) {
        return glm::dvec3{0.0};
    }
    return bullet_to_glm_position(rigid_body_->getLinearVelocity());
}

glm::dvec3 RigidBody::get_angular_velocity() const {
    if (!rigid_body_) {
        return glm::dvec3{0.0};
    }
    return bullet_to_glm_position(rigid_body_->getAngularVelocity());
}

void RigidBody::set_linear_velocity(const glm::dvec3& velocity) {
    if (!rigid_body_ || motion_type_ != MotionType::Dynamic) {
        return;
    }
    rigid_body_->setLinearVelocity(glm_to_bullet_vec(velocity));
    activate();
}

void RigidBody::set_angular_velocity(const glm::dvec3& velocity) {
    if (!rigid_body_ || motion_type_ != MotionType::Dynamic) {
        return;
    }
    rigid_body_->setAngularVelocity(glm_to_bullet_vec(velocity));
    activate();
}

// ============================================================================
// Forces & Impulses
// ============================================================================

void RigidBody::apply_force(const glm::dvec3& force, const glm::dvec3& rel_pos) {
    if (!rigid_body_ || motion_type_ != MotionType::Dynamic) {
        return;
    }
    rigid_body_->applyForce(glm_to_bullet_vec(force), glm_to_bullet_vec(rel_pos));
    activate();
}

void RigidBody::apply_central_force(const glm::dvec3& force) {
    if (!rigid_body_ || motion_type_ != MotionType::Dynamic) {
        return;
    }
    rigid_body_->applyCentralForce(glm_to_bullet_vec(force));
    activate();
}

void RigidBody::apply_torque(const glm::dvec3& torque) {
    if (!rigid_body_ || motion_type_ != MotionType::Dynamic) {
        return;
    }
    rigid_body_->applyTorque(glm_to_bullet_vec(torque));
    activate();
}

void RigidBody::clear_forces() {
    if (!rigid_body_) {
        return;
    }
    rigid_body_->clearForces();
}

void RigidBody::apply_impulse(const glm::dvec3& impulse, const glm::dvec3& rel_pos) {
    if (!rigid_body_ || motion_type_ != MotionType::Dynamic) {
        return;
    }
    rigid_body_->applyImpulse(glm_to_bullet_vec(impulse), glm_to_bullet_vec(rel_pos));
    activate();
}

void RigidBody::apply_central_impulse(const glm::dvec3& impulse) {
    if (!rigid_body_ || motion_type_ != MotionType::Dynamic) {
        return;
    }
    rigid_body_->applyCentralImpulse(glm_to_bullet_vec(impulse));
    activate();
}

void RigidBody::apply_torque_impulse(const glm::dvec3& torque) {
    if (!rigid_body_ || motion_type_ != MotionType::Dynamic) {
        return;
    }
    rigid_body_->applyTorqueImpulse(glm_to_bullet_vec(torque));
    activate();
}

// ============================================================================
// Physics Properties
// ============================================================================

void RigidBody::set_material(const PhysicsMaterial& material) {
    material_ = material;
    if (rigid_body_) {
        rigid_body_->setFriction(static_cast<btScalar>(material_.friction));
        rigid_body_->setRestitution(static_cast<btScalar>(material_.restitution));
        rigid_body_->setDamping(static_cast<btScalar>(material_.linear_damping),
                                static_cast<btScalar>(material_.angular_damping));
    }
}

void RigidBody::set_gravity_scale(double scale) {
    gravity_scale_ = scale;
    update_gravity();
}

void RigidBody::update_gravity() {
    if (!rigid_body_ || motion_type_ != MotionType::Dynamic) {
        return;
    }

    glm::dvec3 scaled_gravity = base_gravity_ * gravity_scale_;
    rigid_body_->setGravity(glm_to_bullet_vec(scaled_gravity));
}

// ============================================================================
// Collision Filtering
// ============================================================================

void RigidBody::set_collision_layer(uint32_t layer) {
    collision_layer_ = layer;
    // Note: To actually update Bullet's collision filtering, the body would need
    // to be removed and re-added to the world. For now, we just store the value.
}

void RigidBody::set_collision_mask(uint32_t mask) {
    collision_mask_ = mask;
    // Note: Same as above - would need to update via world
}

// ============================================================================
// Bounds
// ============================================================================

AABB RigidBody::get_aabb() const {
    if (!rigid_body_ || !shape_) {
        return AABB{};
    }

    btVector3 bt_min, bt_max;
    shape_->getAabb(rigid_body_->getWorldTransform(), bt_min, bt_max);
    return AABB(bullet_to_glm_position(bt_min), bullet_to_glm_position(bt_max));
}

// ============================================================================
// Sleep State
// ============================================================================

bool RigidBody::is_sleeping() const {
    if (!rigid_body_) {
        return true;
    }
    return !rigid_body_->isActive();
}

void RigidBody::activate() {
    if (rigid_body_) {
        rigid_body_->activate(true);
    }
}

void RigidBody::deactivate() {
    if (rigid_body_) {
        rigid_body_->setActivationState(WANTS_DEACTIVATION);
    }
}

// ============================================================================
// Origin Shifting
// ============================================================================

void RigidBody::apply_origin_shift(const glm::dvec3& shift) {
    if (!rigid_body_) {
        return;
    }

    // Update current transform
    btTransform transform = rigid_body_->getWorldTransform();
    btVector3 new_origin = transform.getOrigin() - glm_to_bullet_vec(shift);
    transform.setOrigin(new_origin);
    rigid_body_->setWorldTransform(transform);

    // Update motion state
    if (motion_state_) {
        motion_state_->setWorldTransform(transform);
    }

    // Update previous position for interpolation
    previous_position_ -= shift;
}

}  // namespace realcraft::physics
