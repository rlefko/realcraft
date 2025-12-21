// RealCraft Physics Engine
// fluid_simulation.cpp - Cellular automata fluid simulation implementation

#include <algorithm>
#include <chrono>
#include <cmath>
#include <queue>
#include <realcraft/physics/fluid_simulation.hpp>
#include <realcraft/physics/physics_world.hpp>
#include <unordered_set>

namespace realcraft::physics {

// ============================================================================
// Implementation Details
// ============================================================================

struct FluidSimulation::Impl {
    FluidSimulationConfig config;
    bool initialized = false;
    bool enabled = true;

    PhysicsWorld* physics_world = nullptr;
    world::WorldManager* world_manager = nullptr;

    // Cached water block ID for quick comparisons
    world::BlockId water_block_id = world::BLOCK_INVALID;

    // Active water blocks that need updates (positions queued for processing)
    std::unordered_set<world::WorldBlockPos> active_water_blocks;
    std::queue<world::WorldBlockPos> update_queue;

    // Callback for fluid events
    FluidUpdateCallback update_callback;

    // Statistics
    Stats stats;

    // Time tracking for cellular automata ticks
    double accumulated_time = 0.0;
    double current_time = 0.0;

    // Horizontal neighbor offsets for water spread
    static constexpr glm::ivec3 HORIZONTAL_OFFSETS[4] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};

    // Check if a position can be occupied by water
    [[nodiscard]] bool can_water_occupy(const world::WorldBlockPos& pos) const {
        if (!world_manager) {
            return false;
        }

        world::BlockId block_id = world_manager->get_block(pos);

        // Air can be occupied
        if (block_id == world::BLOCK_AIR) {
            return true;
        }

        // Water can flow into partial water
        if (block_id == water_block_id) {
            return true;
        }

        // Check if block is replaceable (grass, flowers, etc.)
        const auto* block_type = world::BlockRegistry::instance().get(block_id);
        if (block_type && block_type->is_replaceable()) {
            return true;
        }

        return false;
    }

    // Get water state at a position (returns invalid if not water)
    [[nodiscard]] FluidCellInfo get_water_state(const world::WorldBlockPos& pos) const {
        FluidCellInfo info;

        if (!world_manager) {
            return info;
        }

        world::BlockId block_id = world_manager->get_block(pos);
        if (block_id != water_block_id) {
            return info;
        }

        world::BlockStateId state = world_manager->get_block_state(pos);
        info.is_valid = true;
        info.level = world::water_get_level(state);
        info.flow_direction = world::water_get_flow_direction(state);
        info.pressure = world::water_get_pressure(state);
        info.is_source = world::water_is_source(state);

        return info;
    }

    // Set water at a position
    void set_water(const world::WorldBlockPos& pos, uint8_t level, bool is_source,
                   world::WaterFlowDirection flow = world::WaterFlowDirection::None, uint8_t pressure = 0) {
        if (!world_manager) {
            return;
        }

        if (level == 0) {
            // Remove water entirely
            world_manager->set_block(pos, world::BLOCK_AIR, 0);
            active_water_blocks.erase(pos);
        } else {
            world::BlockStateId state = world::water_make_state(level, is_source, flow, pressure);
            world_manager->set_block(pos, water_block_id, state);
            active_water_blocks.insert(pos);
        }
    }

    // Queue position for update
    void queue_update(const world::WorldBlockPos& pos) {
        if (active_water_blocks.find(pos) != active_water_blocks.end()) {
            update_queue.push(pos);
        }
    }

    // Queue neighbors for update
    void queue_neighbor_updates(const world::WorldBlockPos& pos) {
        // Queue all 6 neighbors
        for (int i = 0; i < 6; ++i) {
            world::WorldBlockPos neighbor = pos + world::WorldBlockPos(world::DIRECTION_OFFSETS[i]);
            if (active_water_blocks.find(neighbor) != active_water_blocks.end()) {
                update_queue.push(neighbor);
            }
        }
    }

    // Count adjacent full source blocks
    [[nodiscard]] int count_adjacent_full_sources(const world::WorldBlockPos& pos) const {
        int count = 0;

        for (const auto& offset : HORIZONTAL_OFFSETS) {
            world::WorldBlockPos neighbor = pos + world::WorldBlockPos(offset);
            auto info = get_water_state(neighbor);
            if (info.is_valid && info.is_source && info.level == config.full_level) {
                ++count;
            }
        }

        return count;
    }

    // Calculate pressure based on water depth
    [[nodiscard]] uint8_t calculate_pressure(const world::WorldBlockPos& pos) const {
        // Count water blocks above
        int depth = 0;
        auto check_pos = pos;

        while (depth < 256) {
            check_pos.y++;
            auto info = get_water_state(check_pos);
            if (!info.is_valid) {
                break;
            }
            ++depth;
        }

        return static_cast<uint8_t>(std::min(depth, static_cast<int>(config.max_pressure)));
    }

    // Update flow direction based on level differences
    [[nodiscard]] world::WaterFlowDirection calculate_flow_direction(const world::WorldBlockPos& pos,
                                                                     uint8_t current_level) const {
        // Check downward first (gravity)
        world::WorldBlockPos below = pos + world::WorldBlockPos(0, -1, 0);
        if (can_water_occupy(below)) {
            auto below_info = get_water_state(below);
            if (!below_info.is_valid || below_info.level < config.full_level) {
                return world::WaterFlowDirection::NegY;
            }
        }

        // Check horizontal neighbors for largest level difference
        world::WaterFlowDirection best_dir = world::WaterFlowDirection::None;
        int max_diff = 0;

        for (size_t i = 0; i < 4; ++i) {
            world::WorldBlockPos neighbor = pos + world::WorldBlockPos(HORIZONTAL_OFFSETS[i]);
            if (can_water_occupy(neighbor)) {
                auto neighbor_info = get_water_state(neighbor);
                int neighbor_level = neighbor_info.is_valid ? neighbor_info.level : 0;
                int diff = current_level - neighbor_level;

                if (diff > max_diff) {
                    max_diff = diff;
                    // Map index to direction
                    switch (i) {
                        case 0:
                            best_dir = world::WaterFlowDirection::PosX;
                            break;
                        case 1:
                            best_dir = world::WaterFlowDirection::NegX;
                            break;
                        case 2:
                            best_dir = world::WaterFlowDirection::PosZ;
                            break;
                        case 3:
                            best_dir = world::WaterFlowDirection::NegZ;
                            break;
                    }
                }
            }
        }

        return best_dir;
    }

    // Update a single water cell
    void update_water_cell(const world::WorldBlockPos& pos) {
        auto current = get_water_state(pos);
        if (!current.is_valid || current.level == 0) {
            active_water_blocks.erase(pos);
            return;
        }

        uint8_t original_level = current.level;
        uint8_t remaining_level = current.level;
        bool is_source = current.is_source;

        // 1. Try to flow down (gravity)
        world::WorldBlockPos below = pos + world::WorldBlockPos(0, -1, 0);
        if (can_water_occupy(below)) {
            auto below_info = get_water_state(below);

            if (!below_info.is_valid) {
                // Air below - flow down entirely (unless source)
                if (!is_source) {
                    // Transfer all water down
                    set_water(below, remaining_level, false, world::WaterFlowDirection::NegY);
                    set_water(pos, 0, false);
                    queue_neighbor_updates(pos);
                    queue_neighbor_updates(below);
                    ++stats.flow_updates_this_frame;
                    return;
                } else {
                    // Source: create new water below
                    set_water(below, config.full_level, false, world::WaterFlowDirection::NegY);
                    queue_neighbor_updates(below);
                    ++stats.flow_updates_this_frame;
                }
            } else if (below_info.level < config.full_level) {
                // Partial water below - fill it
                uint8_t space = config.full_level - below_info.level;
                uint8_t transfer = is_source ? space : std::min(space, remaining_level);

                set_water(below, below_info.level + transfer, below_info.is_source, world::WaterFlowDirection::NegY,
                          below_info.pressure);

                if (!is_source) {
                    remaining_level -= transfer;
                    if (remaining_level == 0) {
                        set_water(pos, 0, false);
                        queue_neighbor_updates(pos);
                        queue_neighbor_updates(below);
                        ++stats.flow_updates_this_frame;
                        return;
                    }
                }
                queue_neighbor_updates(below);
                ++stats.flow_updates_this_frame;
            }
        }

        // 2. Horizontal spread (if not source or still has water)
        if (remaining_level > config.min_flow_level || is_source) {
            // Find valid horizontal neighbors
            std::vector<world::WorldBlockPos> valid_neighbors;

            for (const auto& offset : HORIZONTAL_OFFSETS) {
                world::WorldBlockPos neighbor = pos + world::WorldBlockPos(offset);
                if (can_water_occupy(neighbor)) {
                    auto neighbor_info = get_water_state(neighbor);
                    int neighbor_level = neighbor_info.is_valid ? neighbor_info.level : 0;

                    // Only flow to neighbors with lower level (or empty)
                    int effective_level = is_source ? config.full_level : remaining_level;
                    if (neighbor_level < effective_level - 1) {
                        valid_neighbors.push_back(neighbor);
                    }
                }
            }

            if (!valid_neighbors.empty()) {
                if (is_source) {
                    // Source blocks spread at reduced level
                    uint8_t spread_level = config.full_level - 1;
                    for (const auto& neighbor : valid_neighbors) {
                        auto neighbor_info = get_water_state(neighbor);
                        if (!neighbor_info.is_valid || neighbor_info.level < spread_level) {
                            set_water(neighbor, spread_level, false);
                            queue_neighbor_updates(neighbor);
                            ++stats.flow_updates_this_frame;
                        }
                    }
                } else {
                    // Distribute water evenly
                    uint8_t total_level = remaining_level;
                    for (const auto& neighbor : valid_neighbors) {
                        auto neighbor_info = get_water_state(neighbor);
                        total_level += neighbor_info.is_valid ? neighbor_info.level : 0;
                    }

                    uint8_t count = static_cast<uint8_t>(valid_neighbors.size() + 1);
                    uint8_t avg_level = total_level / count;
                    uint8_t remainder = total_level % count;

                    // Update neighbors
                    for (size_t i = 0; i < valid_neighbors.size(); ++i) {
                        uint8_t new_level = avg_level;
                        if (remainder > 0 && i < valid_neighbors.size() - 1) {
                            new_level++;
                            remainder--;
                        }
                        if (new_level > 0) {
                            set_water(valid_neighbors[i], new_level, false);
                            queue_neighbor_updates(valid_neighbors[i]);
                            ++stats.flow_updates_this_frame;
                        }
                    }

                    // Update self
                    remaining_level = avg_level + remainder;
                }
            }
        }

        // 3. Check for infinite source creation
        if (!is_source && remaining_level == config.full_level) {
            int adjacent_sources = count_adjacent_full_sources(pos);
            if (adjacent_sources >= config.source_creation_threshold) {
                is_source = true;
                ++stats.source_blocks;

                if (update_callback) {
                    FluidUpdateEvent event;
                    event.position = pos;
                    event.old_level = original_level;
                    event.new_level = remaining_level;
                    event.became_source = true;
                    event.timestamp = current_time;
                    update_callback(event);
                }
            }
        }

        // 4. Update flow direction
        auto flow_dir = calculate_flow_direction(pos, remaining_level);

        // 5. Calculate pressure
        uint8_t pressure = calculate_pressure(pos);

        // 6. Update the block state if anything changed
        if (remaining_level != original_level || is_source != current.is_source || flow_dir != current.flow_direction ||
            pressure != current.pressure) {
            if (remaining_level > 0) {
                set_water(pos, remaining_level, is_source, flow_dir, pressure);
            } else {
                set_water(pos, 0, false);
            }
        }
    }
};

// Static member initialization
constexpr glm::ivec3 FluidSimulation::Impl::HORIZONTAL_OFFSETS[4];

// ============================================================================
// FluidSimulation Implementation
// ============================================================================

FluidSimulation::FluidSimulation() : impl_(std::make_unique<Impl>()) {}

FluidSimulation::~FluidSimulation() {
    if (impl_->initialized) {
        shutdown();
    }
}

bool FluidSimulation::initialize(PhysicsWorld* physics_world, world::WorldManager* world_manager,
                                 const FluidSimulationConfig& config) {
    if (!physics_world || !world_manager) {
        return false;
    }

    impl_->physics_world = physics_world;
    impl_->world_manager = world_manager;
    impl_->config = config;

    // Cache water block ID
    impl_->water_block_id = world::BlockRegistry::instance().water_id();
    if (impl_->water_block_id == world::BLOCK_INVALID) {
        return false;
    }

    impl_->initialized = true;
    impl_->enabled = true;
    impl_->stats = Stats{};
    impl_->accumulated_time = 0.0;
    impl_->current_time = 0.0;

    return true;
}

void FluidSimulation::shutdown() {
    impl_->active_water_blocks.clear();
    while (!impl_->update_queue.empty()) {
        impl_->update_queue.pop();
    }

    impl_->physics_world = nullptr;
    impl_->world_manager = nullptr;
    impl_->initialized = false;
}

bool FluidSimulation::is_initialized() const {
    return impl_->initialized;
}

void FluidSimulation::update(double delta_time) {
    if (!impl_->initialized || !impl_->enabled) {
        return;
    }

    impl_->current_time += delta_time;
    impl_->accumulated_time += delta_time;
    impl_->stats.flow_updates_this_frame = 0;

    auto start_time = std::chrono::high_resolution_clock::now();

    // Only process if enough time has passed
    if (impl_->accumulated_time < impl_->config.update_interval) {
        return;
    }

    impl_->accumulated_time -= impl_->config.update_interval;

    // Queue all active water blocks for update
    for (const auto& pos : impl_->active_water_blocks) {
        impl_->update_queue.push(pos);
    }

    // Process update queue with a limit
    uint32_t updates_processed = 0;
    std::unordered_set<world::WorldBlockPos> processed_this_frame;

    while (!impl_->update_queue.empty() && updates_processed < impl_->config.max_updates_per_frame) {
        world::WorldBlockPos pos = impl_->update_queue.front();
        impl_->update_queue.pop();

        // Skip if already processed this frame
        if (processed_this_frame.find(pos) != processed_this_frame.end()) {
            continue;
        }
        processed_this_frame.insert(pos);

        impl_->update_water_cell(pos);
        ++updates_processed;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    impl_->stats.last_update_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    impl_->stats.active_water_blocks = impl_->active_water_blocks.size();
    impl_->stats.pending_updates = impl_->update_queue.size();
    impl_->stats.accumulated_time = impl_->accumulated_time;
}

// ============================================================================
// IWorldObserver Implementation
// ============================================================================

void FluidSimulation::on_chunk_loaded(const world::ChunkPos& pos, world::Chunk& chunk) {
    if (!impl_->initialized) {
        return;
    }

    // Scan chunk for water blocks and add to active set
    size_t cells_added = 0;
    for (int y = 0; y < world::CHUNK_SIZE_Y && cells_added < impl_->config.max_active_cells_per_chunk; ++y) {
        for (int z = 0; z < world::CHUNK_SIZE_Z; ++z) {
            for (int x = 0; x < world::CHUNK_SIZE_X; ++x) {
                world::LocalBlockPos local(x, y, z);
                if (chunk.get_block(local) == impl_->water_block_id) {
                    world::WorldBlockPos world_pos = world::local_to_world(pos, local);
                    impl_->active_water_blocks.insert(world_pos);
                    ++cells_added;
                }
            }
        }
    }
}

void FluidSimulation::on_chunk_unloading(const world::ChunkPos& pos, const world::Chunk& chunk) {
    (void)chunk;

    if (!impl_->initialized) {
        return;
    }

    // Remove water blocks from this chunk from active set
    auto it = impl_->active_water_blocks.begin();
    while (it != impl_->active_water_blocks.end()) {
        if (world::world_to_chunk(*it) == pos) {
            it = impl_->active_water_blocks.erase(it);
        } else {
            ++it;
        }
    }
}

void FluidSimulation::on_block_changed(const world::BlockChangeEvent& event) {
    if (!impl_->initialized || !impl_->enabled) {
        return;
    }

    // Skip changes from world generation
    if (event.from_generation) {
        return;
    }

    // Water placed
    if (event.new_entry.block_id == impl_->water_block_id) {
        impl_->active_water_blocks.insert(event.position);
        impl_->queue_update(event.position);
        impl_->queue_neighbor_updates(event.position);
    }

    // Water removed (or replaced)
    if (event.old_entry.block_id == impl_->water_block_id && event.new_entry.block_id != impl_->water_block_id) {
        impl_->active_water_blocks.erase(event.position);
        // Queue neighbor updates as water may flow into this space
        impl_->queue_neighbor_updates(event.position);
    }

    // Block removed next to water - water may flow in
    if (event.old_entry.block_id != world::BLOCK_AIR && event.new_entry.block_id == world::BLOCK_AIR) {
        impl_->queue_neighbor_updates(event.position);
    }
}

void FluidSimulation::on_origin_shifted(const world::WorldBlockPos& old_origin,
                                        const world::WorldBlockPos& new_origin) {
    (void)old_origin;
    (void)new_origin;

    // Water uses world coordinates, no shifting needed
}

// ============================================================================
// Fluid Queries
// ============================================================================

FluidCellInfo FluidSimulation::get_fluid_info(const world::WorldBlockPos& pos) const {
    if (!impl_->initialized) {
        return FluidCellInfo{};
    }
    return impl_->get_water_state(pos);
}

bool FluidSimulation::is_water_at(const world::WorldBlockPos& pos) const {
    if (!impl_->initialized || !impl_->world_manager) {
        return false;
    }
    return impl_->world_manager->get_block(pos) == impl_->water_block_id;
}

uint8_t FluidSimulation::get_water_level(const world::WorldBlockPos& pos) const {
    auto info = get_fluid_info(pos);
    return info.is_valid ? info.level : 0;
}

world::WaterFlowDirection FluidSimulation::get_flow_direction(const world::WorldBlockPos& pos) const {
    auto info = get_fluid_info(pos);
    return info.is_valid ? info.flow_direction : world::WaterFlowDirection::None;
}

uint8_t FluidSimulation::get_water_pressure(const world::WorldBlockPos& pos) const {
    auto info = get_fluid_info(pos);
    return info.is_valid ? info.pressure : 0;
}

bool FluidSimulation::is_source_block(const world::WorldBlockPos& pos) const {
    auto info = get_fluid_info(pos);
    return info.is_valid && info.is_source;
}

glm::dvec3 FluidSimulation::get_flow_velocity_at(const world::WorldPos& pos) const {
    if (!impl_->initialized) {
        return glm::dvec3(0.0);
    }

    world::WorldBlockPos block_pos = world::world_to_block(pos);
    auto info = get_fluid_info(block_pos);

    if (!info.is_valid) {
        return glm::dvec3(0.0);
    }

    return info.get_flow_velocity(impl_->config.current_velocity_scale);
}

// ============================================================================
// Buoyancy Calculations
// ============================================================================

glm::dvec3 FluidSimulation::calculate_buoyancy_force(const RigidBody& body) const {
    if (!impl_->initialized) {
        return glm::dvec3(0.0);
    }

    AABB bounds = body.get_aabb();
    double submerged_fraction = get_submerged_fraction(bounds);

    if (submerged_fraction <= 0.0) {
        return glm::dvec3(0.0);
    }

    // Buoyancy = rho_water * g * V_submerged * scale
    glm::dvec3 extents = bounds.extents();
    double volume = extents.x * extents.y * extents.z;
    double submerged_volume = volume * submerged_fraction;

    // Get gravity from physics world
    glm::dvec3 gravity = impl_->physics_world ? impl_->physics_world->get_gravity() : glm::dvec3(0.0, -9.81, 0.0);
    double gravity_magnitude = glm::length(gravity);

    double buoyancy_magnitude =
        impl_->config.water_density * gravity_magnitude * submerged_volume * impl_->config.buoyancy_scale;

    // Buoyancy acts upward (opposite to gravity)
    return glm::dvec3(0.0, buoyancy_magnitude, 0.0);
}

glm::dvec3 FluidSimulation::calculate_drag_force(const glm::dvec3& velocity, double cross_section,
                                                 const world::WorldPos& position) const {
    if (!impl_->initialized) {
        return glm::dvec3(0.0);
    }

    world::WorldBlockPos block_pos = world::world_to_block(position);
    if (!is_water_at(block_pos)) {
        return glm::dvec3(0.0);
    }

    double speed = glm::length(velocity);
    if (speed < 0.001) {
        return glm::dvec3(0.0);
    }

    // Drag = 0.5 * rho * v^2 * C_d * A
    double drag_magnitude =
        0.5 * impl_->config.water_density * speed * speed * impl_->config.drag_coefficient * cross_section;

    // Drag opposes velocity
    glm::dvec3 drag_direction = -glm::normalize(velocity);
    return drag_direction * drag_magnitude;
}

double FluidSimulation::get_submerged_fraction(const AABB& bounds) const {
    if (!impl_->initialized) {
        return 0.0;
    }

    // Sample points within the AABB to estimate submerged fraction
    const int samples_per_axis = 3;
    glm::dvec3 size = bounds.half_extents() * 2.0;
    glm::dvec3 step = size / static_cast<double>(samples_per_axis);

    int total_samples = samples_per_axis * samples_per_axis * samples_per_axis;
    int submerged_samples = 0;

    for (int x = 0; x < samples_per_axis; ++x) {
        for (int y = 0; y < samples_per_axis; ++y) {
            for (int z = 0; z < samples_per_axis; ++z) {
                glm::dvec3 sample_pos = bounds.min + glm::dvec3(x + 0.5, y + 0.5, z + 0.5) * step;
                world::WorldBlockPos block_pos = world::world_to_block(sample_pos);

                if (is_water_at(block_pos)) {
                    uint8_t level = get_water_level(block_pos);
                    double fill_fraction = static_cast<double>(level) / 15.0;

                    // Check if sample point is below water surface in this block
                    double block_water_height = std::floor(sample_pos.y) + fill_fraction;
                    if (sample_pos.y < block_water_height) {
                        ++submerged_samples;
                    }
                }
            }
        }
    }

    return static_cast<double>(submerged_samples) / static_cast<double>(total_samples);
}

// ============================================================================
// Water Current Forces
// ============================================================================

glm::dvec3 FluidSimulation::calculate_current_force(const world::WorldPos& position, double cross_section) const {
    if (!impl_->initialized) {
        return glm::dvec3(0.0);
    }

    world::WorldBlockPos block_pos = world::world_to_block(position);
    auto info = get_fluid_info(block_pos);

    if (!info.is_valid || info.flow_direction == world::WaterFlowDirection::None) {
        return glm::dvec3(0.0);
    }

    // Get flow velocity
    glm::dvec3 flow_velocity = info.get_flow_velocity(impl_->config.current_velocity_scale);
    double flow_speed = glm::length(flow_velocity);

    if (flow_speed < 0.001) {
        return glm::dvec3(0.0);
    }

    // Force = 0.5 * rho * v^2 * C_d * A * scale
    double force_magnitude = 0.5 * impl_->config.water_density * flow_speed * flow_speed *
                             impl_->config.drag_coefficient * cross_section * impl_->config.current_force_scale;

    return glm::normalize(flow_velocity) * force_magnitude;
}

glm::dvec3 FluidSimulation::get_current_velocity(const world::WorldPos& position) const {
    return get_flow_velocity_at(position);
}

// ============================================================================
// Water Placement
// ============================================================================

bool FluidSimulation::place_water(const world::WorldBlockPos& pos, uint8_t level, bool as_source) {
    if (!impl_->initialized || !impl_->world_manager) {
        return false;
    }

    // Check if position can have water
    world::BlockId current_block = impl_->world_manager->get_block(pos);
    if (current_block != world::BLOCK_AIR && current_block != impl_->water_block_id) {
        const auto* block_type = world::BlockRegistry::instance().get(current_block);
        if (!block_type || !block_type->is_replaceable()) {
            return false;
        }
    }

    impl_->set_water(pos, level, as_source);
    impl_->queue_update(pos);
    impl_->queue_neighbor_updates(pos);

    return true;
}

bool FluidSimulation::remove_water(const world::WorldBlockPos& pos) {
    if (!impl_->initialized || !impl_->world_manager) {
        return false;
    }

    if (!is_water_at(pos)) {
        return false;
    }

    impl_->set_water(pos, 0, false);
    impl_->queue_neighbor_updates(pos);

    return true;
}

// ============================================================================
// Configuration
// ============================================================================

const FluidSimulationConfig& FluidSimulation::get_config() const {
    return impl_->config;
}

void FluidSimulation::set_config(const FluidSimulationConfig& config) {
    impl_->config = config;
}

void FluidSimulation::set_enabled(bool enabled) {
    impl_->enabled = enabled;
}

bool FluidSimulation::is_enabled() const {
    return impl_->enabled;
}

void FluidSimulation::set_fluid_update_callback(FluidUpdateCallback callback) {
    impl_->update_callback = std::move(callback);
}

FluidSimulation::Stats FluidSimulation::get_stats() const {
    return impl_->stats;
}

}  // namespace realcraft::physics
