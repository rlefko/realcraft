// RealCraft Physics Engine
// colliders.cpp - Collider implementations with Bullet Physics integration

#include <btBulletCollisionCommon.h>
#include <realcraft/physics/colliders.hpp>

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

}  // namespace

// ============================================================================
// AABBCollider Implementation
// ============================================================================

AABBCollider::AABBCollider(ColliderHandle handle, const AABBColliderDesc& desc)
    : handle_(handle), half_extents_(desc.half_extents), collision_layer_(desc.collision_layer),
      collision_mask_(desc.collision_mask), is_trigger_(desc.is_trigger), user_data_(desc.user_data) {
    // Create Bullet box shape
    btVector3 bt_half_extents(static_cast<btScalar>(half_extents_.x), static_cast<btScalar>(half_extents_.y),
                              static_cast<btScalar>(half_extents_.z));
    shape_ = std::make_unique<btBoxShape>(bt_half_extents);

    // Create collision object
    collision_object_ = std::make_unique<btCollisionObject>();
    collision_object_->setCollisionShape(shape_.get());
    collision_object_->setWorldTransform(glm_to_bullet_transform(desc.position, desc.rotation));

    // Set collision flags
    if (is_trigger_) {
        collision_object_->setCollisionFlags(collision_object_->getCollisionFlags() |
                                             btCollisionObject::CF_NO_CONTACT_RESPONSE);
    }

    // Store handle for later retrieval
    collision_object_->setUserIndex(static_cast<int>(handle_));
    collision_object_->setUserPointer(this);
}

AABBCollider::~AABBCollider() = default;

glm::dvec3 AABBCollider::get_half_extents() const {
    return half_extents_;
}

void AABBCollider::set_half_extents(const glm::dvec3& half_extents) {
    half_extents_ = half_extents;
    // Recreate shape with new extents
    btVector3 bt_half_extents(static_cast<btScalar>(half_extents_.x), static_cast<btScalar>(half_extents_.y),
                              static_cast<btScalar>(half_extents_.z));
    shape_ = std::make_unique<btBoxShape>(bt_half_extents);
    collision_object_->setCollisionShape(shape_.get());
}

glm::dvec3 AABBCollider::get_position() const {
    return bullet_to_glm_position(collision_object_->getWorldTransform().getOrigin());
}

glm::dquat AABBCollider::get_rotation() const {
    return bullet_to_glm_rotation(collision_object_->getWorldTransform().getRotation());
}

void AABBCollider::set_position(const glm::dvec3& position) {
    btTransform transform = collision_object_->getWorldTransform();
    transform.setOrigin(btVector3(static_cast<btScalar>(position.x), static_cast<btScalar>(position.y),
                                  static_cast<btScalar>(position.z)));
    collision_object_->setWorldTransform(transform);
}

void AABBCollider::set_rotation(const glm::dquat& rotation) {
    btTransform transform = collision_object_->getWorldTransform();
    transform.setRotation(btQuaternion(static_cast<btScalar>(rotation.x), static_cast<btScalar>(rotation.y),
                                       static_cast<btScalar>(rotation.z), static_cast<btScalar>(rotation.w)));
    collision_object_->setWorldTransform(transform);
}

AABB AABBCollider::get_aabb() const {
    glm::dvec3 pos = get_position();
    return AABB::from_center_extents(pos, half_extents_);
}

uint32_t AABBCollider::get_collision_layer() const {
    return collision_layer_;
}

uint32_t AABBCollider::get_collision_mask() const {
    return collision_mask_;
}

void AABBCollider::set_collision_layer(uint32_t layer) {
    collision_layer_ = layer;
}

void AABBCollider::set_collision_mask(uint32_t mask) {
    collision_mask_ = mask;
}

bool AABBCollider::is_trigger() const {
    return is_trigger_;
}

void* AABBCollider::get_user_data() const {
    return user_data_;
}

void AABBCollider::set_user_data(void* data) {
    user_data_ = data;
}

btCollisionObject* AABBCollider::get_bullet_object() {
    return collision_object_.get();
}

const btCollisionObject* AABBCollider::get_bullet_object() const {
    return collision_object_.get();
}

btCollisionShape* AABBCollider::get_bullet_shape() {
    return shape_.get();
}

const btCollisionShape* AABBCollider::get_bullet_shape() const {
    return shape_.get();
}

// ============================================================================
// CapsuleCollider Implementation
// ============================================================================

CapsuleCollider::CapsuleCollider(ColliderHandle handle, const CapsuleColliderDesc& desc)
    : handle_(handle), radius_(desc.radius), height_(desc.height), collision_layer_(desc.collision_layer),
      collision_mask_(desc.collision_mask), is_trigger_(desc.is_trigger), user_data_(desc.user_data) {
    // Bullet's btCapsuleShape takes radius and the height of the cylinder part (excluding caps)
    // Total height = cylinder_height + 2 * radius
    double cylinder_height = height_ - 2.0 * radius_;
    if (cylinder_height < 0.0) {
        cylinder_height = 0.0;
    }

    shape_ = std::make_unique<btCapsuleShape>(static_cast<btScalar>(radius_), static_cast<btScalar>(cylinder_height));

    // Create collision object
    collision_object_ = std::make_unique<btCollisionObject>();
    collision_object_->setCollisionShape(shape_.get());
    collision_object_->setWorldTransform(glm_to_bullet_transform(desc.position, desc.rotation));

    // Set collision flags
    if (is_trigger_) {
        collision_object_->setCollisionFlags(collision_object_->getCollisionFlags() |
                                             btCollisionObject::CF_NO_CONTACT_RESPONSE);
    }

    collision_object_->setUserIndex(static_cast<int>(handle_));
    collision_object_->setUserPointer(this);
}

CapsuleCollider::~CapsuleCollider() = default;

glm::dvec3 CapsuleCollider::get_position() const {
    return bullet_to_glm_position(collision_object_->getWorldTransform().getOrigin());
}

glm::dquat CapsuleCollider::get_rotation() const {
    return bullet_to_glm_rotation(collision_object_->getWorldTransform().getRotation());
}

void CapsuleCollider::set_position(const glm::dvec3& position) {
    btTransform transform = collision_object_->getWorldTransform();
    transform.setOrigin(btVector3(static_cast<btScalar>(position.x), static_cast<btScalar>(position.y),
                                  static_cast<btScalar>(position.z)));
    collision_object_->setWorldTransform(transform);
}

void CapsuleCollider::set_rotation(const glm::dquat& rotation) {
    btTransform transform = collision_object_->getWorldTransform();
    transform.setRotation(btQuaternion(static_cast<btScalar>(rotation.x), static_cast<btScalar>(rotation.y),
                                       static_cast<btScalar>(rotation.z), static_cast<btScalar>(rotation.w)));
    collision_object_->setWorldTransform(transform);
}

AABB CapsuleCollider::get_aabb() const {
    glm::dvec3 pos = get_position();
    // Conservative AABB for capsule
    double half_height = height_ * 0.5;
    glm::dvec3 half_extents(radius_, half_height, radius_);
    return AABB::from_center_extents(pos, half_extents);
}

uint32_t CapsuleCollider::get_collision_layer() const {
    return collision_layer_;
}

uint32_t CapsuleCollider::get_collision_mask() const {
    return collision_mask_;
}

void CapsuleCollider::set_collision_layer(uint32_t layer) {
    collision_layer_ = layer;
}

void CapsuleCollider::set_collision_mask(uint32_t mask) {
    collision_mask_ = mask;
}

bool CapsuleCollider::is_trigger() const {
    return is_trigger_;
}

void* CapsuleCollider::get_user_data() const {
    return user_data_;
}

void CapsuleCollider::set_user_data(void* data) {
    user_data_ = data;
}

btCollisionObject* CapsuleCollider::get_bullet_object() {
    return collision_object_.get();
}

const btCollisionObject* CapsuleCollider::get_bullet_object() const {
    return collision_object_.get();
}

btCollisionShape* CapsuleCollider::get_bullet_shape() {
    return shape_.get();
}

const btCollisionShape* CapsuleCollider::get_bullet_shape() const {
    return shape_.get();
}

// ============================================================================
// ConvexHullCollider Implementation
// ============================================================================

ConvexHullCollider::ConvexHullCollider(ColliderHandle handle, const ConvexHullColliderDesc& desc)
    : handle_(handle), collision_layer_(desc.collision_layer), collision_mask_(desc.collision_mask),
      is_trigger_(desc.is_trigger), user_data_(desc.user_data) {
    // Create convex hull shape from vertices
    shape_ = std::make_unique<btConvexHullShape>();

    for (const auto& vertex : desc.vertices) {
        shape_->addPoint(btVector3(vertex.x, vertex.y, vertex.z), false);
    }

    // Recalculate local AABB after adding all points
    shape_->recalcLocalAabb();

    // Create collision object
    collision_object_ = std::make_unique<btCollisionObject>();
    collision_object_->setCollisionShape(shape_.get());
    collision_object_->setWorldTransform(glm_to_bullet_transform(desc.position, desc.rotation));

    // Set collision flags
    if (is_trigger_) {
        collision_object_->setCollisionFlags(collision_object_->getCollisionFlags() |
                                             btCollisionObject::CF_NO_CONTACT_RESPONSE);
    }

    collision_object_->setUserIndex(static_cast<int>(handle_));
    collision_object_->setUserPointer(this);
}

ConvexHullCollider::~ConvexHullCollider() = default;

glm::dvec3 ConvexHullCollider::get_position() const {
    return bullet_to_glm_position(collision_object_->getWorldTransform().getOrigin());
}

glm::dquat ConvexHullCollider::get_rotation() const {
    return bullet_to_glm_rotation(collision_object_->getWorldTransform().getRotation());
}

void ConvexHullCollider::set_position(const glm::dvec3& position) {
    btTransform transform = collision_object_->getWorldTransform();
    transform.setOrigin(btVector3(static_cast<btScalar>(position.x), static_cast<btScalar>(position.y),
                                  static_cast<btScalar>(position.z)));
    collision_object_->setWorldTransform(transform);
}

void ConvexHullCollider::set_rotation(const glm::dquat& rotation) {
    btTransform transform = collision_object_->getWorldTransform();
    transform.setRotation(btQuaternion(static_cast<btScalar>(rotation.x), static_cast<btScalar>(rotation.y),
                                       static_cast<btScalar>(rotation.z), static_cast<btScalar>(rotation.w)));
    collision_object_->setWorldTransform(transform);
}

AABB ConvexHullCollider::get_aabb() const {
    btVector3 bt_min, bt_max;
    btTransform identity;
    identity.setIdentity();
    shape_->getAabb(collision_object_->getWorldTransform(), bt_min, bt_max);
    return AABB(bullet_to_glm_position(bt_min), bullet_to_glm_position(bt_max));
}

uint32_t ConvexHullCollider::get_collision_layer() const {
    return collision_layer_;
}

uint32_t ConvexHullCollider::get_collision_mask() const {
    return collision_mask_;
}

void ConvexHullCollider::set_collision_layer(uint32_t layer) {
    collision_layer_ = layer;
}

void ConvexHullCollider::set_collision_mask(uint32_t mask) {
    collision_mask_ = mask;
}

bool ConvexHullCollider::is_trigger() const {
    return is_trigger_;
}

void* ConvexHullCollider::get_user_data() const {
    return user_data_;
}

void ConvexHullCollider::set_user_data(void* data) {
    user_data_ = data;
}

btCollisionObject* ConvexHullCollider::get_bullet_object() {
    return collision_object_.get();
}

const btCollisionObject* ConvexHullCollider::get_bullet_object() const {
    return collision_object_.get();
}

btCollisionShape* ConvexHullCollider::get_bullet_shape() {
    return shape_.get();
}

const btCollisionShape* ConvexHullCollider::get_bullet_shape() const {
    return shape_.get();
}

}  // namespace realcraft::physics
