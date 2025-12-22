// RealCraft Gameplay System
// block_interaction.cpp - Block targeting, breaking, and placement implementation

#include <algorithm>
#include <cmath>
#include <realcraft/core/logger.hpp>
#include <realcraft/gameplay/block_interaction.hpp>
#include <realcraft/gameplay/inventory.hpp>
#include <realcraft/gameplay/item_entity.hpp>
#include <realcraft/physics/types.hpp>

namespace realcraft::gameplay {

// ============================================================================
// BlockInteractionSystem Implementation
// ============================================================================

BlockInteractionSystem::BlockInteractionSystem() = default;

BlockInteractionSystem::~BlockInteractionSystem() {
    if (initialized_) {
        shutdown();
    }
}

BlockInteractionSystem::BlockInteractionSystem(BlockInteractionSystem&&) noexcept = default;
BlockInteractionSystem& BlockInteractionSystem::operator=(BlockInteractionSystem&&) noexcept = default;

bool BlockInteractionSystem::initialize(physics::PhysicsWorld* physics_world,
                                        physics::PlayerController* player_controller,
                                        world::WorldManager* world_manager, rendering::Camera* camera,
                                        const BlockInteractionConfig& config) {
    if (initialized_) {
        REALCRAFT_LOG_WARN(core::log_category::GAME, "BlockInteractionSystem already initialized");
        return false;
    }

    if (!physics_world || !player_controller || !world_manager || !camera) {
        REALCRAFT_LOG_ERROR(core::log_category::GAME, "BlockInteractionSystem: null pointer in initialization");
        return false;
    }

    physics_world_ = physics_world;
    player_controller_ = player_controller;
    world_manager_ = world_manager;
    camera_ = camera;
    config_ = config;

    // Set up override mode until inventory is connected
    // This allows the system to work without inventory for testing
    held_block_override_ = world::BLOCK_STONE;
    use_held_block_override_ = true;

    initialized_ = true;
    REALCRAFT_LOG_DEBUG(core::log_category::GAME, "BlockInteractionSystem initialized");
    return true;
}

void BlockInteractionSystem::shutdown() {
    if (!initialized_) {
        return;
    }

    physics_world_ = nullptr;
    player_controller_ = nullptr;
    world_manager_ = nullptr;
    camera_ = nullptr;
    inventory_ = nullptr;
    item_entity_manager_ = nullptr;

    target_ = TargetInfo{};
    mining_.reset();
    use_held_block_override_ = false;
    initialized_ = false;

    REALCRAFT_LOG_DEBUG(core::log_category::GAME, "BlockInteractionSystem shutdown");
}

bool BlockInteractionSystem::is_initialized() const {
    return initialized_;
}

void BlockInteractionSystem::update(double delta_time) {
    if (!initialized_) {
        return;
    }

    current_time_ += delta_time;

    // Update targeting (ray cast from camera)
    update_target();

    // Update mining progress if primary action is held
    update_mining(delta_time);
}

void BlockInteractionSystem::update_target() {
    // Get ray origin from player eye position
    glm::dvec3 origin = player_controller_->get_eye_position();

    // Get direction from camera
    glm::vec3 forward = camera_->get_forward();
    glm::dvec3 direction = glm::dvec3(forward);

    // Cast ray against voxels
    auto hit = physics_world_->raycast_voxels(origin, direction, config_.reach_distance);

    if (hit.has_value() && hit->block_position.has_value() && hit->block_id.has_value() &&
        hit->block_face.has_value()) {
        // Check if it's actually a solid/interactable block
        const auto* block_type = world::BlockRegistry::instance().get(*hit->block_id);
        if (block_type && !block_type->is_air()) {
            target_.has_target = true;
            target_.block_position = *hit->block_position;
            target_.hit_face = *hit->block_face;
            target_.block_id = *hit->block_id;
            target_.distance = hit->distance;
            target_.hit_point = hit->position;
            return;
        }
    }

    // No valid target
    target_.has_target = false;
}

void BlockInteractionSystem::update_mining(double delta_time) {
    if (!config_.enable_breaking) {
        return;
    }

    if (!primary_pressed_) {
        reset_mining();
        return;
    }

    if (!target_.has_target) {
        reset_mining();
        return;
    }

    // Check if block is breakable
    const auto* block_type = world::BlockRegistry::instance().get(target_.block_id);
    if (!block_type || !block_type->is_breakable()) {
        reset_mining();
        return;
    }

    // Instant break mode (creative)
    if (config_.instant_break) {
        break_block(target_.block_position);
        return;
    }

    // Check if we're mining a different block
    if (mining_.is_mining && mining_.target != target_.block_position) {
        reset_mining();
    }

    // Start or continue mining
    if (!mining_.is_mining) {
        start_mining(target_.block_position);
    }

    // Update mining progress
    mining_.progress += delta_time / mining_.required_time;

    // Check if block is broken
    if (mining_.progress >= 1.0) {
        break_block(mining_.target);
        reset_mining();
    }
}

void BlockInteractionSystem::start_mining(const world::WorldBlockPos& pos) {
    mining_.is_mining = true;
    mining_.target = pos;
    mining_.progress = 0.0;
    mining_.start_time = current_time_;
    mining_.required_time = calculate_break_time(target_.block_id);
}

void BlockInteractionSystem::reset_mining() {
    mining_.reset();
}

double BlockInteractionSystem::calculate_break_time(world::BlockId block_id) const {
    const auto* block_type = world::BlockRegistry::instance().get(block_id);
    if (!block_type) {
        return 1.0;
    }

    float hardness = block_type->get_material().hardness;

    // Base formula: break_time = hardness * 0.3 seconds per hardness point
    // Stone (hardness 5.0) = 1.5 seconds
    // Dirt (hardness 1.5) = 0.45 seconds
    // Wood (hardness 2.0) = 0.6 seconds
    // Sand (hardness 0.5) = 0.15 seconds
    // Glass (hardness 0.3) = 0.09 seconds (minimum 0.1)
    double break_time = static_cast<double>(hardness) * 0.3 / config_.base_break_speed;

    // Minimum break time for responsiveness
    return std::max(break_time, 0.1);
}

void BlockInteractionSystem::break_block(const world::WorldBlockPos& pos) {
    world::BlockId old_block = world_manager_->get_block(pos);

    // Set block to air
    world_manager_->set_block(pos, world::BLOCK_AIR, 0);

    // Trigger block broken callback
    world::BlockRegistry::instance().on_block_broken(pos, old_block);

    // Spawn item drop if item entity manager is available
    if (item_entity_manager_ != nullptr) {
        glm::dvec3 drop_pos{static_cast<double>(pos.x), static_cast<double>(pos.y), static_cast<double>(pos.z)};
        item_entity_manager_->spawn_block_drops(drop_pos, old_block);
    }

    // Log the break
    const auto* block_type = world::BlockRegistry::instance().get(old_block);
    const char* block_name = block_type ? block_type->get_name().c_str() : "unknown";
    REALCRAFT_LOG_DEBUG(core::log_category::GAME, "Block broken at ({}, {}, {}): {}", pos.x, pos.y, pos.z, block_name);
}

void BlockInteractionSystem::on_primary_action(bool pressed) {
    primary_pressed_ = pressed;
}

void BlockInteractionSystem::on_secondary_action(bool just_pressed) {
    if (!initialized_ || !config_.enable_placement) {
        return;
    }

    // Only place on just pressed (not while held)
    if (!just_pressed) {
        return;
    }

    if (!target_.has_target) {
        return;
    }

    world::BlockId block_to_place = get_held_block();
    place_block(block_to_place);
}

void BlockInteractionSystem::place_block(world::BlockId block_id) {
    if (!target_.has_target) {
        return;
    }

    // Can't place air
    if (block_id == world::BLOCK_AIR) {
        return;
    }

    world::WorldBlockPos place_pos = target_.placement_position();

    // Validate placement
    if (!can_place_block(place_pos, block_id)) {
        return;
    }

    // Place the block
    world_manager_->set_block(place_pos, block_id, 0);

    // Trigger placed callback
    world::BlockRegistry::instance().on_block_placed(place_pos, block_id, 0);

    // Consume item from inventory (if using inventory, not override)
    if (inventory_ != nullptr && !use_held_block_override_) {
        inventory_->consume_held_item(1);
    }

    // Log the placement
    const auto* block_type = world::BlockRegistry::instance().get(block_id);
    const char* block_name = block_type ? block_type->get_name().c_str() : "unknown";
    REALCRAFT_LOG_DEBUG(core::log_category::GAME, "Block placed at ({}, {}, {}): {}", place_pos.x, place_pos.y,
                        place_pos.z, block_name);
}

bool BlockInteractionSystem::can_place_block(const world::WorldBlockPos& pos, world::BlockId block_id) const {
    // Check Y bounds
    if (pos.y < 0 || pos.y >= world::CHUNK_SIZE_Y) {
        return false;
    }

    // Check if position is already occupied by a solid block
    world::BlockId existing = world_manager_->get_block(pos);
    const auto* existing_type = world::BlockRegistry::instance().get(existing);
    if (existing_type && existing_type->is_solid() && !existing_type->is_replaceable()) {
        return false;
    }

    // Check if placement would intersect player
    physics::AABB block_aabb = physics::AABB::from_block_pos(pos);
    physics::AABB player_aabb = player_controller_->get_bounds();

    if (block_aabb.intersects(player_aabb)) {
        return false;
    }

    // Block type must be valid and solid
    const auto* block_type = world::BlockRegistry::instance().get(block_id);
    if (!block_type) {
        return false;
    }

    return true;
}

void BlockInteractionSystem::cycle_held_block(bool forward) {
    // If using inventory, cycle through hotbar instead
    if (inventory_ != nullptr && !use_held_block_override_) {
        inventory_->scroll_slot(forward ? 1 : -1);

        // Log the new selection
        auto held = inventory_->get_held_block();
        if (held.has_value()) {
            const auto* block_type = world::BlockRegistry::instance().get(held.value());
            const char* block_name = block_type ? block_type->get_name().c_str() : "unknown";
            REALCRAFT_LOG_INFO(core::log_category::GAME, "Selected slot {}: {}", inventory_->get_selected_slot() + 1,
                               block_name);
        } else {
            REALCRAFT_LOG_INFO(core::log_category::GAME, "Selected slot {}: empty",
                               inventory_->get_selected_slot() + 1);
        }
        return;
    }

    // Legacy override mode: cycle through all placeable blocks
    // Get list of placeable block IDs
    std::vector<world::BlockId> placeable_blocks;
    world::BlockRegistry::instance().for_each([&placeable_blocks](const world::BlockType& type) {
        // Only include solid, breakable blocks (not air, water, etc.)
        if (!type.is_air() && !type.is_liquid() && type.is_solid()) {
            placeable_blocks.push_back(type.get_id());
        }
    });

    if (placeable_blocks.empty()) {
        return;
    }

    // Sort for consistent ordering
    std::sort(placeable_blocks.begin(), placeable_blocks.end());

    // Find current block in list
    auto it = std::find(placeable_blocks.begin(), placeable_blocks.end(), held_block_override_);

    if (it == placeable_blocks.end()) {
        // Current block not in list, use first
        held_block_override_ = placeable_blocks.front();
    } else if (forward) {
        // Next block
        ++it;
        if (it == placeable_blocks.end()) {
            it = placeable_blocks.begin();
        }
        held_block_override_ = *it;
    } else {
        // Previous block
        if (it == placeable_blocks.begin()) {
            it = placeable_blocks.end();
        }
        --it;
        held_block_override_ = *it;
    }

    const auto* block_type = world::BlockRegistry::instance().get(held_block_override_);
    const char* block_name = block_type ? block_type->get_name().c_str() : "unknown";
    REALCRAFT_LOG_INFO(core::log_category::GAME, "Selected block: {}", block_name);
}

// ============================================================================
// Inventory Integration
// ============================================================================

void BlockInteractionSystem::set_inventory(Inventory* inventory) {
    inventory_ = inventory;
    if (inventory_ != nullptr) {
        // Switch to inventory mode when inventory is connected
        use_held_block_override_ = false;
        REALCRAFT_LOG_DEBUG(core::log_category::GAME, "BlockInteractionSystem: inventory connected");
    }
}

void BlockInteractionSystem::set_item_entity_manager(ItemEntityManager* item_manager) {
    item_entity_manager_ = item_manager;
    if (item_entity_manager_ != nullptr) {
        REALCRAFT_LOG_DEBUG(core::log_category::GAME, "BlockInteractionSystem: item entity manager connected");
    }
}

world::BlockId BlockInteractionSystem::get_held_block() const {
    // Use override if set
    if (use_held_block_override_) {
        return held_block_override_;
    }

    // Use inventory if available
    if (inventory_ != nullptr) {
        auto block_id = inventory_->get_held_block();
        if (block_id.has_value()) {
            return block_id.value();
        }
    }

    // Fallback to air (can't place)
    return world::BLOCK_AIR;
}

void BlockInteractionSystem::set_held_block_override(world::BlockId block_id) {
    held_block_override_ = block_id;
    use_held_block_override_ = true;
}

void BlockInteractionSystem::clear_held_block_override() {
    use_held_block_override_ = false;
}

}  // namespace realcraft::gameplay
