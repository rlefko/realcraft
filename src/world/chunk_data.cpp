// RealCraft World System
// chunk_data.cpp - Palette-based voxel storage implementation

#include <algorithm>
#include <cstring>
#include <realcraft/core/logger.hpp>
#include <realcraft/world/chunk_data.hpp>
#include <unordered_map>

namespace realcraft::world {

// ============================================================================
// VoxelStorage Implementation
// ============================================================================

struct VoxelStorage::Impl {
    std::vector<PaletteEntry> palette;
    std::vector<uint16_t> indices;  // Index into palette for each voxel

    Impl() {
        // Initialize with air only
        palette.push_back(PaletteEntry::air());
        // All voxels start as air (index 0)
        indices.resize(CHUNK_VOLUME, 0);
    }

    // Get or create palette index for entry
    uint16_t get_or_create_palette_index(const PaletteEntry& entry) {
        // Search for existing entry
        for (size_t i = 0; i < palette.size(); ++i) {
            if (palette[i] == entry) {
                return static_cast<uint16_t>(i);
            }
        }

        // Add new entry
        if (palette.size() >= UINT16_MAX) {
            REALCRAFT_LOG_ERROR(core::log_category::WORLD, "Palette overflow in VoxelStorage");
            return 0;  // Fallback to air
        }

        palette.push_back(entry);
        return static_cast<uint16_t>(palette.size() - 1);
    }
};

VoxelStorage::VoxelStorage() : impl_(std::make_unique<Impl>()) {}

VoxelStorage::~VoxelStorage() = default;

VoxelStorage::VoxelStorage(VoxelStorage&&) noexcept = default;
VoxelStorage& VoxelStorage::operator=(VoxelStorage&&) noexcept = default;

PaletteEntry VoxelStorage::get(const LocalBlockPos& pos) const {
    if (!is_valid_local(pos)) {
        return PaletteEntry::air();
    }
    return get(local_to_index(pos));
}

PaletteEntry VoxelStorage::get(size_t index) const {
    if (index >= CHUNK_VOLUME) {
        return PaletteEntry::air();
    }
    uint16_t palette_idx = impl_->indices[index];
    if (palette_idx >= impl_->palette.size()) {
        return PaletteEntry::air();
    }
    return impl_->palette[palette_idx];
}

void VoxelStorage::set(const LocalBlockPos& pos, const PaletteEntry& entry) {
    if (!is_valid_local(pos)) {
        return;
    }
    set(local_to_index(pos), entry);
}

void VoxelStorage::set(size_t index, const PaletteEntry& entry) {
    if (index >= CHUNK_VOLUME) {
        return;
    }
    uint16_t palette_idx = impl_->get_or_create_palette_index(entry);
    impl_->indices[index] = palette_idx;
}

BlockId VoxelStorage::get_block(const LocalBlockPos& pos) const {
    return get(pos).block_id;
}

BlockStateId VoxelStorage::get_state(const LocalBlockPos& pos) const {
    return get(pos).state_id;
}

void VoxelStorage::set_block(const LocalBlockPos& pos, BlockId id, BlockStateId state) {
    set(pos, PaletteEntry{id, state});
}

void VoxelStorage::fill(const PaletteEntry& entry) {
    impl_->palette.clear();
    impl_->palette.push_back(entry);
    std::fill(impl_->indices.begin(), impl_->indices.end(), static_cast<uint16_t>(0));
}

void VoxelStorage::fill_region(const LocalBlockPos& min, const LocalBlockPos& max, const PaletteEntry& entry) {
    uint16_t palette_idx = impl_->get_or_create_palette_index(entry);

    for (int32_t y = min.y; y <= max.y; ++y) {
        for (int32_t z = min.z; z <= max.z; ++z) {
            for (int32_t x = min.x; x <= max.x; ++x) {
                LocalBlockPos pos(x, y, z);
                if (is_valid_local(pos)) {
                    impl_->indices[local_to_index(pos)] = palette_idx;
                }
            }
        }
    }
}

size_t VoxelStorage::palette_size() const {
    return impl_->palette.size();
}

const std::vector<PaletteEntry>& VoxelStorage::get_palette() const {
    return impl_->palette;
}

void VoxelStorage::optimize_palette() {
    // Count usage of each palette entry
    std::vector<size_t> usage(impl_->palette.size(), 0);
    for (uint16_t idx : impl_->indices) {
        if (idx < usage.size()) {
            ++usage[idx];
        }
    }

    // Build new palette with only used entries
    std::vector<PaletteEntry> new_palette;
    std::vector<uint16_t> old_to_new(impl_->palette.size(), 0);

    for (size_t i = 0; i < impl_->palette.size(); ++i) {
        if (usage[i] > 0) {
            old_to_new[i] = static_cast<uint16_t>(new_palette.size());
            new_palette.push_back(impl_->palette[i]);
        }
    }

    // Remap indices
    for (uint16_t& idx : impl_->indices) {
        idx = old_to_new[idx];
    }

    impl_->palette = std::move(new_palette);
}

bool VoxelStorage::is_empty() const {
    // Empty if palette only contains air and all indices point to it
    if (impl_->palette.size() == 1 && impl_->palette[0].block_id == BLOCK_AIR) {
        return true;
    }

    // Check if any non-air blocks exist
    for (uint16_t idx : impl_->indices) {
        if (idx < impl_->palette.size() && impl_->palette[idx].block_id != BLOCK_AIR) {
            return false;
        }
    }
    return true;
}

bool VoxelStorage::is_uniform() const {
    if (impl_->indices.empty()) {
        return true;
    }

    uint16_t first = impl_->indices[0];
    for (uint16_t idx : impl_->indices) {
        if (idx != first) {
            return false;
        }
    }
    return true;
}

size_t VoxelStorage::non_air_count() const {
    size_t count = 0;
    for (uint16_t idx : impl_->indices) {
        if (idx < impl_->palette.size() && impl_->palette[idx].block_id != BLOCK_AIR) {
            ++count;
        }
    }
    return count;
}

size_t VoxelStorage::unique_block_count() const {
    std::unordered_map<BlockId, bool> seen;
    for (const auto& entry : impl_->palette) {
        seen[entry.block_id] = true;
    }
    return seen.size();
}

std::vector<uint8_t> VoxelStorage::serialize_rle() const {
    std::vector<uint8_t> data;

    // Header: palette size (2 bytes)
    uint16_t palette_size_val = static_cast<uint16_t>(impl_->palette.size());
    data.push_back(static_cast<uint8_t>(palette_size_val & 0xFF));
    data.push_back(static_cast<uint8_t>((palette_size_val >> 8) & 0xFF));

    // Palette entries (4 bytes each: block_id + state_id)
    for (const auto& entry : impl_->palette) {
        data.push_back(static_cast<uint8_t>(entry.block_id & 0xFF));
        data.push_back(static_cast<uint8_t>((entry.block_id >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>(entry.state_id & 0xFF));
        data.push_back(static_cast<uint8_t>((entry.state_id >> 8) & 0xFF));
    }

    // RLE-encoded indices
    if (!impl_->indices.empty()) {
        uint16_t current = impl_->indices[0];
        uint16_t count = 1;

        auto write_run = [&data](uint16_t val, uint16_t run_count) {
            // Write count (2 bytes) + value (2 bytes)
            data.push_back(static_cast<uint8_t>(run_count & 0xFF));
            data.push_back(static_cast<uint8_t>((run_count >> 8) & 0xFF));
            data.push_back(static_cast<uint8_t>(val & 0xFF));
            data.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        };

        for (size_t i = 1; i < impl_->indices.size(); ++i) {
            if (impl_->indices[i] == current && count < UINT16_MAX) {
                ++count;
            } else {
                write_run(current, count);
                current = impl_->indices[i];
                count = 1;
            }
        }
        write_run(current, count);
    }

    return data;
}

bool VoxelStorage::deserialize_rle(std::span<const uint8_t> data) {
    if (data.size() < 2) {
        return false;
    }

    size_t offset = 0;

    // Read palette size
    uint16_t palette_size_val = static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
    offset += 2;

    // Read palette entries
    if (data.size() < offset + palette_size_val * 4) {
        return false;
    }

    impl_->palette.clear();
    impl_->palette.reserve(palette_size_val);

    for (uint16_t i = 0; i < palette_size_val; ++i) {
        PaletteEntry entry;
        entry.block_id = static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
        offset += 2;
        entry.state_id = static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
        offset += 2;
        impl_->palette.push_back(entry);
    }

    // Read RLE-encoded indices
    impl_->indices.clear();
    impl_->indices.reserve(CHUNK_VOLUME);

    while (offset + 4 <= data.size() && impl_->indices.size() < CHUNK_VOLUME) {
        uint16_t count = static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
        offset += 2;
        uint16_t value = static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
        offset += 2;

        for (uint16_t i = 0; i < count && impl_->indices.size() < CHUNK_VOLUME; ++i) {
            impl_->indices.push_back(value);
        }
    }

    // Ensure we have the right number of voxels
    impl_->indices.resize(CHUNK_VOLUME, 0);

    return true;
}

std::vector<uint8_t> VoxelStorage::serialize_raw() const {
    std::vector<uint8_t> data;

    // Palette size
    uint16_t palette_size_val = static_cast<uint16_t>(impl_->palette.size());
    data.push_back(static_cast<uint8_t>(palette_size_val & 0xFF));
    data.push_back(static_cast<uint8_t>((palette_size_val >> 8) & 0xFF));

    // Palette entries
    for (const auto& entry : impl_->palette) {
        data.push_back(static_cast<uint8_t>(entry.block_id & 0xFF));
        data.push_back(static_cast<uint8_t>((entry.block_id >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>(entry.state_id & 0xFF));
        data.push_back(static_cast<uint8_t>((entry.state_id >> 8) & 0xFF));
    }

    // Raw indices (2 bytes each)
    for (uint16_t idx : impl_->indices) {
        data.push_back(static_cast<uint8_t>(idx & 0xFF));
        data.push_back(static_cast<uint8_t>((idx >> 8) & 0xFF));
    }

    return data;
}

bool VoxelStorage::deserialize_raw(std::span<const uint8_t> data) {
    if (data.size() < 2) {
        return false;
    }

    size_t offset = 0;

    // Read palette size
    uint16_t palette_size_val = static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
    offset += 2;

    // Read palette
    if (data.size() < offset + palette_size_val * 4 + CHUNK_VOLUME * 2) {
        return false;
    }

    impl_->palette.clear();
    impl_->palette.reserve(palette_size_val);

    for (uint16_t i = 0; i < palette_size_val; ++i) {
        PaletteEntry entry;
        entry.block_id = static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
        offset += 2;
        entry.state_id = static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
        offset += 2;
        impl_->palette.push_back(entry);
    }

    // Read indices
    impl_->indices.resize(CHUNK_VOLUME);
    for (size_t i = 0; i < CHUNK_VOLUME; ++i) {
        impl_->indices[i] = static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
        offset += 2;
    }

    return true;
}

size_t VoxelStorage::memory_usage() const {
    return sizeof(Impl) + impl_->palette.capacity() * sizeof(PaletteEntry) +
           impl_->indices.capacity() * sizeof(uint16_t);
}

uint8_t VoxelStorage::bits_per_voxel() const {
    size_t size = impl_->palette.size();
    if (size <= 16)
        return 4;
    if (size <= 256)
        return 8;
    return 16;
}

// ============================================================================
// ChunkSection Implementation
// ============================================================================

struct ChunkSection::Impl {
    std::vector<PaletteEntry> palette;
    std::vector<uint8_t> indices;  // Compact storage

    Impl() {
        palette.push_back(PaletteEntry::air());
        indices.resize(VOLUME, 0);
    }
};

ChunkSection::ChunkSection() : impl_(std::make_unique<Impl>()) {}

ChunkSection::~ChunkSection() = default;

ChunkSection::ChunkSection(ChunkSection&&) noexcept = default;
ChunkSection& ChunkSection::operator=(ChunkSection&&) noexcept = default;

PaletteEntry ChunkSection::get(int32_t x, int32_t y, int32_t z) const {
    if (x < 0 || x >= SIZE || y < 0 || y >= SIZE || z < 0 || z >= SIZE) {
        return PaletteEntry::air();
    }
    size_t index = static_cast<size_t>(y * SIZE * SIZE + z * SIZE + x);
    uint8_t palette_idx = impl_->indices[index];
    if (palette_idx >= impl_->palette.size()) {
        return PaletteEntry::air();
    }
    return impl_->palette[palette_idx];
}

void ChunkSection::set(int32_t x, int32_t y, int32_t z, const PaletteEntry& entry) {
    if (x < 0 || x >= SIZE || y < 0 || y >= SIZE || z < 0 || z >= SIZE) {
        return;
    }

    // Find or add to palette
    uint8_t palette_idx = 0;
    for (size_t i = 0; i < impl_->palette.size(); ++i) {
        if (impl_->palette[i] == entry) {
            palette_idx = static_cast<uint8_t>(i);
            goto found;
        }
    }
    if (impl_->palette.size() < 256) {
        palette_idx = static_cast<uint8_t>(impl_->palette.size());
        impl_->palette.push_back(entry);
    }

found:
    size_t index = static_cast<size_t>(y * SIZE * SIZE + z * SIZE + x);
    impl_->indices[index] = palette_idx;
}

bool ChunkSection::is_empty() const {
    if (impl_->palette.size() == 1 && impl_->palette[0].block_id == BLOCK_AIR) {
        return true;
    }
    for (uint8_t idx : impl_->indices) {
        if (idx < impl_->palette.size() && impl_->palette[idx].block_id != BLOCK_AIR) {
            return false;
        }
    }
    return true;
}

size_t ChunkSection::non_air_count() const {
    size_t count = 0;
    for (uint8_t idx : impl_->indices) {
        if (idx < impl_->palette.size() && impl_->palette[idx].block_id != BLOCK_AIR) {
            ++count;
        }
    }
    return count;
}

std::vector<uint8_t> ChunkSection::serialize() const {
    std::vector<uint8_t> data;

    // Palette size (1 byte, max 256)
    data.push_back(static_cast<uint8_t>(impl_->palette.size()));

    // Palette entries
    for (const auto& entry : impl_->palette) {
        data.push_back(static_cast<uint8_t>(entry.block_id & 0xFF));
        data.push_back(static_cast<uint8_t>((entry.block_id >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>(entry.state_id & 0xFF));
        data.push_back(static_cast<uint8_t>((entry.state_id >> 8) & 0xFF));
    }

    // Indices
    data.insert(data.end(), impl_->indices.begin(), impl_->indices.end());

    return data;
}

bool ChunkSection::deserialize(std::span<const uint8_t> data) {
    if (data.empty()) {
        return false;
    }

    size_t offset = 0;
    uint8_t palette_size_val = data[offset++];

    if (data.size() < offset + palette_size_val * 4 + VOLUME) {
        return false;
    }

    impl_->palette.clear();
    impl_->palette.reserve(palette_size_val);

    for (uint8_t i = 0; i < palette_size_val; ++i) {
        PaletteEntry entry;
        entry.block_id = static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
        offset += 2;
        entry.state_id = static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
        offset += 2;
        impl_->palette.push_back(entry);
    }

    impl_->indices.assign(data.begin() + static_cast<std::ptrdiff_t>(offset),
                          data.begin() + static_cast<std::ptrdiff_t>(offset + VOLUME));

    return true;
}

}  // namespace realcraft::world
