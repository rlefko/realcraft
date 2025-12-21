// RealCraft Physics Engine
// impact_damage.cpp - Default impact damage handling implementation

#include <realcraft/core/logger.hpp>
#include <realcraft/physics/impact_damage.hpp>
#include <realcraft/world/block.hpp>
#include <realcraft/world/world_manager.hpp>

namespace realcraft::physics {

// ============================================================================
// DefaultImpactDamageHandler Implementation
// ============================================================================

DefaultImpactDamageHandler::DefaultImpactDamageHandler(world::WorldManager* world_manager)
    : world_manager_(world_manager) {}

bool DefaultImpactDamageHandler::on_block_impact(const world::WorldBlockPos& position, world::BlockId block_id,
                                                 double damage_amount, const glm::dvec3& impact_direction) {
    if (!world_manager_) {
        return false;
    }

    // Get block type info
    const auto& registry = world::BlockRegistry::instance();
    const world::BlockType* block_type = registry.get(block_id);

    if (!block_type) {
        return false;
    }

    // Skip air and fluids
    if (block_id == world::BLOCK_AIR || block_type->is_liquid()) {
        return false;
    }

    // Skip bedrock (indestructible)
    if (block_id == registry.find_id("bedrock")) {
        return false;
    }

    // Get block's impact resistance
    const auto& material = block_type->get_material();
    double impact_resistance = material.impact_resistance;

    // Calculate effective damage
    // Brittle materials take more damage
    double brittleness_modifier = 1.0 + static_cast<double>(material.brittleness);
    double effective_damage = damage_amount * brittleness_modifier / impact_resistance;

    // Check if block should break
    if (effective_damage < minimum_break_damage_) {
        return false;
    }

    // Break the block
    world::ChunkPos chunk_pos = world::world_to_chunk(position);
    world::Chunk* chunk = world_manager_->get_chunk(chunk_pos);

    if (!chunk) {
        return false;
    }

    world::LocalBlockPos local_pos = world::world_to_local(position);

    // Set block to air
    chunk->set_block(local_pos, world::BLOCK_AIR);

    REALCRAFT_LOG_DEBUG(core::log_category::PHYSICS,
                        "Debris impact destroyed block {} at ({}, {}, {}) with damage {:.1f}", block_type->get_name(),
                        position.x, position.y, position.z, effective_damage);

    return true;
}

void DefaultImpactDamageHandler::on_entity_impact(uint32_t entity_id, double damage_amount,
                                                  const glm::dvec3& impact_point, const glm::dvec3& impact_direction) {
    // Phase 8: Entity system not yet implemented
    // This method is a placeholder for future entity damage handling

    REALCRAFT_LOG_DEBUG(core::log_category::PHYSICS,
                        "Entity {} hit by debris at ({:.1f}, {:.1f}, {:.1f}) for {:.1f} damage", entity_id,
                        impact_point.x, impact_point.y, impact_point.z, damage_amount);
}

void DefaultImpactDamageHandler::on_player_impact(double damage_amount, const glm::dvec3& impact_point,
                                                  const glm::dvec3& impact_direction) {
    // Phase 8: Player health system not yet implemented
    // This method is a placeholder for future player damage handling

    REALCRAFT_LOG_DEBUG(core::log_category::PHYSICS,
                        "Player hit by debris at ({:.1f}, {:.1f}, {:.1f}) for {:.1f} damage", impact_point.x,
                        impact_point.y, impact_point.z, damage_amount);
}

// ============================================================================
// Configuration
// ============================================================================

void DefaultImpactDamageHandler::set_minimum_break_damage(double damage) {
    minimum_break_damage_ = damage;
}

double DefaultImpactDamageHandler::get_minimum_break_damage() const {
    return minimum_break_damage_;
}

void DefaultImpactDamageHandler::set_trigger_structural_checks(bool enabled) {
    trigger_structural_checks_ = enabled;
}

bool DefaultImpactDamageHandler::get_trigger_structural_checks() const {
    return trigger_structural_checks_;
}

}  // namespace realcraft::physics
