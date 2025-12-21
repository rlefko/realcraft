// RealCraft Physics Engine
// fluid_simulation.hpp - Cellular automata fluid simulation with buoyancy and currents

#pragma once

#include "rigid_body.hpp"
#include "types.hpp"

#include <functional>
#include <memory>
#include <realcraft/world/world_manager.hpp>
#include <vector>

namespace realcraft::physics {

// Forward declarations
class PhysicsWorld;

// ============================================================================
// Fluid Simulation Configuration
// ============================================================================

struct FluidSimulationConfig {
    // Flow parameters
    double flow_rate = 0.25;                // Base flow rate (level units per second)
    double gravity_flow_speed = 4.0;        // Downward flow rate multiplier
    double pressure_flow_multiplier = 1.5;  // Extra speed under pressure
    uint8_t source_creation_threshold = 2;  // Adjacent full sources needed for infinite
    uint8_t full_level = 15;                // Maximum water level
    uint8_t min_flow_level = 1;             // Minimum level to propagate
    uint32_t max_flow_distance = 8;         // Maximum horizontal spread from source

    // Pressure
    double pressure_per_block = 1.0;          // Pressure units per block of depth
    uint8_t max_pressure = 15;                // Maximum pressure level
    double pressure_equalization_rate = 2.0;  // How fast pressure equalizes

    // Buoyancy
    double water_density = 1000.0;   // kg/m^3 (real water density)
    double buoyancy_scale = 1.0;     // Adjustment factor for gameplay
    double drag_coefficient = 0.47;  // For submerged objects

    // Currents
    double current_force_scale = 5.0;     // Force multiplier from flow
    double current_velocity_scale = 2.0;  // How fast current pushes entities

    // Performance
    uint32_t max_updates_per_frame = 1000;      // Limit flow calculations
    double update_interval = 0.05;              // Seconds between cellular automata ticks (20 ticks/sec)
    uint32_t max_active_cells_per_chunk = 256;  // Limit tracked cells per chunk
};

// ============================================================================
// Fluid Cell Information
// ============================================================================

struct FluidCellInfo {
    uint8_t level = 0;  // 0-15 (0 = empty, 15 = full)
    world::WaterFlowDirection flow_direction = world::WaterFlowDirection::None;
    uint8_t pressure = 0;    // 0-15 based on depth
    bool is_source = false;  // Infinite source block
    bool is_valid = false;   // Whether this is actually a water block

    // Computed velocity vector based on flow direction and speed
    [[nodiscard]] glm::dvec3 get_flow_velocity(double base_speed = 1.0) const {
        if (!is_valid || flow_direction == world::WaterFlowDirection::None) {
            return glm::dvec3(0.0);
        }
        glm::ivec3 offset = world::water_flow_to_offset(flow_direction);
        double speed = base_speed * (1.0 + pressure / 15.0);  // Faster with pressure
        return glm::dvec3(offset) * speed;
    }
};

// ============================================================================
// Fluid Update Event (for callbacks)
// ============================================================================

struct FluidUpdateEvent {
    world::WorldBlockPos position;
    uint8_t old_level = 0;
    uint8_t new_level = 0;
    bool became_source = false;
    double timestamp = 0.0;
};

using FluidUpdateCallback = std::function<void(const FluidUpdateEvent&)>;

// ============================================================================
// Fluid Simulation System
// ============================================================================

class FluidSimulation : public world::IWorldObserver {
public:
    FluidSimulation();
    ~FluidSimulation() override;

    // Non-copyable, non-movable
    FluidSimulation(const FluidSimulation&) = delete;
    FluidSimulation& operator=(const FluidSimulation&) = delete;
    FluidSimulation(FluidSimulation&&) = delete;
    FluidSimulation& operator=(FluidSimulation&&) = delete;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    bool initialize(PhysicsWorld* physics_world, world::WorldManager* world_manager,
                    const FluidSimulationConfig& config = {});
    void shutdown();
    [[nodiscard]] bool is_initialized() const;

    // ========================================================================
    // Update (called from PhysicsWorld::fixed_update)
    // ========================================================================

    void update(double delta_time);

    // ========================================================================
    // IWorldObserver Implementation
    // ========================================================================

    void on_chunk_loaded(const world::ChunkPos& pos, world::Chunk& chunk) override;
    void on_chunk_unloading(const world::ChunkPos& pos, const world::Chunk& chunk) override;
    void on_block_changed(const world::BlockChangeEvent& event) override;
    void on_origin_shifted(const world::WorldBlockPos& old_origin, const world::WorldBlockPos& new_origin) override;

    // ========================================================================
    // Fluid Queries
    // ========================================================================

    // Get complete fluid info at a position
    [[nodiscard]] FluidCellInfo get_fluid_info(const world::WorldBlockPos& pos) const;

    // Quick checks
    [[nodiscard]] bool is_water_at(const world::WorldBlockPos& pos) const;
    [[nodiscard]] uint8_t get_water_level(const world::WorldBlockPos& pos) const;
    [[nodiscard]] world::WaterFlowDirection get_flow_direction(const world::WorldBlockPos& pos) const;
    [[nodiscard]] uint8_t get_water_pressure(const world::WorldBlockPos& pos) const;
    [[nodiscard]] bool is_source_block(const world::WorldBlockPos& pos) const;

    // Get flow velocity at a world position (interpolated)
    [[nodiscard]] glm::dvec3 get_flow_velocity_at(const world::WorldPos& pos) const;

    // ========================================================================
    // Buoyancy Calculations
    // ========================================================================

    // Calculate buoyancy force for a rigid body based on submerged volume
    [[nodiscard]] glm::dvec3 calculate_buoyancy_force(const RigidBody& body) const;

    // Calculate drag force for a moving object in water
    [[nodiscard]] glm::dvec3 calculate_drag_force(const glm::dvec3& velocity, double cross_section,
                                                  const world::WorldPos& position) const;

    // Get submerged fraction of an AABB (0.0 = not submerged, 1.0 = fully submerged)
    [[nodiscard]] double get_submerged_fraction(const AABB& bounds) const;

    // ========================================================================
    // Water Current Forces
    // ========================================================================

    // Calculate force from water current on an object
    [[nodiscard]] glm::dvec3 calculate_current_force(const world::WorldPos& position, double cross_section) const;

    // Get current velocity for entity movement (players, mobs)
    [[nodiscard]] glm::dvec3 get_current_velocity(const world::WorldPos& position) const;

    // ========================================================================
    // Water Placement (utility for gameplay)
    // ========================================================================

    // Place water at a position (returns true if successful)
    bool place_water(const world::WorldBlockPos& pos, uint8_t level, bool as_source);

    // Remove water at a position
    bool remove_water(const world::WorldBlockPos& pos);

    // ========================================================================
    // Configuration
    // ========================================================================

    [[nodiscard]] const FluidSimulationConfig& get_config() const;
    void set_config(const FluidSimulationConfig& config);

    // Toggle fluid simulation (for Creative mode or performance)
    void set_enabled(bool enabled);
    [[nodiscard]] bool is_enabled() const;

    // ========================================================================
    // Callbacks
    // ========================================================================

    void set_fluid_update_callback(FluidUpdateCallback callback);

    // ========================================================================
    // Statistics
    // ========================================================================

    struct Stats {
        size_t active_water_blocks = 0;
        size_t pending_updates = 0;
        size_t flow_updates_this_frame = 0;
        size_t source_blocks = 0;
        double last_update_time_ms = 0.0;
        double accumulated_time = 0.0;
    };

    [[nodiscard]] Stats get_stats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::physics
