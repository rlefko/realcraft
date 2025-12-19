// RealCraft World System
// serialization.cpp - Binary chunk format and region file management

#include <cstring>
#include <fstream>
#include <list>
#include <mutex>
#include <realcraft/core/logger.hpp>
#include <realcraft/world/serialization.hpp>
#include <unordered_map>
#include <zlib.h>

namespace realcraft::world {

// ============================================================================
// Compression Helpers
// ============================================================================

namespace {

std::vector<uint8_t> compress_zlib(std::span<const uint8_t> data) {
    if (data.empty()) {
        return {};
    }

    // Estimate compressed size
    uLong compressed_size = compressBound(static_cast<uLong>(data.size()));
    std::vector<uint8_t> compressed(compressed_size);

    int result = compress2(compressed.data(), &compressed_size, data.data(), static_cast<uLong>(data.size()),
                           Z_DEFAULT_COMPRESSION);

    if (result != Z_OK) {
        REALCRAFT_LOG_ERROR(core::log_category::WORLD, "zlib compression failed: {}", result);
        return {};
    }

    compressed.resize(compressed_size);
    return compressed;
}

std::vector<uint8_t> decompress_zlib(std::span<const uint8_t> data, size_t uncompressed_size) {
    if (data.empty()) {
        return {};
    }

    std::vector<uint8_t> decompressed(uncompressed_size);
    uLong dest_size = static_cast<uLong>(uncompressed_size);

    int result = uncompress(decompressed.data(), &dest_size, data.data(), static_cast<uLong>(data.size()));

    if (result != Z_OK) {
        REALCRAFT_LOG_ERROR(core::log_category::WORLD, "zlib decompression failed: {}", result);
        return {};
    }

    decompressed.resize(dest_size);
    return decompressed;
}

}  // namespace

// ============================================================================
// ChunkSerializer Implementation
// ============================================================================

struct ChunkSerializer::Impl {
    CompressionType compression = CompressionType::Zlib;
};

ChunkSerializer::ChunkSerializer() : impl_(std::make_unique<Impl>()) {}

ChunkSerializer::~ChunkSerializer() = default;

void ChunkSerializer::set_compression(CompressionType type) {
    impl_->compression = type;
}

CompressionType ChunkSerializer::get_compression() const {
    return impl_->compression;
}

std::vector<uint8_t> ChunkSerializer::serialize(const Chunk& chunk) const {
    // Get raw chunk data
    std::vector<uint8_t> raw_data = chunk.serialize();

    // Prepare header
    ChunkFileHeader header;
    header.chunk_x = chunk.get_position().x;
    header.chunk_z = chunk.get_position().y;
    header.uncompressed_size = static_cast<uint32_t>(raw_data.size());
    header.compression_type = static_cast<uint8_t>(impl_->compression);

    // Compress if needed
    std::vector<uint8_t> payload;
    if (impl_->compression == CompressionType::Zlib) {
        payload = compress_zlib(raw_data);
        if (payload.empty() && !raw_data.empty()) {
            // Compression failed, fall back to uncompressed
            header.compression_type = 0;
            payload = std::move(raw_data);
        }
    } else {
        payload = std::move(raw_data);
    }

    header.compressed_size = static_cast<uint32_t>(payload.size());

    // Build final data: header + payload
    std::vector<uint8_t> result(sizeof(ChunkFileHeader) + payload.size());
    std::memcpy(result.data(), &header, sizeof(ChunkFileHeader));
    std::memcpy(result.data() + sizeof(ChunkFileHeader), payload.data(), payload.size());

    return result;
}

bool ChunkSerializer::deserialize(Chunk& chunk, std::span<const uint8_t> data) const {
    if (data.size() < sizeof(ChunkFileHeader)) {
        REALCRAFT_LOG_ERROR(core::log_category::WORLD, "Chunk data too small for header");
        return false;
    }

    // Read header
    ChunkFileHeader header;
    std::memcpy(&header, data.data(), sizeof(ChunkFileHeader));

    if (!validate_header(data)) {
        return false;
    }

    // Extract payload
    if (data.size() < sizeof(ChunkFileHeader) + header.compressed_size) {
        REALCRAFT_LOG_ERROR(core::log_category::WORLD, "Chunk data truncated");
        return false;
    }

    std::span<const uint8_t> payload(data.data() + sizeof(ChunkFileHeader), header.compressed_size);

    // Decompress if needed
    std::vector<uint8_t> raw_data;
    if (header.compression_type == static_cast<uint8_t>(CompressionType::Zlib)) {
        raw_data = decompress_zlib(payload, header.uncompressed_size);
        if (raw_data.empty() && header.uncompressed_size > 0) {
            REALCRAFT_LOG_ERROR(core::log_category::WORLD, "Failed to decompress chunk data");
            return false;
        }
    } else {
        raw_data.assign(payload.begin(), payload.end());
    }

    // Deserialize chunk
    return chunk.deserialize(raw_data);
}

bool ChunkSerializer::save_to_file(const Chunk& chunk, const std::filesystem::path& path) const {
    std::vector<uint8_t> data = serialize(chunk);
    if (data.empty()) {
        return false;
    }

    // Ensure parent directory exists
    std::filesystem::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        REALCRAFT_LOG_ERROR(core::log_category::WORLD, "Failed to open file for writing: {}", path.string());
        return false;
    }

    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return file.good();
}

bool ChunkSerializer::load_from_file(Chunk& chunk, const std::filesystem::path& path) const {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }

    auto size = file.tellg();
    file.seekg(0);

    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), size);

    if (!file) {
        REALCRAFT_LOG_ERROR(core::log_category::WORLD, "Failed to read file: {}", path.string());
        return false;
    }

    return deserialize(chunk, data);
}

bool ChunkSerializer::validate_header(std::span<const uint8_t> data) const {
    if (data.size() < sizeof(ChunkFileHeader)) {
        return false;
    }

    ChunkFileHeader header;
    std::memcpy(&header, data.data(), sizeof(ChunkFileHeader));

    if (header.magic != CHUNK_MAGIC) {
        REALCRAFT_LOG_ERROR(core::log_category::WORLD, "Invalid chunk magic: 0x{:08X}", header.magic);
        return false;
    }

    if (header.version > CHUNK_FORMAT_VERSION) {
        REALCRAFT_LOG_ERROR(core::log_category::WORLD, "Unsupported chunk version: {}", header.version);
        return false;
    }

    return true;
}

// ============================================================================
// RegionFile Implementation
// ============================================================================

struct RegionFile::Impl {
    std::filesystem::path path;
    std::fstream file;
    bool is_open = false;
    glm::ivec2 region_pos{0, 0};

    // Chunk offset table (32x32 = 1024 entries)
    std::array<RegionChunkEntry, TOTAL_CHUNKS> entries{};

    // Current end of file (for appending new chunks)
    size_t data_end = 0;
};

RegionFile::RegionFile(const std::filesystem::path& path) : impl_(std::make_unique<Impl>()) {
    impl_->path = path;

    // Parse region position from filename (r.X.Z.rcr)
    std::string filename = path.stem().string();
    if (filename.length() > 2 && filename[0] == 'r' && filename[1] == '.') {
        size_t pos1 = 2;
        size_t pos2 = filename.find('.', pos1);
        if (pos2 != std::string::npos) {
            impl_->region_pos.x = std::stoi(filename.substr(pos1, pos2 - pos1));
            impl_->region_pos.y = std::stoi(filename.substr(pos2 + 1));
        }
    }
}

RegionFile::~RegionFile() {
    close();
}

bool RegionFile::open(bool create_if_missing) {
    if (impl_->is_open) {
        return true;
    }

    bool exists = std::filesystem::exists(impl_->path);

    if (!exists && !create_if_missing) {
        return false;
    }

    // Create parent directory
    std::filesystem::create_directories(impl_->path.parent_path());

    if (exists) {
        impl_->file.open(impl_->path, std::ios::binary | std::ios::in | std::ios::out);
    } else {
        impl_->file.open(impl_->path, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
    }

    if (!impl_->file) {
        REALCRAFT_LOG_ERROR(core::log_category::WORLD, "Failed to open region file: {}", impl_->path.string());
        return false;
    }

    impl_->is_open = true;

    if (exists) {
        // Read header and entry table
        RegionFileHeader header;
        impl_->file.read(reinterpret_cast<char*>(&header), sizeof(header));

        if (header.magic != REGION_MAGIC) {
            REALCRAFT_LOG_ERROR(core::log_category::WORLD, "Invalid region file magic: {}", impl_->path.string());
            close();
            return false;
        }

        impl_->region_pos.x = header.region_x;
        impl_->region_pos.y = header.region_z;

        impl_->file.read(reinterpret_cast<char*>(impl_->entries.data()), sizeof(impl_->entries));

        // Calculate data end
        impl_->data_end = sizeof(RegionFileHeader) + sizeof(impl_->entries);
        for (const auto& entry : impl_->entries) {
            size_t entry_end = entry.offset + entry.size;
            if (entry_end > impl_->data_end) {
                impl_->data_end = entry_end;
            }
        }
    } else {
        // Write initial header and empty entry table
        RegionFileHeader header;
        header.region_x = impl_->region_pos.x;
        header.region_z = impl_->region_pos.y;
        header.chunk_count = 0;

        impl_->file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        impl_->file.write(reinterpret_cast<const char*>(impl_->entries.data()), sizeof(impl_->entries));

        impl_->data_end = sizeof(RegionFileHeader) + sizeof(impl_->entries);
        impl_->file.flush();
    }

    return true;
}

void RegionFile::close() {
    if (impl_->is_open) {
        flush();
        impl_->file.close();
        impl_->is_open = false;
    }
}

bool RegionFile::is_open() const {
    return impl_->is_open;
}

const std::filesystem::path& RegionFile::get_path() const {
    return impl_->path;
}

glm::ivec2 RegionFile::chunk_to_region(const ChunkPos& chunk) {
    return world::chunk_to_region(chunk);
}

glm::ivec2 RegionFile::chunk_local_in_region(const ChunkPos& chunk) {
    return world::chunk_local_in_region(chunk);
}

size_t RegionFile::chunk_to_index(const ChunkPos& chunk) {
    glm::ivec2 local = chunk_local_in_region(chunk);
    return static_cast<size_t>(local.y * CHUNKS_PER_SIDE + local.x);
}

bool RegionFile::has_chunk(const ChunkPos& chunk) const {
    if (!impl_->is_open) {
        return false;
    }
    size_t index = chunk_to_index(chunk);
    return impl_->entries[index].size > 0;
}

std::optional<std::vector<uint8_t>> RegionFile::read_chunk(const ChunkPos& chunk) const {
    if (!impl_->is_open) {
        return std::nullopt;
    }

    size_t index = chunk_to_index(chunk);
    const auto& entry = impl_->entries[index];

    if (entry.size == 0) {
        return std::nullopt;
    }

    std::vector<uint8_t> data(entry.size);
    impl_->file.seekg(static_cast<std::streamoff>(entry.offset));
    impl_->file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(entry.size));

    if (!impl_->file) {
        REALCRAFT_LOG_ERROR(core::log_category::WORLD, "Failed to read chunk from region file");
        return std::nullopt;
    }

    return data;
}

bool RegionFile::write_chunk(const ChunkPos& chunk, std::span<const uint8_t> data) {
    if (!impl_->is_open) {
        return false;
    }

    size_t index = chunk_to_index(chunk);
    auto& entry = impl_->entries[index];

    // For simplicity, always append (could optimize by reusing space)
    size_t offset = impl_->data_end;

    impl_->file.seekp(static_cast<std::streamoff>(offset));
    impl_->file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));

    if (!impl_->file) {
        REALCRAFT_LOG_ERROR(core::log_category::WORLD, "Failed to write chunk to region file");
        return false;
    }

    entry.offset = static_cast<uint32_t>(offset);
    entry.size = static_cast<uint32_t>(data.size());
    impl_->data_end = offset + data.size();

    // Update entry in file
    size_t entry_offset = sizeof(RegionFileHeader) + index * sizeof(RegionChunkEntry);
    impl_->file.seekp(static_cast<std::streamoff>(entry_offset));
    impl_->file.write(reinterpret_cast<const char*>(&entry), sizeof(entry));

    return impl_->file.good();
}

bool RegionFile::delete_chunk(const ChunkPos& chunk) {
    if (!impl_->is_open) {
        return false;
    }

    size_t index = chunk_to_index(chunk);
    auto& entry = impl_->entries[index];

    entry.offset = 0;
    entry.size = 0;

    // Update entry in file
    size_t entry_offset = sizeof(RegionFileHeader) + index * sizeof(RegionChunkEntry);
    impl_->file.seekp(static_cast<std::streamoff>(entry_offset));
    impl_->file.write(reinterpret_cast<const char*>(&entry), sizeof(entry));

    return impl_->file.good();
}

size_t RegionFile::chunk_count() const {
    size_t count = 0;
    for (const auto& entry : impl_->entries) {
        if (entry.size > 0) {
            ++count;
        }
    }
    return count;
}

size_t RegionFile::file_size() const {
    return impl_->data_end;
}

glm::ivec2 RegionFile::get_region_pos() const {
    return impl_->region_pos;
}

void RegionFile::flush() {
    if (impl_->is_open) {
        impl_->file.flush();
    }
}

// ============================================================================
// RegionManager Implementation
// ============================================================================

struct RegionManager::Impl {
    std::filesystem::path world_dir;
    size_t max_open_files = 8;

    // LRU cache of open region files
    std::list<std::pair<glm::ivec2, std::unique_ptr<RegionFile>>> lru_list;
    std::unordered_map<int64_t, decltype(lru_list)::iterator> cache;

    mutable std::mutex mutex;

    static int64_t region_key(const glm::ivec2& pos) {
        return (static_cast<int64_t>(pos.x) << 32) | static_cast<uint32_t>(pos.y);
    }

    RegionFile* get_or_open_region(const glm::ivec2& region_pos, bool create_if_missing) {
        int64_t key = region_key(region_pos);

        auto it = cache.find(key);
        if (it != cache.end()) {
            // Move to front of LRU
            lru_list.splice(lru_list.begin(), lru_list, it->second);
            return it->second->second.get();
        }

        // Evict if at capacity
        while (lru_list.size() >= max_open_files) {
            auto& back = lru_list.back();
            cache.erase(region_key(back.first));
            lru_list.pop_back();
        }

        // Open new region file
        std::filesystem::path path = world_dir / "regions" / fmt::format("r.{}.{}.rcr", region_pos.x, region_pos.y);
        auto region = std::make_unique<RegionFile>(path);

        if (!region->open(create_if_missing)) {
            return nullptr;
        }

        lru_list.emplace_front(region_pos, std::move(region));
        cache[key] = lru_list.begin();

        return lru_list.front().second.get();
    }
};

RegionManager::RegionManager(const std::filesystem::path& world_directory) : impl_(std::make_unique<Impl>()) {
    impl_->world_dir = world_directory;
}

RegionManager::~RegionManager() {
    close_all();
}

bool RegionManager::has_chunk(const ChunkPos& chunk) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    glm::ivec2 region_pos = chunk_to_region(chunk);
    RegionFile* region = impl_->get_or_open_region(region_pos, false);
    if (!region) {
        return false;
    }
    return region->has_chunk(chunk);
}

std::optional<std::vector<uint8_t>> RegionManager::read_chunk(const ChunkPos& chunk) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    glm::ivec2 region_pos = chunk_to_region(chunk);
    RegionFile* region = impl_->get_or_open_region(region_pos, false);
    if (!region) {
        return std::nullopt;
    }
    return region->read_chunk(chunk);
}

bool RegionManager::write_chunk(const ChunkPos& chunk, std::span<const uint8_t> data) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    glm::ivec2 region_pos = chunk_to_region(chunk);
    RegionFile* region = impl_->get_or_open_region(region_pos, true);
    if (!region) {
        return false;
    }
    return region->write_chunk(chunk, data);
}

bool RegionManager::delete_chunk(const ChunkPos& chunk) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    glm::ivec2 region_pos = chunk_to_region(chunk);
    RegionFile* region = impl_->get_or_open_region(region_pos, false);
    if (!region) {
        return false;
    }
    return region->delete_chunk(chunk);
}

void RegionManager::flush() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (auto& [pos, region] : impl_->lru_list) {
        region->flush();
    }
}

void RegionManager::close_all() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->lru_list.clear();
    impl_->cache.clear();
}

void RegionManager::set_max_open_files(size_t count) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->max_open_files = count;

    // Evict excess
    while (impl_->lru_list.size() > count) {
        auto& back = impl_->lru_list.back();
        impl_->cache.erase(Impl::region_key(back.first));
        impl_->lru_list.pop_back();
    }
}

size_t RegionManager::get_max_open_files() const {
    return impl_->max_open_files;
}

size_t RegionManager::open_region_count() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->lru_list.size();
}

size_t RegionManager::total_chunks_on_disk() const {
    // This would require scanning all region files - expensive
    // For now, just return count from open regions
    std::lock_guard<std::mutex> lock(impl_->mutex);
    size_t count = 0;
    for (const auto& [pos, region] : impl_->lru_list) {
        count += region->chunk_count();
    }
    return count;
}

std::filesystem::path RegionManager::get_region_path(const ChunkPos& chunk) const {
    glm::ivec2 region_pos = chunk_to_region(chunk);
    return impl_->world_dir / "regions" / fmt::format("r.{}.{}.rcr", region_pos.x, region_pos.y);
}

}  // namespace realcraft::world
