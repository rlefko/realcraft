// RealCraft World System
// serialization.hpp - Binary chunk format and region file management

#pragma once

#include "chunk.hpp"
#include "types.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace realcraft::world {

// ============================================================================
// Binary Format Version
// ============================================================================

inline constexpr uint32_t CHUNK_FORMAT_VERSION = 1;
inline constexpr uint32_t REGION_FORMAT_VERSION = 1;

// Magic bytes for file identification
inline constexpr uint32_t CHUNK_MAGIC = 0x52435643;   // "RCVC" - RealCraft Voxel Chunk
inline constexpr uint32_t REGION_MAGIC = 0x52435652;  // "RCVR" - RealCraft Voxel Region

// ============================================================================
// Chunk Header (binary layout)
// ============================================================================

#pragma pack(push, 1)
struct ChunkFileHeader {
    uint32_t magic = CHUNK_MAGIC;
    uint32_t version = CHUNK_FORMAT_VERSION;
    int32_t chunk_x = 0;
    int32_t chunk_z = 0;
    uint32_t compressed_size = 0;    // Size after compression
    uint32_t uncompressed_size = 0;  // Original data size
    uint8_t compression_type = 1;    // 0=none, 1=zstd
    uint8_t reserved[7] = {};
};
static_assert(sizeof(ChunkFileHeader) == 32, "ChunkFileHeader must be 32 bytes");

struct RegionFileHeader {
    uint32_t magic = REGION_MAGIC;
    uint32_t version = REGION_FORMAT_VERSION;
    int32_t region_x = 0;
    int32_t region_z = 0;
    uint32_t chunk_count = 0;  // Number of chunks in this region
    uint8_t reserved[12] = {};
};
static_assert(sizeof(RegionFileHeader) == 32, "RegionFileHeader must be 32 bytes");

struct RegionChunkEntry {
    uint32_t offset = 0;  // Offset from start of chunk data section
    uint32_t size = 0;    // Compressed size (0 = not present)
};
static_assert(sizeof(RegionChunkEntry) == 8, "RegionChunkEntry must be 8 bytes");
#pragma pack(pop)

// ============================================================================
// Compression Type
// ============================================================================

enum class CompressionType : uint8_t {
    None = 0,
    Zstd = 1,
    // Legacy: Zlib = 2 (for backwards compatibility if needed)
};

// ============================================================================
// Chunk Serializer
// ============================================================================

class ChunkSerializer {
public:
    ChunkSerializer();
    ~ChunkSerializer();

    // Non-copyable
    ChunkSerializer(const ChunkSerializer&) = delete;
    ChunkSerializer& operator=(const ChunkSerializer&) = delete;

    // Configuration
    void set_compression(CompressionType type);
    [[nodiscard]] CompressionType get_compression() const;

    // Serialize chunk to binary blob
    [[nodiscard]] std::vector<uint8_t> serialize(const Chunk& chunk) const;

    // Deserialize binary blob to chunk
    bool deserialize(Chunk& chunk, std::span<const uint8_t> data) const;

    // File I/O
    bool save_to_file(const Chunk& chunk, const std::filesystem::path& path) const;
    bool load_from_file(Chunk& chunk, const std::filesystem::path& path) const;

    // Validate chunk data without full deserialize
    [[nodiscard]] bool validate_header(std::span<const uint8_t> data) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Region File (32x32 chunks per file)
// ============================================================================

class RegionFile {
public:
    static constexpr int32_t CHUNKS_PER_SIDE = REGION_SIZE;
    static constexpr int32_t TOTAL_CHUNKS = CHUNKS_PER_REGION;

    explicit RegionFile(const std::filesystem::path& path);
    ~RegionFile();

    // Non-copyable
    RegionFile(const RegionFile&) = delete;
    RegionFile& operator=(const RegionFile&) = delete;

    // Open/close
    bool open(bool create_if_missing = true);
    void close();
    [[nodiscard]] bool is_open() const;
    [[nodiscard]] const std::filesystem::path& get_path() const;

    // Region position calculations
    [[nodiscard]] static glm::ivec2 chunk_to_region(const ChunkPos& chunk);
    [[nodiscard]] static glm::ivec2 chunk_local_in_region(const ChunkPos& chunk);
    [[nodiscard]] static size_t chunk_to_index(const ChunkPos& chunk);

    // Chunk operations
    [[nodiscard]] bool has_chunk(const ChunkPos& chunk) const;
    [[nodiscard]] std::optional<std::vector<uint8_t>> read_chunk(const ChunkPos& chunk) const;
    bool write_chunk(const ChunkPos& chunk, std::span<const uint8_t> data);
    bool delete_chunk(const ChunkPos& chunk);

    // Statistics
    [[nodiscard]] size_t chunk_count() const;
    [[nodiscard]] size_t file_size() const;
    [[nodiscard]] glm::ivec2 get_region_pos() const;

    // Flush changes to disk
    void flush();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Region Manager (caches open region files)
// ============================================================================

class RegionManager {
public:
    explicit RegionManager(const std::filesystem::path& world_directory);
    ~RegionManager();

    // Non-copyable
    RegionManager(const RegionManager&) = delete;
    RegionManager& operator=(const RegionManager&) = delete;

    // Chunk operations (handles region file selection)
    [[nodiscard]] bool has_chunk(const ChunkPos& chunk);
    [[nodiscard]] std::optional<std::vector<uint8_t>> read_chunk(const ChunkPos& chunk);
    bool write_chunk(const ChunkPos& chunk, std::span<const uint8_t> data);
    bool delete_chunk(const ChunkPos& chunk);

    // Flush all open region files
    void flush();

    // Close all region files
    void close_all();

    // Set max open region files (LRU eviction)
    void set_max_open_files(size_t count);
    [[nodiscard]] size_t get_max_open_files() const;

    // Statistics
    [[nodiscard]] size_t open_region_count() const;
    [[nodiscard]] size_t total_chunks_on_disk() const;

    // Get region file path for a chunk
    [[nodiscard]] std::filesystem::path get_region_path(const ChunkPos& chunk) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::world
