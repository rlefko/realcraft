// RealCraft Physics Engine
// player_controller.hpp - Physics-based player movement controller

#pragma once

#include "types.hpp"

#include <glm/glm.hpp>

#include <memory>

namespace realcraft {
namespace world {
class WorldManager;
}
}  // namespace realcraft

namespace realcraft::physics {

class PhysicsWorld;

// ============================================================================
// Player State
// ============================================================================

enum class PlayerState : uint8_t {
    Grounded,  // On solid ground
    Airborne,  // Falling or jumping
    Swimming   // In water
};

// ============================================================================
// Player Input
// ============================================================================

struct PlayerInput {
    glm::vec3 move_direction{0.0f};  // Normalized XZ movement wish (in camera space)
    bool jump_pressed = false;
    bool jump_just_pressed = false;
    bool sprint_pressed = false;
    bool crouch_pressed = false;
};

// ============================================================================
// Ground Detection Result
// ============================================================================

struct GroundInfo {
    bool on_ground = false;
    glm::dvec3 ground_point{0.0};
    glm::dvec3 ground_normal{0.0, 1.0, 0.0};
    double slope_angle = 0.0;  // Radians from vertical
    bool on_edge = false;      // Near a ledge
    bool stable = true;        // Can stand without sliding
};

// ============================================================================
// Player Controller Configuration
// ============================================================================

struct PlayerControllerConfig {
    // Capsule dimensions
    double capsule_radius = 0.4;
    double capsule_height = 1.8;  // Total height including caps
    double eye_height = 1.6;      // Eye position from feet

    // Movement speeds (m/s)
    double walk_speed = 4.3;
    double sprint_speed = 5.6;
    double crouch_speed = 1.3;
    double swim_speed = 2.0;

    // Acceleration (m/s^2)
    double ground_acceleration = 50.0;
    double ground_deceleration = 50.0;
    double air_acceleration = 5.0;
    double air_deceleration = 5.0;
    double swim_acceleration = 20.0;

    // Friction
    double ground_friction = 6.0;
    double air_friction = 0.0;
    double swim_friction = 4.0;

    // Jumping
    double jump_height = 1.25;          // Desired jump height in blocks
    double variable_jump_factor = 0.5;  // Min jump if released early
    double coyote_time = 0.1;           // Seconds after leaving ground
    double jump_buffer_time = 0.1;      // Pre-ground jump buffer

    // Stair stepping
    double step_height = 0.6;         // Max step-up height (blocks)
    double step_search_offset = 0.1;  // Forward probe distance

    // Slope handling
    double max_slope_angle = 50.0;  // Degrees, slide above this
    double slide_friction = 2.0;    // Friction when sliding

    // Ground detection
    double ground_check_distance = 0.1;  // Ray length below feet
    double edge_detection_radius = 0.3;  // For detecting ledges

    // Swimming
    double water_surface_offset = 0.4;  // Head above water threshold
    double buoyancy = 0.5;              // Upward force in water

    // Gravity
    double gravity = 32.0;            // Blocks/s^2
    double terminal_velocity = 78.0;  // Max fall speed
    double swim_gravity = 4.0;        // Reduced gravity in water
};

// ============================================================================
// Player Controller
// ============================================================================

class PlayerController {
public:
    PlayerController();
    ~PlayerController();

    // Non-copyable, movable
    PlayerController(const PlayerController&) = delete;
    PlayerController& operator=(const PlayerController&) = delete;
    PlayerController(PlayerController&&) noexcept;
    PlayerController& operator=(PlayerController&&) noexcept;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    bool initialize(PhysicsWorld* physics_world, world::WorldManager* world_manager,
                    const PlayerControllerConfig& config = {});
    void shutdown();
    [[nodiscard]] bool is_initialized() const;

    // ========================================================================
    // Update (call in fixed_update at 60Hz)
    // ========================================================================

    void fixed_update(double fixed_delta, const PlayerInput& input);

    // Store previous state for interpolation (call before fixed_update)
    void store_previous_state();

    // ========================================================================
    // Position & Velocity
    // ========================================================================

    void set_position(const glm::dvec3& position);
    [[nodiscard]] glm::dvec3 get_position() const;
    [[nodiscard]] glm::dvec3 get_eye_position() const;

    // Interpolated positions for rendering
    [[nodiscard]] glm::dvec3 get_interpolated_position(double alpha) const;
    [[nodiscard]] glm::dvec3 get_interpolated_eye_position(double alpha) const;

    [[nodiscard]] glm::dvec3 get_velocity() const;
    void set_velocity(const glm::dvec3& velocity);

    // ========================================================================
    // State Queries
    // ========================================================================

    [[nodiscard]] PlayerState get_state() const;
    [[nodiscard]] bool is_grounded() const;
    [[nodiscard]] bool is_swimming() const;
    [[nodiscard]] bool is_sprinting() const;
    [[nodiscard]] bool is_crouching() const;
    [[nodiscard]] bool is_on_slope() const;
    [[nodiscard]] const GroundInfo& get_ground_info() const;

    // ========================================================================
    // Configuration
    // ========================================================================

    void set_config(const PlayerControllerConfig& config);
    [[nodiscard]] const PlayerControllerConfig& get_config() const;

    // Enable/disable physics (for spectator mode, cutscenes, etc.)
    void set_enabled(bool enabled);
    [[nodiscard]] bool is_enabled() const;

    // ========================================================================
    // Collision
    // ========================================================================

    [[nodiscard]] AABB get_bounds() const;

    // ========================================================================
    // Origin Shifting
    // ========================================================================

    void apply_origin_shift(const glm::dvec3& shift);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::physics
