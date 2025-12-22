// RealCraft Gameplay System
// item_entity.cpp - Dropped item entities implementation

#include <algorithm>
#include <cmath>
#include <random>
#include <realcraft/core/logger.hpp>
#include <realcraft/gameplay/item_entity.hpp>
#include <realcraft/physics/physics_world.hpp>
#include <realcraft/world/world_manager.hpp>

namespace realcraft::gameplay {

// ============================================================================
// Implementation Details
// ============================================================================

struct ItemEntityManager::Impl {
    std::vector<ItemEntity> entities;
    uint32_t next_id = 1;
    double current_time = 0.0;
    std::mt19937 rng{std::random_device{}()};
};

// ============================================================================
// ItemEntityManager Implementation
// ============================================================================

ItemEntityManager::ItemEntityManager() : impl_(std::make_unique<Impl>()) {}

ItemEntityManager::~ItemEntityManager() {
    if (initialized_) {
        shutdown();
    }
}

ItemEntityManager::ItemEntityManager(ItemEntityManager&&) noexcept = default;
ItemEntityManager& ItemEntityManager::operator=(ItemEntityManager&&) noexcept = default;

bool ItemEntityManager::initialize(physics::PhysicsWorld* physics_world, world::WorldManager* world_manager,
                                   Inventory* player_inventory, const ItemEntityConfig& config) {
    if (initialized_) {
        REALCRAFT_LOG_WARN(core::log_category::GAME, "ItemEntityManager already initialized");
        return true;
    }

    if (player_inventory == nullptr) {
        REALCRAFT_LOG_ERROR(core::log_category::GAME, "ItemEntityManager requires a valid player inventory");
        return false;
    }

    physics_world_ = physics_world;
    world_manager_ = world_manager;
    player_inventory_ = player_inventory;
    config_ = config;

    impl_->entities.reserve(config_.max_entities);
    initialized_ = true;

    REALCRAFT_LOG_INFO(core::log_category::GAME, "ItemEntityManager initialized (max {} entities)",
                       config_.max_entities);
    return true;
}

void ItemEntityManager::shutdown() {
    if (!initialized_) {
        return;
    }

    impl_->entities.clear();
    physics_world_ = nullptr;
    world_manager_ = nullptr;
    player_inventory_ = nullptr;
    initialized_ = false;

    REALCRAFT_LOG_DEBUG(core::log_category::GAME, "ItemEntityManager shutdown");
}

void ItemEntityManager::update(double dt, const glm::dvec3& player_position) {
    if (!initialized_) {
        return;
    }

    impl_->current_time += dt;

    // Update each entity
    for (auto& entity : impl_->entities) {
        if (entity.marked_for_removal) {
            continue;
        }

        // Update pickup availability
        if (!entity.can_pickup && (impl_->current_time - entity.spawn_time) >= ItemEntity::PICKUP_DELAY) {
            entity.can_pickup = true;
        }

        // Update physics
        if (config_.enable_physics) {
            update_physics(entity, dt);
        }

        // Update visual effects
        entity.rotation += ItemEntity::ROTATION_SPEED * static_cast<float>(dt);
        entity.bob_offset =
            std::sin(static_cast<float>(impl_->current_time) * ItemEntity::BOB_SPEED) * ItemEntity::BOB_AMPLITUDE;

        // Check for pickup
        update_pickup(entity, player_position, impl_->current_time);
    }

    // Merge nearby identical items
    if (config_.enable_merging) {
        update_merging();
    }

    // Remove expired/picked up entities
    cleanup_entities(impl_->current_time);
}

uint32_t ItemEntityManager::spawn_item(const glm::dvec3& position, const ItemStack& item, const glm::dvec3& velocity) {
    if (!initialized_ || item.is_empty()) {
        return 0;
    }

    // Check entity limit
    if (impl_->entities.size() >= config_.max_entities) {
        // Remove oldest entity to make room
        if (!impl_->entities.empty()) {
            impl_->entities.erase(impl_->entities.begin());
        }
    }

    ItemEntity entity;
    entity.id = impl_->next_id++;
    entity.position = position;
    entity.velocity = velocity;
    entity.item = item;
    entity.spawn_time = impl_->current_time;
    entity.lifetime = config_.default_lifetime;
    entity.rotation = 0.0f;
    entity.bob_offset = 0.0f;
    entity.on_ground = false;
    entity.can_pickup = false;
    entity.marked_for_removal = false;

    impl_->entities.push_back(entity);

    REALCRAFT_LOG_DEBUG(core::log_category::GAME, "Spawned item entity {} at ({:.1f}, {:.1f}, {:.1f})", entity.id,
                        position.x, position.y, position.z);

    return entity.id;
}

void ItemEntityManager::spawn_block_drops(const glm::dvec3& position, world::BlockId block_id) {
    // Create item stack for this block
    ItemStack item = ItemRegistry::instance().create_block_item(block_id, 1);
    if (item.is_empty()) {
        REALCRAFT_LOG_DEBUG(core::log_category::GAME, "No item drop for block ID {}", block_id);
        return;
    }

    // Spawn at block center with slight upward velocity
    glm::dvec3 spawn_pos = position + glm::dvec3(0.5, 0.3, 0.5);
    spawn_item_scattered(spawn_pos, item);
}

uint32_t ItemEntityManager::spawn_item_scattered(const glm::dvec3& position, const ItemStack& item) {
    // Random velocity for visual variety
    std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * 3.14159265358979323846);
    std::uniform_real_distribution<double> speed_dist(1.0, 3.0);
    std::uniform_real_distribution<double> up_dist(2.0, 4.0);

    double angle = angle_dist(impl_->rng);
    double speed = speed_dist(impl_->rng);
    double up = up_dist(impl_->rng);

    glm::dvec3 velocity{std::cos(angle) * speed, up, std::sin(angle) * speed};

    return spawn_item(position, item, velocity);
}

size_t ItemEntityManager::entity_count() const {
    return impl_->entities.size();
}

const std::vector<ItemEntity>& ItemEntityManager::get_entities() const {
    return impl_->entities;
}

const ItemEntity* ItemEntityManager::find_entity(uint32_t id) const {
    for (const auto& entity : impl_->entities) {
        if (entity.id == id) {
            return &entity;
        }
    }
    return nullptr;
}

bool ItemEntityManager::pickup_entity(uint32_t id) {
    for (auto& entity : impl_->entities) {
        if (entity.id == id && !entity.marked_for_removal) {
            // Try to add to inventory
            ItemStack remaining = player_inventory_->add_item(entity.item);
            if (remaining.is_empty()) {
                entity.marked_for_removal = true;
                REALCRAFT_LOG_DEBUG(core::log_category::GAME, "Picked up item entity {}", id);
                return true;
            }
            // Partial pickup
            entity.item = remaining;
            return false;
        }
    }
    return false;
}

void ItemEntityManager::remove_entity(uint32_t id) {
    for (auto& entity : impl_->entities) {
        if (entity.id == id) {
            entity.marked_for_removal = true;
            return;
        }
    }
}

void ItemEntityManager::clear() {
    impl_->entities.clear();
}

// ============================================================================
// Private Methods
// ============================================================================

void ItemEntityManager::update_physics(ItemEntity& entity, double dt) {
    // Apply gravity if not on ground
    if (!entity.on_ground) {
        entity.velocity.y -= ItemEntity::GRAVITY * dt;
    }

    // Apply air resistance
    entity.velocity *= (1.0 - ItemEntity::AIR_RESISTANCE);

    // Update position
    glm::dvec3 new_position = entity.position + entity.velocity * dt;

    // Check ground collision
    if (check_ground_collision(entity)) {
        entity.on_ground = true;
        entity.velocity.y = 0.0;
        entity.velocity.x *= ItemEntity::GROUND_FRICTION;
        entity.velocity.z *= ItemEntity::GROUND_FRICTION;

        // Stop movement if slow enough
        if (glm::length(entity.velocity) < 0.1) {
            entity.velocity = glm::dvec3(0.0);
        }
    } else {
        entity.on_ground = false;
        entity.position = new_position;
    }
}

void ItemEntityManager::update_pickup(ItemEntity& entity, const glm::dvec3& player_position, double current_time) {
    if (!entity.can_pickup || entity.marked_for_removal) {
        return;
    }

    // Check distance to player
    double distance = glm::length(entity.position - player_position);
    if (distance > config_.pickup_radius) {
        return;
    }

    // Try to add to inventory
    ItemStack remaining = player_inventory_->add_item(entity.item);
    if (remaining.is_empty()) {
        entity.marked_for_removal = true;
        REALCRAFT_LOG_DEBUG(core::log_category::GAME, "Auto-picked up item entity {} ({}x)", entity.id,
                            entity.item.count);
    } else if (remaining.count < entity.item.count) {
        // Partial pickup
        REALCRAFT_LOG_DEBUG(core::log_category::GAME, "Partial pickup of item entity {}: {} -> {}", entity.id,
                            entity.item.count, remaining.count);
        entity.item = remaining;
    }
}

void ItemEntityManager::update_merging() {
    // Check pairs of entities for merging
    for (size_t i = 0; i < impl_->entities.size(); ++i) {
        ItemEntity& a = impl_->entities[i];
        if (a.marked_for_removal || !a.can_pickup) {
            continue;
        }

        for (size_t j = i + 1; j < impl_->entities.size(); ++j) {
            ItemEntity& b = impl_->entities[j];
            if (b.marked_for_removal || !b.can_pickup) {
                continue;
            }

            // Check if same item type and can stack
            if (a.item.item_id != b.item.item_id) {
                continue;
            }
            if (!a.item.can_merge(b.item)) {
                continue;
            }

            // Check distance
            double distance = glm::length(a.position - b.position);
            if (distance > ItemEntity::MERGE_RADIUS) {
                continue;
            }

            // Merge b into a
            uint16_t overflow = a.item.add(b.item.count);
            if (overflow == 0) {
                // Fully merged
                b.marked_for_removal = true;
            } else {
                // Partial merge
                b.item.count = overflow;
            }
        }
    }
}

void ItemEntityManager::cleanup_entities(double current_time) {
    impl_->entities.erase(std::remove_if(impl_->entities.begin(), impl_->entities.end(),
                                         [current_time](const ItemEntity& entity) {
                                             // Remove if marked or expired
                                             if (entity.marked_for_removal) {
                                                 return true;
                                             }
                                             if ((current_time - entity.spawn_time) >= entity.lifetime) {
                                                 REALCRAFT_LOG_DEBUG(core::log_category::GAME,
                                                                     "Item entity {} despawned (expired)", entity.id);
                                                 return true;
                                             }
                                             return false;
                                         }),
                          impl_->entities.end());
}

bool ItemEntityManager::check_ground_collision(ItemEntity& entity) {
    // Simple ground check - check block below
    glm::dvec3 feet_pos = entity.position - glm::dvec3(0.0, 0.1, 0.0);

    if (is_position_solid(feet_pos)) {
        // Snap to top of block
        entity.position.y = std::floor(entity.position.y) + 0.25;  // Slight offset for visual
        return true;
    }

    return false;
}

bool ItemEntityManager::is_position_solid(const glm::dvec3& position) const {
    if (world_manager_ == nullptr) {
        return false;
    }

    world::WorldBlockPos block_pos{static_cast<int64_t>(std::floor(position.x)),
                                   static_cast<int64_t>(std::floor(position.y)),
                                   static_cast<int64_t>(std::floor(position.z))};

    world::BlockId block_id = world_manager_->get_block(block_pos);

    // Check if block is solid
    const world::BlockType* block_type = world::BlockRegistry::instance().get(block_id);
    return block_type != nullptr && block_type->is_solid() && !block_type->is_liquid();
}

}  // namespace realcraft::gameplay
