// RealCraft World System
// origin_shifter.cpp - Origin shifting implementation

#include <cmath>
#include <realcraft/core/logger.hpp>
#include <realcraft/world/origin_shifter.hpp>

namespace realcraft::world {

// ============================================================================
// OriginShifter Implementation
// ============================================================================

struct OriginShifter::Impl {
    OriginShiftConfig config;
    WorldBlockPos origin_offset{0, 0, 0};
    ShiftCallback shift_callback;
    uint32_t shift_count = 0;
};

OriginShifter::OriginShifter() : impl_(std::make_unique<Impl>()) {}

OriginShifter::~OriginShifter() = default;

void OriginShifter::configure(const OriginShiftConfig& config) {
    impl_->config = config;
}

const OriginShiftConfig& OriginShifter::get_config() const {
    return impl_->config;
}

WorldBlockPos OriginShifter::get_origin_offset() const {
    return impl_->origin_offset;
}

bool OriginShifter::update(const WorldPos& player_position) {
    if (!impl_->config.enabled) {
        return false;
    }

    // Calculate distance from current origin
    double dx = player_position.x - static_cast<double>(impl_->origin_offset.x);
    double dy = player_position.y - static_cast<double>(impl_->origin_offset.y);
    double dz = player_position.z - static_cast<double>(impl_->origin_offset.z);
    double distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (distance < impl_->config.shift_threshold) {
        return false;
    }

    // Calculate new origin (snap to block grid, centered on player)
    WorldBlockPos old_origin = impl_->origin_offset;
    WorldBlockPos new_origin;

    // Shift to player position, but maintain some buffer
    new_origin.x = static_cast<int64_t>(std::floor(player_position.x));
    new_origin.y = static_cast<int64_t>(std::floor(player_position.y));
    new_origin.z = static_cast<int64_t>(std::floor(player_position.z));

    // Round to nearest minimum shift distance for stability
    int64_t shift_unit = static_cast<int64_t>(impl_->config.min_shift_distance);
    new_origin.x = (new_origin.x / shift_unit) * shift_unit;
    new_origin.y = (new_origin.y / shift_unit) * shift_unit;
    new_origin.z = (new_origin.z / shift_unit) * shift_unit;

    impl_->origin_offset = new_origin;
    impl_->shift_count++;

    REALCRAFT_LOG_DEBUG(core::log_category::WORLD, "Origin shifted from ({}, {}, {}) to ({}, {}, {})", old_origin.x,
                        old_origin.y, old_origin.z, new_origin.x, new_origin.y, new_origin.z);

    // Notify callback
    if (impl_->shift_callback) {
        impl_->shift_callback(old_origin, new_origin);
    }

    return true;
}

glm::dvec3 OriginShifter::world_to_relative(const WorldPos& world) const {
    return glm::dvec3(world.x - static_cast<double>(impl_->origin_offset.x),
                      world.y - static_cast<double>(impl_->origin_offset.y),
                      world.z - static_cast<double>(impl_->origin_offset.z));
}

WorldPos OriginShifter::relative_to_world(const glm::dvec3& relative) const {
    return WorldPos(relative.x + static_cast<double>(impl_->origin_offset.x),
                    relative.y + static_cast<double>(impl_->origin_offset.y),
                    relative.z + static_cast<double>(impl_->origin_offset.z));
}

glm::vec3 OriginShifter::world_to_render(const WorldPos& world) const {
    glm::dvec3 relative = world_to_relative(world);
    return glm::vec3(static_cast<float>(relative.x), static_cast<float>(relative.y), static_cast<float>(relative.z));
}

glm::vec3 OriginShifter::block_to_render(const WorldBlockPos& block) const {
    return glm::vec3(static_cast<float>(block.x - impl_->origin_offset.x),
                     static_cast<float>(block.y - impl_->origin_offset.y),
                     static_cast<float>(block.z - impl_->origin_offset.z));
}

void OriginShifter::set_shift_callback(ShiftCallback callback) {
    impl_->shift_callback = std::move(callback);
}

void OriginShifter::force_shift(const WorldBlockPos& new_origin) {
    WorldBlockPos old_origin = impl_->origin_offset;
    impl_->origin_offset = new_origin;
    impl_->shift_count++;

    if (impl_->shift_callback) {
        impl_->shift_callback(old_origin, new_origin);
    }
}

void OriginShifter::reset() {
    force_shift(WorldBlockPos(0, 0, 0));
}

uint32_t OriginShifter::get_shift_count() const {
    return impl_->shift_count;
}

double OriginShifter::get_distance_from_origin(const WorldPos& pos) const {
    glm::dvec3 relative = world_to_relative(pos);
    return std::sqrt(relative.x * relative.x + relative.y * relative.y + relative.z * relative.z);
}

}  // namespace realcraft::world
