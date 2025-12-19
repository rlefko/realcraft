// RealCraft World System
// chunk.cpp - Chunk class implementation

#include <chrono>
#include <realcraft/core/logger.hpp>
#include <realcraft/world/chunk.hpp>

namespace realcraft::world {

// ============================================================================
// Chunk Implementation
// ============================================================================

Chunk::Chunk(const ChunkDesc& desc) : position_(desc.position), storage_() {
    metadata_.generation_seed = desc.seed;

    if (!desc.debug_name.empty()) {
        REALCRAFT_LOG_DEBUG(core::log_category::WORLD, "Created chunk {} at ({}, {})", desc.debug_name, position_.x,
                            position_.y);
    }
}

Chunk::~Chunk() = default;

WorldBlockPos Chunk::get_world_origin() const {
    return local_to_world(position_, LocalBlockPos(0, 0, 0));
}

ChunkState Chunk::get_state() const {
    return state_.load(std::memory_order_acquire);
}

void Chunk::set_state(ChunkState state) {
    state_.store(state, std::memory_order_release);
}

bool Chunk::is_ready() const {
    ChunkState s = get_state();
    return s == ChunkState::Loaded || s == ChunkState::Modified;
}

bool Chunk::is_dirty() const {
    return dirty_.load(std::memory_order_acquire);
}

void Chunk::mark_dirty() {
    dirty_.store(true, std::memory_order_release);
    if (get_state() == ChunkState::Loaded) {
        set_state(ChunkState::Modified);
    }
    metadata_.has_player_modifications = true;
    metadata_.last_modified_time =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void Chunk::mark_clean() {
    dirty_.store(false, std::memory_order_release);
    if (get_state() == ChunkState::Modified) {
        set_state(ChunkState::Loaded);
    }
}

BlockId Chunk::get_block(const LocalBlockPos& pos) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return storage_.get_block(pos);
}

BlockStateId Chunk::get_block_state(const LocalBlockPos& pos) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return storage_.get_state(pos);
}

PaletteEntry Chunk::get_entry(const LocalBlockPos& pos) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return storage_.get(pos);
}

void Chunk::set_block(const LocalBlockPos& pos, BlockId id, BlockStateId state) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    storage_.set_block(pos, id, state);
    mark_dirty();
}

void Chunk::set_entry(const LocalBlockPos& pos, const PaletteEntry& entry) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    storage_.set(pos, entry);
    mark_dirty();
}

Chunk::ReadLock Chunk::read_lock() const {
    return ReadLock(*this);
}

Chunk::WriteLock Chunk::write_lock() {
    return WriteLock(*this);
}

void Chunk::set_neighbor(HorizontalDirection dir, Chunk* neighbor) {
    // NOLINTNEXTLINE(clang-analyzer-core.StackAddressEscape)
    // Design contract: neighbor must be nullptr or a heap-allocated Chunk
    // managed by WorldManager. WorldManager ensures proper lifetime management
    // by clearing neighbor links in unload_chunk() before chunk destruction.
    neighbors_[static_cast<size_t>(dir)] = neighbor;
}

Chunk* Chunk::get_neighbor(HorizontalDirection dir) const {
    return neighbors_[static_cast<size_t>(dir)];
}

bool Chunk::has_all_neighbors() const {
    for (size_t i = 0; i < static_cast<size_t>(HorizontalDirection::Count); ++i) {
        if (neighbors_[i] == nullptr) {
            return false;
        }
    }
    return true;
}

std::optional<PaletteEntry> Chunk::get_entry_or_neighbor(const LocalBlockPos& pos) const {
    // Check if within this chunk
    if (is_valid_local(pos)) {
        return get_entry(pos);
    }

    // Determine which neighbor to check
    LocalBlockPos adjusted = pos;
    Chunk* neighbor = nullptr;

    if (pos.x < 0) {
        neighbor = neighbors_[static_cast<size_t>(HorizontalDirection::NegX)];
        adjusted.x += CHUNK_SIZE_X;
    } else if (pos.x >= CHUNK_SIZE_X) {
        neighbor = neighbors_[static_cast<size_t>(HorizontalDirection::PosX)];
        adjusted.x -= CHUNK_SIZE_X;
    } else if (pos.z < 0) {
        neighbor = neighbors_[static_cast<size_t>(HorizontalDirection::NegZ)];
        adjusted.z += CHUNK_SIZE_Z;
    } else if (pos.z >= CHUNK_SIZE_Z) {
        neighbor = neighbors_[static_cast<size_t>(HorizontalDirection::PosZ)];
        adjusted.z -= CHUNK_SIZE_Z;
    }

    // Y out of bounds - return air for above, invalid for below
    if (pos.y < 0 || pos.y >= CHUNK_SIZE_Y) {
        return PaletteEntry::air();
    }

    if (neighbor == nullptr) {
        return std::nullopt;
    }

    return neighbor->get_entry(adjusted);
}

std::vector<uint8_t> Chunk::serialize() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return storage_.serialize_rle();
}

bool Chunk::deserialize(std::span<const uint8_t> data) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    return storage_.deserialize_rle(data);
}

size_t Chunk::memory_usage() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return sizeof(Chunk) + storage_.memory_usage();
}

size_t Chunk::non_air_count() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return storage_.non_air_count();
}

// ============================================================================
// ReadLock Implementation
// ============================================================================

Chunk::ReadLock::ReadLock(const Chunk& chunk) : chunk_(&chunk), lock_(chunk.mutex_) {}

Chunk::ReadLock::~ReadLock() = default;

Chunk::ReadLock::ReadLock(ReadLock&& other) noexcept : chunk_(other.chunk_), lock_(std::move(other.lock_)) {
    other.chunk_ = nullptr;
}

BlockId Chunk::ReadLock::get_block(const LocalBlockPos& pos) const {
    return chunk_->storage_.get_block(pos);
}

BlockStateId Chunk::ReadLock::get_block_state(const LocalBlockPos& pos) const {
    return chunk_->storage_.get_state(pos);
}

PaletteEntry Chunk::ReadLock::get_entry(const LocalBlockPos& pos) const {
    return chunk_->storage_.get(pos);
}

PaletteEntry Chunk::ReadLock::get_entry(size_t index) const {
    return chunk_->storage_.get(index);
}

size_t Chunk::ReadLock::non_air_count() const {
    return chunk_->storage_.non_air_count();
}

bool Chunk::ReadLock::is_empty() const {
    return chunk_->storage_.is_empty();
}

const VoxelStorage& Chunk::ReadLock::storage() const {
    return chunk_->storage_;
}

// ============================================================================
// WriteLock Implementation
// ============================================================================

Chunk::WriteLock::WriteLock(Chunk& chunk) : chunk_(&chunk), lock_(chunk.mutex_) {}

Chunk::WriteLock::~WriteLock() = default;

Chunk::WriteLock::WriteLock(WriteLock&& other) noexcept : chunk_(other.chunk_), lock_(std::move(other.lock_)) {
    other.chunk_ = nullptr;
}

BlockId Chunk::WriteLock::get_block(const LocalBlockPos& pos) const {
    return chunk_->storage_.get_block(pos);
}

PaletteEntry Chunk::WriteLock::get_entry(const LocalBlockPos& pos) const {
    return chunk_->storage_.get(pos);
}

void Chunk::WriteLock::set_block(const LocalBlockPos& pos, BlockId id, BlockStateId state) {
    chunk_->storage_.set_block(pos, id, state);
    chunk_->dirty_.store(true, std::memory_order_release);
}

void Chunk::WriteLock::set_entry(const LocalBlockPos& pos, const PaletteEntry& entry) {
    chunk_->storage_.set(pos, entry);
    chunk_->dirty_.store(true, std::memory_order_release);
}

void Chunk::WriteLock::set_entry(size_t index, const PaletteEntry& entry) {
    chunk_->storage_.set(index, entry);
    chunk_->dirty_.store(true, std::memory_order_release);
}

void Chunk::WriteLock::fill(const PaletteEntry& entry) {
    chunk_->storage_.fill(entry);
    chunk_->dirty_.store(true, std::memory_order_release);
}

void Chunk::WriteLock::fill_region(const LocalBlockPos& min, const LocalBlockPos& max, const PaletteEntry& entry) {
    chunk_->storage_.fill_region(min, max, entry);
    chunk_->dirty_.store(true, std::memory_order_release);
}

VoxelStorage& Chunk::WriteLock::storage() {
    return chunk_->storage_;
}

}  // namespace realcraft::world
