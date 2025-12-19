// RealCraft World System
// origin_shifter.hpp - Origin shifting system for floating-point precision

#pragma once

#include "types.hpp"

#include <functional>
#include <memory>

namespace realcraft::world {

// ============================================================================
// Origin Shift Configuration
// ============================================================================

struct OriginShiftConfig {
    double shift_threshold = 4096.0;     // Shift when player exceeds this distance from origin
    double min_shift_distance = 2048.0;  // Minimum shift amount
    bool enabled = true;
};

// ============================================================================
// Origin Shifter
// ============================================================================

class OriginShifter {
public:
    OriginShifter();
    ~OriginShifter();

    // Non-copyable
    OriginShifter(const OriginShifter&) = delete;
    OriginShifter& operator=(const OriginShifter&) = delete;

    // ========================================================================
    // Configuration
    // ========================================================================

    void configure(const OriginShiftConfig& config);
    [[nodiscard]] const OriginShiftConfig& get_config() const;

    // ========================================================================
    // Current Origin
    // ========================================================================

    // Current origin offset (in world block coordinates)
    [[nodiscard]] WorldBlockPos get_origin_offset() const;

    // ========================================================================
    // Update
    // ========================================================================

    // Check if shift is needed and perform if necessary
    // Returns true if origin was shifted
    bool update(const WorldPos& player_position);

    // ========================================================================
    // Coordinate Conversion
    // ========================================================================

    // Convert world position to position relative to current origin
    [[nodiscard]] glm::dvec3 world_to_relative(const WorldPos& world) const;

    // Convert relative position back to world position
    [[nodiscard]] WorldPos relative_to_world(const glm::dvec3& relative) const;

    // Convert world position to render coordinates (float for GPU)
    [[nodiscard]] glm::vec3 world_to_render(const WorldPos& world) const;

    // Convert world block position to render coordinates
    [[nodiscard]] glm::vec3 block_to_render(const WorldBlockPos& block) const;

    // ========================================================================
    // Callbacks
    // ========================================================================

    // Callback when origin shifts
    using ShiftCallback = std::function<void(const WorldBlockPos& old_origin, const WorldBlockPos& new_origin)>;
    void set_shift_callback(ShiftCallback callback);

    // ========================================================================
    // Manual Control
    // ========================================================================

    // Force a shift to specific origin (useful for teleportation)
    void force_shift(const WorldBlockPos& new_origin);

    // Reset to origin (0,0,0)
    void reset();

    // ========================================================================
    // Statistics
    // ========================================================================

    // Number of shifts performed
    [[nodiscard]] uint32_t get_shift_count() const;

    // Distance from current render origin (useful for debugging)
    [[nodiscard]] double get_distance_from_origin(const WorldPos& pos) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::world
