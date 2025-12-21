// RealCraft Physics Engine
// impact_damage.hpp - Interface and implementation for impact damage handling

#pragma once

#include <glm/glm.hpp>

#include <realcraft/world/types.hpp>

namespace realcraft::world {
class WorldManager;
}

namespace realcraft::physics {

// ============================================================================
// Impact Damage Handler Interface
// ============================================================================

class IImpactDamageHandler {
public:
    virtual ~IImpactDamageHandler() = default;

    // Handle impact damage to a block
    // Returns true if the block was destroyed
    virtual bool on_block_impact(const world::WorldBlockPos& position, world::BlockId block_id, double damage_amount,
                                 const glm::dvec3& impact_direction) = 0;

    // Handle impact damage to an entity (Phase 8 preparation)
    // entity_id is the unique identifier for the entity
    virtual void on_entity_impact(uint32_t entity_id, double damage_amount, const glm::dvec3& impact_point,
                                  const glm::dvec3& impact_direction) = 0;

    // Handle impact damage to the player (Phase 8 preparation)
    virtual void on_player_impact(double damage_amount, const glm::dvec3& impact_point,
                                  const glm::dvec3& impact_direction) = 0;
};

// ============================================================================
// Default Impact Damage Handler
// ============================================================================

class DefaultImpactDamageHandler : public IImpactDamageHandler {
public:
    explicit DefaultImpactDamageHandler(world::WorldManager* world_manager);
    ~DefaultImpactDamageHandler() override = default;

    // ========================================================================
    // IImpactDamageHandler Implementation
    // ========================================================================

    bool on_block_impact(const world::WorldBlockPos& position, world::BlockId block_id, double damage_amount,
                         const glm::dvec3& impact_direction) override;

    void on_entity_impact(uint32_t entity_id, double damage_amount, const glm::dvec3& impact_point,
                          const glm::dvec3& impact_direction) override;

    void on_player_impact(double damage_amount, const glm::dvec3& impact_point,
                          const glm::dvec3& impact_direction) override;

    // ========================================================================
    // Configuration
    // ========================================================================

    // Set minimum damage required to break a block (default: 100.0)
    void set_minimum_break_damage(double damage);
    [[nodiscard]] double get_minimum_break_damage() const;

    // Set whether to trigger structural integrity checks after breaking blocks
    void set_trigger_structural_checks(bool enabled);
    [[nodiscard]] bool get_trigger_structural_checks() const;

private:
    world::WorldManager* world_manager_;
    double minimum_break_damage_ = 100.0;
    bool trigger_structural_checks_ = true;
};

}  // namespace realcraft::physics
