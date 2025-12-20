// RealCraft Physics Engine
// rigid_body.hpp - Rigid body dynamics with Bullet Physics integration

#pragma once

#include "types.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <memory>

// Forward declarations for Bullet types
class btRigidBody;
class btCollisionShape;
class btMotionState;

namespace realcraft::physics {

// ============================================================================
// Rigid Body Descriptor
// ============================================================================

struct RigidBodyDesc {
    // Transform
    glm::dvec3 position{0.0};
    glm::dquat rotation = glm::dquat(1.0, 0.0, 0.0, 0.0);

    // Mass (0 = static/infinite mass, ignored if motion_type is Static)
    double mass = 1.0;

    // Motion type
    MotionType motion_type = MotionType::Dynamic;

    // Shape configuration
    ColliderType collider_type = ColliderType::AABB;
    glm::dvec3 half_extents{0.5};  // For AABB
    double radius = 0.5;           // For Capsule
    double height = 1.0;           // For Capsule (total height including caps)

    // Physics material
    PhysicsMaterial material;

    // Collision filtering
    uint32_t collision_layer = static_cast<uint32_t>(CollisionLayer::Default);
    uint32_t collision_mask = static_cast<uint32_t>(CollisionLayer::All);

    // Initial velocity
    glm::dvec3 linear_velocity{0.0};
    glm::dvec3 angular_velocity{0.0};

    // Gravity settings
    double gravity_scale = 1.0;  // Multiplier for world gravity (0 = no gravity)

    // Rotation constraints (true = locked)
    bool lock_rotation_x = false;
    bool lock_rotation_y = false;
    bool lock_rotation_z = false;

    // User data
    void* user_data = nullptr;
};

// ============================================================================
// Rigid Body
// ============================================================================

class RigidBody {
public:
    RigidBody(RigidBodyHandle handle, const RigidBodyDesc& desc);
    ~RigidBody();

    // Non-copyable
    RigidBody(const RigidBody&) = delete;
    RigidBody& operator=(const RigidBody&) = delete;

    // Move-only
    RigidBody(RigidBody&& other) noexcept;
    RigidBody& operator=(RigidBody&& other) noexcept;

    // ========================================================================
    // Identification
    // ========================================================================

    [[nodiscard]] RigidBodyHandle get_handle() const { return handle_; }
    [[nodiscard]] MotionType get_motion_type() const { return motion_type_; }
    [[nodiscard]] ColliderType get_collider_type() const { return collider_type_; }

    // ========================================================================
    // Transform
    // ========================================================================

    [[nodiscard]] glm::dvec3 get_position() const;
    [[nodiscard]] glm::dquat get_rotation() const;
    void set_position(const glm::dvec3& position);
    void set_rotation(const glm::dquat& rotation);
    void set_transform(const glm::dvec3& position, const glm::dquat& rotation);

    // ========================================================================
    // Interpolation (for rendering between physics steps)
    // ========================================================================

    void store_previous_transform();
    [[nodiscard]] glm::dvec3 get_interpolated_position(double alpha) const;
    [[nodiscard]] glm::dquat get_interpolated_rotation(double alpha) const;

    // ========================================================================
    // Velocity
    // ========================================================================

    [[nodiscard]] glm::dvec3 get_linear_velocity() const;
    [[nodiscard]] glm::dvec3 get_angular_velocity() const;
    void set_linear_velocity(const glm::dvec3& velocity);
    void set_angular_velocity(const glm::dvec3& velocity);

    // ========================================================================
    // Forces & Impulses
    // ========================================================================

    // Forces are applied continuously and cleared each physics step
    void apply_force(const glm::dvec3& force, const glm::dvec3& rel_pos);
    void apply_central_force(const glm::dvec3& force);
    void apply_torque(const glm::dvec3& torque);
    void clear_forces();

    // Impulses are instantaneous changes to velocity
    void apply_impulse(const glm::dvec3& impulse, const glm::dvec3& rel_pos);
    void apply_central_impulse(const glm::dvec3& impulse);
    void apply_torque_impulse(const glm::dvec3& torque);

    // ========================================================================
    // Physics Properties
    // ========================================================================

    [[nodiscard]] double get_mass() const { return mass_; }
    [[nodiscard]] const PhysicsMaterial& get_material() const { return material_; }
    void set_material(const PhysicsMaterial& material);

    [[nodiscard]] double get_gravity_scale() const { return gravity_scale_; }
    void set_gravity_scale(double scale);

    // ========================================================================
    // Collision Filtering
    // ========================================================================

    [[nodiscard]] uint32_t get_collision_layer() const { return collision_layer_; }
    [[nodiscard]] uint32_t get_collision_mask() const { return collision_mask_; }
    void set_collision_layer(uint32_t layer);
    void set_collision_mask(uint32_t mask);

    // ========================================================================
    // Bounds
    // ========================================================================

    [[nodiscard]] AABB get_aabb() const;

    // ========================================================================
    // Sleep State
    // ========================================================================

    [[nodiscard]] bool is_sleeping() const;
    void activate();
    void deactivate();

    // ========================================================================
    // User Data
    // ========================================================================

    [[nodiscard]] void* get_user_data() const { return user_data_; }
    void set_user_data(void* data) { user_data_ = data; }

    // ========================================================================
    // Bullet Access (for advanced usage)
    // ========================================================================

    [[nodiscard]] btRigidBody* get_bullet_body() { return rigid_body_.get(); }
    [[nodiscard]] const btRigidBody* get_bullet_body() const { return rigid_body_.get(); }
    [[nodiscard]] btCollisionShape* get_bullet_shape() { return shape_.get(); }
    [[nodiscard]] const btCollisionShape* get_bullet_shape() const { return shape_.get(); }

    // ========================================================================
    // Origin Shifting Support
    // ========================================================================

    void apply_origin_shift(const glm::dvec3& shift);

private:
    RigidBodyHandle handle_;
    MotionType motion_type_;
    ColliderType collider_type_;
    double mass_;
    double gravity_scale_;
    PhysicsMaterial material_;
    uint32_t collision_layer_;
    uint32_t collision_mask_;
    void* user_data_;

    // Interpolation state
    glm::dvec3 previous_position_{0.0};
    glm::dquat previous_rotation_{1.0, 0.0, 0.0, 0.0};

    // Bullet objects
    std::unique_ptr<btCollisionShape> shape_;
    std::unique_ptr<btMotionState> motion_state_;
    std::unique_ptr<btRigidBody> rigid_body_;

    // Base gravity (from world, stored for gravity_scale calculations)
    glm::dvec3 base_gravity_{0.0, -9.81, 0.0};

    void create_shape(const RigidBodyDesc& desc);
    void create_rigid_body(const RigidBodyDesc& desc);
    void update_gravity();
};

}  // namespace realcraft::physics
