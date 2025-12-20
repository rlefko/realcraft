// RealCraft Physics Engine
// player_controller.cpp - Physics-based player movement controller implementation

#include <algorithm>
#include <cmath>
#include <realcraft/physics/physics_world.hpp>
#include <realcraft/physics/player_controller.hpp>
#include <realcraft/world/block.hpp>
#include <realcraft/world/world_manager.hpp>

namespace realcraft::physics {

// ============================================================================
// Implementation Details
// ============================================================================

struct PlayerController::Impl {
    // Dependencies
    PhysicsWorld* physics_world = nullptr;
    world::WorldManager* world_manager = nullptr;
    PlayerControllerConfig config;
    bool initialized = false;
    bool enabled = true;

    // Position and velocity
    glm::dvec3 position{0.0, 64.0, 0.0};
    glm::dvec3 velocity{0.0};
    glm::dvec3 previous_position{0.0, 64.0, 0.0};

    // Player state
    PlayerState state = PlayerState::Airborne;
    GroundInfo ground_info;

    // Movement state
    bool is_sprinting = false;
    bool is_crouching = false;

    // Jump state
    bool wants_jump = false;
    bool is_jumping = false;
    double time_since_grounded = 1.0;      // For coyote time
    double time_since_jump_pressed = 1.0;  // For jump buffering
    double variable_jump_timer = 0.0;      // For variable jump height

    // Water state
    double water_depth = 0.0;

    // ========================================================================
    // Internal Methods
    // ========================================================================

    void update_ground_detection();
    void update_water_detection();
    void update_state();
    void update_movement(double dt, const PlayerInput& input);
    void apply_friction(double dt);
    void apply_gravity(double dt);
    void handle_jump(double dt, const PlayerInput& input);
    void handle_stair_stepping(glm::dvec3& move_delta);
    void handle_slope_sliding(double dt);
    void resolve_collisions(glm::dvec3& new_position);
    bool is_position_in_water(const glm::dvec3& pos) const;
    bool is_position_solid(const glm::dvec3& pos) const;
    double calculate_jump_velocity(double desired_height, double gravity) const;
};

// ============================================================================
// Ground Detection
// ============================================================================

void PlayerController::Impl::update_ground_detection() {
    ground_info = GroundInfo{};

    if (!physics_world) {
        return;
    }

    // Cast ray from capsule bottom
    double capsule_bottom = position.y - config.capsule_height / 2.0;
    glm::dvec3 ray_origin = position;
    ray_origin.y = capsule_bottom + 0.01;  // Slight offset above feet

    double ray_length = config.ground_check_distance + config.step_height;

    auto hit = physics_world->raycast_voxels(ray_origin, glm::dvec3(0.0, -1.0, 0.0), ray_length);

    if (hit && hit->distance <= config.ground_check_distance) {
        ground_info.on_ground = true;
        ground_info.ground_point = hit->position;
        ground_info.ground_normal = hit->normal;

        // Calculate slope angle (angle from vertical)
        double dot = glm::dot(hit->normal, glm::dvec3(0.0, 1.0, 0.0));
        ground_info.slope_angle = std::acos(std::clamp(dot, -1.0, 1.0));

        // Check if slope is too steep to stand on
        ground_info.stable = (glm::degrees(ground_info.slope_angle) <= config.max_slope_angle);
    }

    // Edge detection: cast rays at capsule corners
    if (ground_info.on_ground) {
        const double offsets[] = {config.edge_detection_radius, -config.edge_detection_radius};
        int edge_rays_hit = 0;

        for (double ox : offsets) {
            for (double oz : offsets) {
                glm::dvec3 edge_origin = ray_origin + glm::dvec3(ox, 0.0, oz);
                auto edge_hit = physics_world->raycast_voxels(edge_origin, glm::dvec3(0.0, -1.0, 0.0),
                                                              config.ground_check_distance * 2.0);

                if (edge_hit && edge_hit->distance <= config.ground_check_distance) {
                    edge_rays_hit++;
                }
            }
        }

        // If fewer than 3 of 4 corner rays hit, we're on an edge
        ground_info.on_edge = (edge_rays_hit < 3);
    }
}

// ============================================================================
// Water Detection
// ============================================================================

void PlayerController::Impl::update_water_detection() {
    // Check water at feet and head positions
    glm::dvec3 feet_pos = position;
    feet_pos.y -= config.capsule_height / 2.0;

    glm::dvec3 head_pos = position;
    head_pos.y += config.capsule_height / 2.0 - config.water_surface_offset;

    bool feet_in_water = is_position_in_water(feet_pos);
    bool head_in_water = is_position_in_water(head_pos);

    if (head_in_water) {
        water_depth = config.capsule_height;  // Fully submerged
    } else if (feet_in_water) {
        // Partially submerged
        water_depth = config.capsule_height * 0.5;
    } else {
        water_depth = 0.0;
    }
}

bool PlayerController::Impl::is_position_in_water(const glm::dvec3& pos) const {
    if (!world_manager) {
        return false;
    }

    world::WorldBlockPos block_pos = world::world_to_block(pos);
    world::BlockId block_id = world_manager->get_block(block_pos);

    const world::BlockType* block_type = world::BlockRegistry::instance().get(block_id);

    return block_type && block_type->is_liquid();
}

bool PlayerController::Impl::is_position_solid(const glm::dvec3& pos) const {
    if (!world_manager) {
        return false;
    }

    world::WorldBlockPos block_pos = world::world_to_block(pos);
    world::BlockId block_id = world_manager->get_block(block_pos);

    const world::BlockType* block_type = world::BlockRegistry::instance().get(block_id);

    return block_type && block_type->is_solid();
}

// ============================================================================
// State Updates
// ============================================================================

void PlayerController::Impl::update_state() {
    // Determine player state based on ground and water
    if (water_depth > config.capsule_height * 0.5) {
        state = PlayerState::Swimming;
    } else if (ground_info.on_ground && ground_info.stable) {
        state = PlayerState::Grounded;
    } else {
        state = PlayerState::Airborne;
    }
}

// ============================================================================
// Movement
// ============================================================================

void PlayerController::Impl::update_movement(double dt, const PlayerInput& input) {
    // Update sprint/crouch state
    is_sprinting = input.sprint_pressed && state == PlayerState::Grounded && glm::length(input.move_direction) > 0.1f;
    is_crouching = input.crouch_pressed && state == PlayerState::Grounded;

    // Determine target speed based on state
    double target_speed = config.walk_speed;
    double acceleration = config.ground_acceleration;

    if (state == PlayerState::Swimming) {
        target_speed = config.swim_speed;
        acceleration = config.swim_acceleration;
    } else if (state == PlayerState::Airborne) {
        acceleration = config.air_acceleration;
    } else if (state == PlayerState::Grounded) {
        if (is_crouching) {
            target_speed = config.crouch_speed;
        } else if (is_sprinting) {
            target_speed = config.sprint_speed;
        }
    }

    // Calculate wish velocity (horizontal only for grounded/airborne)
    glm::dvec3 wish_vel{0.0};
    if (glm::length(input.move_direction) > 0.01f) {
        glm::dvec3 move_dir = glm::normalize(glm::dvec3(input.move_direction));

        if (state == PlayerState::Swimming) {
            // Swimming allows vertical input
            wish_vel = move_dir * target_speed;
        } else {
            // Ground/air movement is horizontal only
            wish_vel = glm::dvec3(move_dir.x, 0.0, move_dir.z) * target_speed;
        }
    }

    // Current horizontal velocity
    glm::dvec3 current_horiz{velocity.x, 0.0, velocity.z};

    // Accelerate towards wish velocity
    if (glm::length(wish_vel) > 0.01) {
        glm::dvec3 wish_dir = glm::normalize(wish_vel);
        double current_speed_in_wish_dir = glm::dot(current_horiz, wish_dir);
        double add_speed = target_speed - current_speed_in_wish_dir;

        if (add_speed > 0.0) {
            double accel_amount = acceleration * dt;
            accel_amount = std::min(accel_amount, add_speed);

            velocity.x += wish_dir.x * accel_amount;
            velocity.z += wish_dir.z * accel_amount;

            // For swimming, also apply vertical acceleration
            if (state == PlayerState::Swimming) {
                velocity.y += wish_dir.y * accel_amount;
            }
        }
    } else {
        // No input: apply friction
        apply_friction(dt);
    }
}

void PlayerController::Impl::apply_friction(double dt) {
    double friction = config.ground_friction;

    if (state == PlayerState::Airborne) {
        friction = config.air_friction;
    } else if (state == PlayerState::Swimming) {
        friction = config.swim_friction;
    }

    if (friction <= 0.0) {
        return;
    }

    // Apply friction to horizontal velocity
    glm::dvec3 horiz_vel{velocity.x, 0.0, velocity.z};
    double speed = glm::length(horiz_vel);

    if (speed < 0.01) {
        velocity.x = 0.0;
        velocity.z = 0.0;
        return;
    }

    double drop = speed * friction * dt;
    double new_speed = std::max(0.0, speed - drop);
    double scale = new_speed / speed;

    velocity.x *= scale;
    velocity.z *= scale;

    // Swimming also applies friction to vertical
    if (state == PlayerState::Swimming) {
        velocity.y *= (1.0 - friction * dt * 0.5);
    }
}

// ============================================================================
// Gravity
// ============================================================================

void PlayerController::Impl::apply_gravity(double dt) {
    if (state == PlayerState::Swimming) {
        // Reduced gravity + buoyancy in water
        double effective_gravity = config.swim_gravity;

        // Buoyancy pushes up when submerged
        if (water_depth > config.capsule_height * 0.5) {
            velocity.y += config.buoyancy * dt;
        }

        velocity.y -= effective_gravity * dt;

        // Clamp vertical speed in water
        velocity.y = std::clamp(velocity.y, -config.swim_speed, config.swim_speed);
    } else if (!ground_info.on_ground || !ground_info.stable) {
        // Normal gravity when airborne or on unstable slope
        velocity.y -= config.gravity * dt;

        // Terminal velocity
        velocity.y = std::max(velocity.y, -config.terminal_velocity);
    } else {
        // On stable ground: zero out downward velocity
        if (velocity.y < 0.0) {
            velocity.y = 0.0;
        }
    }
}

// ============================================================================
// Jump Mechanics
// ============================================================================

void PlayerController::Impl::handle_jump(double dt, const PlayerInput& input) {
    // Track jump press timing
    if (input.jump_just_pressed) {
        time_since_jump_pressed = 0.0;
        wants_jump = true;
    } else {
        time_since_jump_pressed += dt;
    }

    // Track time since grounded for coyote time
    if (ground_info.on_ground && ground_info.stable) {
        time_since_grounded = 0.0;
    } else {
        time_since_grounded += dt;
    }

    // Coyote time: allow jumping shortly after leaving ground
    bool can_coyote_jump = (time_since_grounded < config.coyote_time);

    // Jump buffering: remember jump press for when we land
    bool buffered_jump = (time_since_jump_pressed < config.jump_buffer_time);

    // Can we jump?
    bool can_jump =
        ((ground_info.on_ground && ground_info.stable) || can_coyote_jump) && (state != PlayerState::Swimming);

    if (can_jump && (wants_jump || buffered_jump)) {
        // Calculate jump velocity for desired height
        // v = sqrt(2 * g * h)
        double jump_velocity = calculate_jump_velocity(config.jump_height, config.gravity);

        // Apply jump
        velocity.y = jump_velocity;
        is_jumping = true;
        variable_jump_timer = 0.0;
        wants_jump = false;
        time_since_grounded = config.coyote_time + 1.0;  // Prevent double jump
        time_since_jump_pressed = config.jump_buffer_time + 1.0;

        state = PlayerState::Airborne;
    }

    // Variable jump height: reduce velocity if jump released early
    if (is_jumping && !input.jump_pressed && velocity.y > 0.0) {
        // Cut jump short
        double min_velocity = calculate_jump_velocity(config.jump_height, config.gravity) * config.variable_jump_factor;

        // Quickly reduce to minimum if above it
        if (velocity.y > min_velocity) {
            velocity.y = std::max(min_velocity, velocity.y - config.gravity * dt * 2.0);
        }
    }

    // Reset jumping flag when falling
    if (velocity.y <= 0.0) {
        is_jumping = false;
    }

    // Swimming: allow jumping out of water
    if (state == PlayerState::Swimming && input.jump_pressed) {
        velocity.y = std::min(velocity.y + config.swim_acceleration * dt, config.swim_speed);
    }
}

double PlayerController::Impl::calculate_jump_velocity(double desired_height, double gravity) const {
    // v = sqrt(2 * g * h)
    return std::sqrt(2.0 * gravity * desired_height);
}

// ============================================================================
// Stair Stepping
// ============================================================================

void PlayerController::Impl::handle_stair_stepping(glm::dvec3& move_delta) {
    if (!ground_info.on_ground || !physics_world) {
        return;
    }

    // Only step when moving horizontally
    glm::dvec3 horizontal_delta{move_delta.x, 0.0, move_delta.z};
    double move_distance = glm::length(horizontal_delta);

    if (move_distance < 0.001) {
        return;
    }

    glm::dvec3 move_dir = glm::normalize(horizontal_delta);

    // Check if blocked at current height
    auto forward_hit = physics_world->raycast_voxels(position, move_dir, move_distance + config.capsule_radius);

    if (!forward_hit || forward_hit->distance > move_distance + config.capsule_radius) {
        // Not blocked, no stepping needed
        return;
    }

    // Try stepping up
    glm::dvec3 step_up_pos = position;
    step_up_pos.y += config.step_height;

    // Check if we can move up (not blocked by ceiling)
    auto ceiling_hit = physics_world->raycast_voxels(position, glm::dvec3(0.0, 1.0, 0.0), config.step_height + 0.1);

    if (ceiling_hit && ceiling_hit->distance < config.step_height) {
        // Ceiling blocks us
        return;
    }

    // Check if forward movement is clear at elevated height
    auto elevated_forward_hit =
        physics_world->raycast_voxels(step_up_pos, move_dir, move_distance + config.capsule_radius);

    if (elevated_forward_hit && elevated_forward_hit->distance <= move_distance + config.capsule_radius) {
        // Still blocked even when elevated
        return;
    }

    // Step down to find the stair surface
    glm::dvec3 elevated_target = step_up_pos + horizontal_delta;
    auto step_down_hit = physics_world->raycast_voxels(elevated_target, glm::dvec3(0.0, -1.0, 0.0),
                                                       config.step_height + config.ground_check_distance);

    if (step_down_hit) {
        // Found the stair - calculate final position
        double final_y = elevated_target.y - step_down_hit->distance + config.capsule_height / 2.0;

        // Only step if the height difference is within step_height
        double height_diff = final_y - position.y;
        if (height_diff > 0.0 && height_diff <= config.step_height) {
            move_delta.x = horizontal_delta.x;
            move_delta.y = height_diff + 0.01;  // Slight offset
            move_delta.z = horizontal_delta.z;
        }
    }
}

// ============================================================================
// Slope Sliding
// ============================================================================

void PlayerController::Impl::handle_slope_sliding(double dt) {
    if (!ground_info.on_ground || ground_info.stable) {
        return;  // Only slide on steep slopes
    }

    // Calculate slide direction (downhill along slope)
    glm::dvec3 gravity_dir{0.0, -1.0, 0.0};
    glm::dvec3 normal = ground_info.ground_normal;

    // Project gravity onto the slope plane
    glm::dvec3 slide_dir = gravity_dir - normal * glm::dot(gravity_dir, normal);
    double slide_length = glm::length(slide_dir);

    if (slide_length < 0.001) {
        return;
    }

    slide_dir = glm::normalize(slide_dir);

    // Apply slide acceleration
    double slide_acceleration = config.gravity * std::sin(ground_info.slope_angle);

    // Add slide velocity
    velocity += slide_dir * slide_acceleration * dt;

    // Apply slide friction
    double speed = glm::length(velocity);
    if (speed > 0.01) {
        double friction_force = config.slide_friction * dt;
        double new_speed = std::max(0.0, speed - friction_force);
        velocity *= (new_speed / speed);
    }
}

// ============================================================================
// Collision Resolution
// ============================================================================

void PlayerController::Impl::resolve_collisions(glm::dvec3& new_position) {
    if (!physics_world) {
        return;
    }

    // Simple collision resolution using raycasts
    // Check for horizontal collisions
    double capsule_bottom = new_position.y - config.capsule_height / 2.0;
    double capsule_top = new_position.y + config.capsule_height / 2.0;

    // Check collision at multiple heights
    const double check_heights[] = {capsule_bottom + 0.1, new_position.y, capsule_top - 0.1};

    for (double check_y : check_heights) {
        glm::dvec3 check_pos{new_position.x, check_y, new_position.z};

        // Check all 4 horizontal directions
        const glm::dvec3 directions[] = {{1.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {0.0, 0.0, -1.0}};

        for (const auto& dir : directions) {
            auto hit = physics_world->raycast_voxels(check_pos, dir, config.capsule_radius);

            if (hit && hit->distance < config.capsule_radius) {
                // Push back from collision
                double push_back = config.capsule_radius - hit->distance;
                new_position -= dir * push_back;

                // Zero velocity in collision direction
                double vel_dot = glm::dot(velocity, dir);
                if (vel_dot > 0.0) {
                    velocity -= dir * vel_dot;
                }
            }
        }
    }

    // Check ceiling collision
    auto ceiling_hit =
        physics_world->raycast_voxels(new_position, glm::dvec3(0.0, 1.0, 0.0), config.capsule_height / 2.0 + 0.1);

    if (ceiling_hit && ceiling_hit->distance < config.capsule_height / 2.0) {
        new_position.y = ceiling_hit->position.y - config.capsule_height / 2.0 - 0.01;
        if (velocity.y > 0.0) {
            velocity.y = 0.0;
        }
    }
}

// ============================================================================
// PlayerController Public Methods
// ============================================================================

PlayerController::PlayerController() : impl_(std::make_unique<Impl>()) {}

PlayerController::~PlayerController() = default;

PlayerController::PlayerController(PlayerController&&) noexcept = default;
PlayerController& PlayerController::operator=(PlayerController&&) noexcept = default;

bool PlayerController::initialize(PhysicsWorld* physics_world, world::WorldManager* world_manager,
                                  const PlayerControllerConfig& config) {
    if (impl_->initialized) {
        return false;
    }

    if (!physics_world || !world_manager) {
        return false;
    }

    // PhysicsWorld and WorldManager must outlive the PlayerController
    // Caller is responsible for lifetime management
    // NOLINTNEXTLINE(codeql-cpp/local-variable-address-stored-in-non-local-memory)
    impl_->physics_world = physics_world;
    // NOLINTNEXTLINE(codeql-cpp/local-variable-address-stored-in-non-local-memory)
    impl_->world_manager = world_manager;
    impl_->config = config;
    impl_->initialized = true;
    impl_->enabled = true;

    return true;
}

void PlayerController::shutdown() {
    if (!impl_->initialized) {
        return;
    }

    impl_->physics_world = nullptr;
    impl_->world_manager = nullptr;
    impl_->initialized = false;
}

bool PlayerController::is_initialized() const {
    return impl_->initialized;
}

void PlayerController::fixed_update(double fixed_delta, const PlayerInput& input) {
    if (!impl_->initialized || !impl_->enabled) {
        return;
    }

    // Update detections
    impl_->update_ground_detection();
    impl_->update_water_detection();
    impl_->update_state();

    // Handle slope sliding first
    impl_->handle_slope_sliding(fixed_delta);

    // Handle jumping
    impl_->handle_jump(fixed_delta, input);

    // Update movement
    impl_->update_movement(fixed_delta, input);

    // Apply gravity
    impl_->apply_gravity(fixed_delta);

    // Calculate movement delta
    glm::dvec3 move_delta = impl_->velocity * fixed_delta;

    // Handle stair stepping
    impl_->handle_stair_stepping(move_delta);

    // Apply movement
    glm::dvec3 new_position = impl_->position + move_delta;

    // Resolve collisions
    impl_->resolve_collisions(new_position);

    // Update position
    impl_->position = new_position;
}

void PlayerController::store_previous_state() {
    impl_->previous_position = impl_->position;
}

void PlayerController::set_position(const glm::dvec3& position) {
    impl_->position = position;
}

glm::dvec3 PlayerController::get_position() const {
    return impl_->position;
}

glm::dvec3 PlayerController::get_eye_position() const {
    glm::dvec3 eye = impl_->position;
    eye.y += impl_->config.eye_height - impl_->config.capsule_height / 2.0;
    return eye;
}

glm::dvec3 PlayerController::get_interpolated_position(double alpha) const {
    return glm::mix(impl_->previous_position, impl_->position, alpha);
}

glm::dvec3 PlayerController::get_interpolated_eye_position(double alpha) const {
    glm::dvec3 interpolated = get_interpolated_position(alpha);
    interpolated.y += impl_->config.eye_height - impl_->config.capsule_height / 2.0;
    return interpolated;
}

glm::dvec3 PlayerController::get_velocity() const {
    return impl_->velocity;
}

void PlayerController::set_velocity(const glm::dvec3& velocity) {
    impl_->velocity = velocity;
}

PlayerState PlayerController::get_state() const {
    return impl_->state;
}

bool PlayerController::is_grounded() const {
    return impl_->ground_info.on_ground && impl_->ground_info.stable;
}

bool PlayerController::is_swimming() const {
    return impl_->state == PlayerState::Swimming;
}

bool PlayerController::is_sprinting() const {
    return impl_->is_sprinting;
}

bool PlayerController::is_crouching() const {
    return impl_->is_crouching;
}

bool PlayerController::is_on_slope() const {
    return impl_->ground_info.on_ground && !impl_->ground_info.stable;
}

const GroundInfo& PlayerController::get_ground_info() const {
    return impl_->ground_info;
}

void PlayerController::set_config(const PlayerControllerConfig& config) {
    impl_->config = config;
}

const PlayerControllerConfig& PlayerController::get_config() const {
    return impl_->config;
}

void PlayerController::set_enabled(bool enabled) {
    impl_->enabled = enabled;
}

bool PlayerController::is_enabled() const {
    return impl_->enabled;
}

AABB PlayerController::get_bounds() const {
    double half_height = impl_->config.capsule_height / 2.0;
    double radius = impl_->config.capsule_radius;

    return AABB(impl_->position - glm::dvec3(radius, half_height, radius),
                impl_->position + glm::dvec3(radius, half_height, radius));
}

void PlayerController::apply_origin_shift(const glm::dvec3& shift) {
    impl_->position -= shift;
    impl_->previous_position -= shift;
}

}  // namespace realcraft::physics
