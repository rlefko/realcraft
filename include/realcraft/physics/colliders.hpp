// RealCraft Physics Engine
// colliders.hpp - Collider interface and implementations (AABB, Capsule, ConvexHull)

#pragma once

#include "types.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <memory>
#include <vector>

// Forward declarations for Bullet types
class btCollisionObject;
class btCollisionShape;
class btBoxShape;
class btCapsuleShape;
class btConvexHullShape;

namespace realcraft::physics {

// ============================================================================
// Collider Descriptors
// ============================================================================

struct ColliderDescBase {
    glm::dvec3 position{0.0};
    glm::dquat rotation = glm::dquat(1.0, 0.0, 0.0, 0.0);
    uint32_t collision_layer = static_cast<uint32_t>(CollisionLayer::Default);
    uint32_t collision_mask = static_cast<uint32_t>(CollisionLayer::All);
    bool is_trigger = false;  // If true, detects overlap but doesn't block
    void* user_data = nullptr;
};

struct AABBColliderDesc : ColliderDescBase {
    glm::dvec3 half_extents{0.5};  // Half-size in each direction
};

struct CapsuleColliderDesc : ColliderDescBase {
    double radius = 0.4;
    double height = 1.8;  // Total height including caps
};

struct ConvexHullColliderDesc : ColliderDescBase {
    std::vector<glm::vec3> vertices;  // Convex hull points
};

// ============================================================================
// Collider Base Interface
// ============================================================================

class Collider {
public:
    virtual ~Collider() = default;

    // Non-copyable
    Collider(const Collider&) = delete;
    Collider& operator=(const Collider&) = delete;

    [[nodiscard]] virtual ColliderType get_type() const = 0;
    [[nodiscard]] virtual ColliderHandle get_handle() const = 0;

    // Transform
    [[nodiscard]] virtual glm::dvec3 get_position() const = 0;
    [[nodiscard]] virtual glm::dquat get_rotation() const = 0;
    virtual void set_position(const glm::dvec3& position) = 0;
    virtual void set_rotation(const glm::dquat& rotation) = 0;

    // Bounds
    [[nodiscard]] virtual AABB get_aabb() const = 0;

    // Layer filtering
    [[nodiscard]] virtual uint32_t get_collision_layer() const = 0;
    [[nodiscard]] virtual uint32_t get_collision_mask() const = 0;
    virtual void set_collision_layer(uint32_t layer) = 0;
    virtual void set_collision_mask(uint32_t mask) = 0;

    // Trigger mode
    [[nodiscard]] virtual bool is_trigger() const = 0;

    // User data
    [[nodiscard]] virtual void* get_user_data() const = 0;
    virtual void set_user_data(void* data) = 0;

    // Access underlying Bullet objects
    [[nodiscard]] virtual btCollisionObject* get_bullet_object() = 0;
    [[nodiscard]] virtual const btCollisionObject* get_bullet_object() const = 0;
    [[nodiscard]] virtual btCollisionShape* get_bullet_shape() = 0;
    [[nodiscard]] virtual const btCollisionShape* get_bullet_shape() const = 0;

protected:
    Collider() = default;
};

// ============================================================================
// AABB Collider
// ============================================================================

class AABBCollider : public Collider {
public:
    AABBCollider(ColliderHandle handle, const AABBColliderDesc& desc);
    ~AABBCollider() override;

    [[nodiscard]] ColliderType get_type() const override { return ColliderType::AABB; }
    [[nodiscard]] ColliderHandle get_handle() const override { return handle_; }

    [[nodiscard]] glm::dvec3 get_half_extents() const;
    void set_half_extents(const glm::dvec3& half_extents);

    [[nodiscard]] glm::dvec3 get_position() const override;
    [[nodiscard]] glm::dquat get_rotation() const override;
    void set_position(const glm::dvec3& position) override;
    void set_rotation(const glm::dquat& rotation) override;
    [[nodiscard]] AABB get_aabb() const override;
    [[nodiscard]] uint32_t get_collision_layer() const override;
    [[nodiscard]] uint32_t get_collision_mask() const override;
    void set_collision_layer(uint32_t layer) override;
    void set_collision_mask(uint32_t mask) override;
    [[nodiscard]] bool is_trigger() const override;
    [[nodiscard]] void* get_user_data() const override;
    void set_user_data(void* data) override;
    [[nodiscard]] btCollisionObject* get_bullet_object() override;
    [[nodiscard]] const btCollisionObject* get_bullet_object() const override;
    [[nodiscard]] btCollisionShape* get_bullet_shape() override;
    [[nodiscard]] const btCollisionShape* get_bullet_shape() const override;

private:
    ColliderHandle handle_;
    std::unique_ptr<btBoxShape> shape_;
    std::unique_ptr<btCollisionObject> collision_object_;
    glm::dvec3 half_extents_;
    uint32_t collision_layer_;
    uint32_t collision_mask_;
    bool is_trigger_;
    void* user_data_;
};

// ============================================================================
// Capsule Collider
// ============================================================================

class CapsuleCollider : public Collider {
public:
    CapsuleCollider(ColliderHandle handle, const CapsuleColliderDesc& desc);
    ~CapsuleCollider() override;

    [[nodiscard]] ColliderType get_type() const override { return ColliderType::Capsule; }
    [[nodiscard]] ColliderHandle get_handle() const override { return handle_; }

    [[nodiscard]] double get_radius() const { return radius_; }
    [[nodiscard]] double get_height() const { return height_; }

    [[nodiscard]] glm::dvec3 get_position() const override;
    [[nodiscard]] glm::dquat get_rotation() const override;
    void set_position(const glm::dvec3& position) override;
    void set_rotation(const glm::dquat& rotation) override;
    [[nodiscard]] AABB get_aabb() const override;
    [[nodiscard]] uint32_t get_collision_layer() const override;
    [[nodiscard]] uint32_t get_collision_mask() const override;
    void set_collision_layer(uint32_t layer) override;
    void set_collision_mask(uint32_t mask) override;
    [[nodiscard]] bool is_trigger() const override;
    [[nodiscard]] void* get_user_data() const override;
    void set_user_data(void* data) override;
    [[nodiscard]] btCollisionObject* get_bullet_object() override;
    [[nodiscard]] const btCollisionObject* get_bullet_object() const override;
    [[nodiscard]] btCollisionShape* get_bullet_shape() override;
    [[nodiscard]] const btCollisionShape* get_bullet_shape() const override;

private:
    ColliderHandle handle_;
    std::unique_ptr<btCapsuleShape> shape_;
    std::unique_ptr<btCollisionObject> collision_object_;
    double radius_;
    double height_;
    uint32_t collision_layer_;
    uint32_t collision_mask_;
    bool is_trigger_;
    void* user_data_;
};

// ============================================================================
// Convex Hull Collider
// ============================================================================

class ConvexHullCollider : public Collider {
public:
    ConvexHullCollider(ColliderHandle handle, const ConvexHullColliderDesc& desc);
    ~ConvexHullCollider() override;

    [[nodiscard]] ColliderType get_type() const override { return ColliderType::ConvexHull; }
    [[nodiscard]] ColliderHandle get_handle() const override { return handle_; }

    [[nodiscard]] glm::dvec3 get_position() const override;
    [[nodiscard]] glm::dquat get_rotation() const override;
    void set_position(const glm::dvec3& position) override;
    void set_rotation(const glm::dquat& rotation) override;
    [[nodiscard]] AABB get_aabb() const override;
    [[nodiscard]] uint32_t get_collision_layer() const override;
    [[nodiscard]] uint32_t get_collision_mask() const override;
    void set_collision_layer(uint32_t layer) override;
    void set_collision_mask(uint32_t mask) override;
    [[nodiscard]] bool is_trigger() const override;
    [[nodiscard]] void* get_user_data() const override;
    void set_user_data(void* data) override;
    [[nodiscard]] btCollisionObject* get_bullet_object() override;
    [[nodiscard]] const btCollisionObject* get_bullet_object() const override;
    [[nodiscard]] btCollisionShape* get_bullet_shape() override;
    [[nodiscard]] const btCollisionShape* get_bullet_shape() const override;

private:
    ColliderHandle handle_;
    std::unique_ptr<btConvexHullShape> shape_;
    std::unique_ptr<btCollisionObject> collision_object_;
    uint32_t collision_layer_;
    uint32_t collision_mask_;
    bool is_trigger_;
    void* user_data_;
};

}  // namespace realcraft::physics
