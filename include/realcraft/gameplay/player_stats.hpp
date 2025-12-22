// RealCraft Player Stats
// player_stats.hpp - Player health, hunger, and status effects

#pragma once

#include <cstdint>

namespace realcraft::gameplay {

// Status effect flags for future use
enum class StatusEffect : uint8_t {
    None = 0,
    Poison = 1 << 0,
    Regeneration = 1 << 1,
    Hunger = 1 << 2,
    Strength = 1 << 3,
    Weakness = 1 << 4,
    Speed = 1 << 5,
    Slowness = 1 << 6,
};

// Bitwise operators for StatusEffect
inline StatusEffect operator|(StatusEffect a, StatusEffect b) {
    return static_cast<StatusEffect>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline StatusEffect operator&(StatusEffect a, StatusEffect b) {
    return static_cast<StatusEffect>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline StatusEffect& operator|=(StatusEffect& a, StatusEffect b) {
    return a = a | b;
}

inline StatusEffect& operator&=(StatusEffect& a, StatusEffect b) {
    return a = a & b;
}

// Player stats for survival mode
// This is a stub implementation for UI display - full mechanics in Milestone 8.1
struct PlayerStats {
    // Health (0-20, displayed as 10 hearts)
    float health = 20.0f;
    float max_health = 20.0f;

    // Hunger (0-20, displayed as 10 drumsticks)
    float hunger = 20.0f;
    float max_hunger = 20.0f;

    // Saturation (hidden hunger buffer)
    float saturation = 5.0f;
    float max_saturation = 20.0f;

    // Active status effects
    StatusEffect active_effects = StatusEffect::None;

    // ========================================================================
    // Health Helpers
    // ========================================================================

    [[nodiscard]] bool is_full_health() const { return health >= max_health; }
    [[nodiscard]] bool is_dead() const { return health <= 0.0f; }
    [[nodiscard]] float get_health_percent() const { return max_health > 0.0f ? health / max_health : 0.0f; }

    // ========================================================================
    // Hunger Helpers
    // ========================================================================

    [[nodiscard]] bool is_full_hunger() const { return hunger >= max_hunger; }
    [[nodiscard]] bool is_starving() const { return hunger <= 0.0f; }
    [[nodiscard]] float get_hunger_percent() const { return max_hunger > 0.0f ? hunger / max_hunger : 0.0f; }

    // ========================================================================
    // Status Effect Helpers
    // ========================================================================

    [[nodiscard]] bool has_effect(StatusEffect effect) const { return (active_effects & effect) != StatusEffect::None; }

    void add_effect(StatusEffect effect) { active_effects |= effect; }

    void remove_effect(StatusEffect effect) {
        active_effects =
            static_cast<StatusEffect>(static_cast<uint8_t>(active_effects) & ~static_cast<uint8_t>(effect));
    }

    void clear_effects() { active_effects = StatusEffect::None; }
};

}  // namespace realcraft::gameplay
