// RealCraft World System
// types.hpp - Core types, coordinates, enums, and conversion functions

#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <functional>

namespace realcraft::world {

// ============================================================================
// Block Identification
// ============================================================================

// Block type ID - references a registered BlockType
using BlockId = uint16_t;

// Block state for variants (orientation, waterlogged, etc.)
// Encoded as bit fields: [orientation:3][variant:5][flags:8]
using BlockStateId = uint16_t;

// Special block IDs
inline constexpr BlockId BLOCK_AIR = 0;
inline constexpr BlockId BLOCK_STONE = 1;  // Default solid block (registered as ID 1)
inline constexpr BlockId BLOCK_INVALID = 0xFFFF;

// ============================================================================
// Chunk Constants
// ============================================================================

inline constexpr int32_t CHUNK_SIZE_X = 32;
inline constexpr int32_t CHUNK_SIZE_Y = 256;
inline constexpr int32_t CHUNK_SIZE_Z = 32;
inline constexpr int32_t CHUNK_VOLUME = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;

// Sub-chunks for LOD and partial updates
inline constexpr int32_t SUBCHUNK_SIZE = 16;
inline constexpr int32_t SUBCHUNKS_PER_CHUNK = CHUNK_SIZE_Y / SUBCHUNK_SIZE;

// Region file dimensions (32x32 chunks per region)
inline constexpr int32_t REGION_SIZE = 32;
inline constexpr int32_t CHUNKS_PER_REGION = REGION_SIZE * REGION_SIZE;

// ============================================================================
// Coordinate Types (using GLM)
// ============================================================================

// Local block position within a chunk [0, CHUNK_SIZE)
using LocalBlockPos = glm::ivec3;

// Chunk coordinate (chunk grid position, x and z only)
using ChunkPos = glm::ivec2;

// Absolute block position (64-bit for infinite worlds)
using WorldBlockPos = glm::i64vec3;

// Precise world position (double precision for physics/rendering)
using WorldPos = glm::dvec3;

// ============================================================================
// Coordinate Conversion Functions
// ============================================================================

// Convert world block position to chunk position
[[nodiscard]] inline ChunkPos world_to_chunk(const WorldBlockPos& pos) {
    // Use arithmetic right shift for correct floor division with negatives
    return ChunkPos(
        static_cast<int32_t>(pos.x >= 0 ? pos.x / CHUNK_SIZE_X : (pos.x - CHUNK_SIZE_X + 1) / CHUNK_SIZE_X),
        static_cast<int32_t>(pos.z >= 0 ? pos.z / CHUNK_SIZE_Z : (pos.z - CHUNK_SIZE_Z + 1) / CHUNK_SIZE_Z));
}

// Convert world block position to local position within chunk
[[nodiscard]] inline LocalBlockPos world_to_local(const WorldBlockPos& pos) {
    // Use modulo that always returns positive result
    auto mod = [](int64_t a, int32_t b) -> int32_t {
        int64_t result = a % b;
        return static_cast<int32_t>(result >= 0 ? result : result + b);
    };
    return LocalBlockPos(mod(pos.x, CHUNK_SIZE_X), static_cast<int32_t>(pos.y), mod(pos.z, CHUNK_SIZE_Z));
}

// Convert chunk position and local position to world block position
[[nodiscard]] inline WorldBlockPos local_to_world(const ChunkPos& chunk, const LocalBlockPos& local) {
    return WorldBlockPos(static_cast<int64_t>(chunk.x) * CHUNK_SIZE_X + local.x, static_cast<int64_t>(local.y),
                         static_cast<int64_t>(chunk.y) * CHUNK_SIZE_Z + local.z);
}

// Convert local position to array index (Y-major for better cache locality)
[[nodiscard]] inline size_t local_to_index(const LocalBlockPos& pos) {
    return static_cast<size_t>(pos.y * CHUNK_SIZE_X * CHUNK_SIZE_Z + pos.z * CHUNK_SIZE_X + pos.x);
}

// Convert array index to local position
[[nodiscard]] inline LocalBlockPos index_to_local(size_t index) {
    const int32_t x = static_cast<int32_t>(index % CHUNK_SIZE_X);
    const int32_t z = static_cast<int32_t>((index / CHUNK_SIZE_X) % CHUNK_SIZE_Z);
    const int32_t y = static_cast<int32_t>(index / (CHUNK_SIZE_X * CHUNK_SIZE_Z));
    return LocalBlockPos(x, y, z);
}

// Check if local position is within chunk bounds
[[nodiscard]] inline bool is_valid_local(const LocalBlockPos& pos) {
    return pos.x >= 0 && pos.x < CHUNK_SIZE_X && pos.y >= 0 && pos.y < CHUNK_SIZE_Y && pos.z >= 0 &&
           pos.z < CHUNK_SIZE_Z;
}

// Convert world position (double) to world block position (integer, floor)
[[nodiscard]] inline WorldBlockPos world_to_block(const WorldPos& pos) {
    return WorldBlockPos(static_cast<int64_t>(std::floor(pos.x)), static_cast<int64_t>(std::floor(pos.y)),
                         static_cast<int64_t>(std::floor(pos.z)));
}

// Convert chunk position to region position
[[nodiscard]] inline glm::ivec2 chunk_to_region(const ChunkPos& chunk) {
    return glm::ivec2(chunk.x >= 0 ? chunk.x / REGION_SIZE : (chunk.x - REGION_SIZE + 1) / REGION_SIZE,
                      chunk.y >= 0 ? chunk.y / REGION_SIZE : (chunk.y - REGION_SIZE + 1) / REGION_SIZE);
}

// Get chunk's local position within its region
[[nodiscard]] inline glm::ivec2 chunk_local_in_region(const ChunkPos& chunk) {
    auto mod = [](int32_t a, int32_t b) -> int32_t {
        int32_t result = a % b;
        return result >= 0 ? result : result + b;
    };
    return glm::ivec2(mod(chunk.x, REGION_SIZE), mod(chunk.y, REGION_SIZE));
}

// ============================================================================
// Chunk State Machine
// ============================================================================

enum class ChunkState : uint8_t {
    Unloaded,    // Not in memory
    Loading,     // Being loaded from disk
    Generating,  // Being procedurally generated
    Loaded,      // Ready for use
    Modified,    // Has unsaved changes
    Saving,      // Being written to disk
    Unloading    // Being removed from memory
};

// Get string representation of chunk state
[[nodiscard]] inline const char* chunk_state_to_string(ChunkState state) {
    switch (state) {
        case ChunkState::Unloaded:
            return "Unloaded";
        case ChunkState::Loading:
            return "Loading";
        case ChunkState::Generating:
            return "Generating";
        case ChunkState::Loaded:
            return "Loaded";
        case ChunkState::Modified:
            return "Modified";
        case ChunkState::Saving:
            return "Saving";
        case ChunkState::Unloading:
            return "Unloading";
        default:
            return "Unknown";
    }
}

// ============================================================================
// Block Property Flags
// ============================================================================

enum class BlockFlags : uint32_t {
    None = 0,
    Solid = 1 << 0,          // Blocks movement
    Transparent = 1 << 1,    // Light passes through
    Liquid = 1 << 2,         // Water, lava
    Emissive = 1 << 3,       // Emits light
    Gravity = 1 << 4,        // Falls like sand
    Flammable = 1 << 5,      // Can catch fire
    Replaceable = 1 << 6,    // Can be replaced by placement (grass, flowers)
    FullCube = 1 << 7,       // Occupies entire block space
    HasCollision = 1 << 8,   // Has collision mesh
    Breakable = 1 << 9,      // Can be broken by player
    HasBlockState = 1 << 10  // Has variant states
};

inline BlockFlags operator|(BlockFlags a, BlockFlags b) {
    return static_cast<BlockFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline BlockFlags operator&(BlockFlags a, BlockFlags b) {
    return static_cast<BlockFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline BlockFlags& operator|=(BlockFlags& a, BlockFlags b) {
    a = a | b;
    return a;
}

inline BlockFlags& operator&=(BlockFlags& a, BlockFlags b) {
    a = a & b;
    return a;
}

[[nodiscard]] inline bool has_flag(BlockFlags flags, BlockFlags flag) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

// ============================================================================
// Neighbor Directions
// ============================================================================

enum class Direction : uint8_t {
    NegX = 0,  // West
    PosX = 1,  // East
    NegY = 2,  // Down
    PosY = 3,  // Up
    NegZ = 4,  // North
    PosZ = 5,  // South
    Count = 6
};

// Horizontal directions only (for chunk neighbors)
enum class HorizontalDirection : uint8_t {
    NegX = 0,  // West
    PosX = 1,  // East
    NegZ = 2,  // North
    PosZ = 3,  // South
    Count = 4
};

// Direction offset vectors
inline constexpr glm::ivec3 DIRECTION_OFFSETS[6] = {{-1, 0, 0}, {1, 0, 0},  {0, -1, 0},
                                                    {0, 1, 0},  {0, 0, -1}, {0, 0, 1}};

// Horizontal direction offset vectors
inline constexpr glm::ivec2 HORIZONTAL_DIRECTION_OFFSETS[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

[[nodiscard]] inline Direction opposite(Direction dir) {
    return static_cast<Direction>(static_cast<uint8_t>(dir) ^ 1);
}

[[nodiscard]] inline HorizontalDirection opposite(HorizontalDirection dir) {
    return static_cast<HorizontalDirection>(static_cast<uint8_t>(dir) ^ 1);
}

[[nodiscard]] inline glm::ivec3 direction_offset(Direction dir) {
    return DIRECTION_OFFSETS[static_cast<uint8_t>(dir)];
}

[[nodiscard]] inline glm::ivec2 direction_offset(HorizontalDirection dir) {
    return HORIZONTAL_DIRECTION_OFFSETS[static_cast<uint8_t>(dir)];
}

// Get direction name
[[nodiscard]] inline const char* direction_to_string(Direction dir) {
    switch (dir) {
        case Direction::NegX:
            return "West";
        case Direction::PosX:
            return "East";
        case Direction::NegY:
            return "Down";
        case Direction::PosY:
            return "Up";
        case Direction::NegZ:
            return "North";
        case Direction::PosZ:
            return "South";
        default:
            return "Unknown";
    }
}

// ============================================================================
// Water State Encoding (for BlockStateId of water blocks)
// ============================================================================
//
// BlockStateId bit layout for water (16 bits total):
// Bits 0-3:   Water level (0-15, where 15 = full, 0 = empty/almost dry)
// Bits 4-6:   Flow direction (0=none, 1=+X, 2=-X, 3=+Z, 4=-Z, 5=down, 6=up)
// Bit 7:      Is source block (1 = infinite source, 0 = flowing)
// Bits 8-11:  Pressure level (0-15, derived from depth)
// Bits 12-15: Reserved for future use

inline constexpr uint16_t WATER_LEVEL_MASK = 0x000F;     // bits 0-3
inline constexpr uint16_t WATER_FLOW_DIR_MASK = 0x0070;  // bits 4-6
inline constexpr uint16_t WATER_SOURCE_MASK = 0x0080;    // bit 7
inline constexpr uint16_t WATER_PRESSURE_MASK = 0x0F00;  // bits 8-11

inline constexpr uint8_t WATER_FULL_LEVEL = 15;
inline constexpr uint8_t WATER_MIN_FLOW_LEVEL = 1;

// Flow direction values (stored in bits 4-6)
enum class WaterFlowDirection : uint8_t {
    None = 0,
    PosX = 1,  // East
    NegX = 2,  // West
    PosZ = 3,  // South
    NegZ = 4,  // North
    NegY = 5,  // Down (gravity)
    PosY = 6   // Up (pressure-driven)
};

// Get water level (0-15)
[[nodiscard]] inline uint8_t water_get_level(BlockStateId state) {
    return static_cast<uint8_t>(state & WATER_LEVEL_MASK);
}

// Set water level (0-15)
inline void water_set_level(BlockStateId& state, uint8_t level) {
    state = static_cast<BlockStateId>((state & ~WATER_LEVEL_MASK) | (level & 0x0F));
}

// Get flow direction
[[nodiscard]] inline WaterFlowDirection water_get_flow_direction(BlockStateId state) {
    return static_cast<WaterFlowDirection>((state & WATER_FLOW_DIR_MASK) >> 4);
}

// Set flow direction
inline void water_set_flow_direction(BlockStateId& state, WaterFlowDirection dir) {
    state = static_cast<BlockStateId>((state & ~WATER_FLOW_DIR_MASK) | ((static_cast<uint8_t>(dir) & 0x07) << 4));
}

// Check if water is a source block (infinite)
[[nodiscard]] inline bool water_is_source(BlockStateId state) {
    return (state & WATER_SOURCE_MASK) != 0;
}

// Set source block flag
inline void water_set_source(BlockStateId& state, bool is_source) {
    if (is_source) {
        state |= WATER_SOURCE_MASK;
    } else {
        state = static_cast<BlockStateId>(state & ~WATER_SOURCE_MASK);
    }
}

// Get pressure level (0-15)
[[nodiscard]] inline uint8_t water_get_pressure(BlockStateId state) {
    return static_cast<uint8_t>((state & WATER_PRESSURE_MASK) >> 8);
}

// Set pressure level (0-15)
inline void water_set_pressure(BlockStateId& state, uint8_t pressure) {
    state = static_cast<BlockStateId>((state & ~WATER_PRESSURE_MASK) | ((pressure & 0x0F) << 8));
}

// Create a complete water state
[[nodiscard]] inline BlockStateId water_make_state(uint8_t level, bool is_source,
                                                   WaterFlowDirection flow = WaterFlowDirection::None,
                                                   uint8_t pressure = 0) {
    BlockStateId state = 0;
    water_set_level(state, level);
    water_set_source(state, is_source);
    water_set_flow_direction(state, flow);
    water_set_pressure(state, pressure);
    return state;
}

// Convert flow direction to 3D vector
[[nodiscard]] inline glm::ivec3 water_flow_to_offset(WaterFlowDirection dir) {
    switch (dir) {
        case WaterFlowDirection::PosX:
            return {1, 0, 0};
        case WaterFlowDirection::NegX:
            return {-1, 0, 0};
        case WaterFlowDirection::PosZ:
            return {0, 0, 1};
        case WaterFlowDirection::NegZ:
            return {0, 0, -1};
        case WaterFlowDirection::NegY:
            return {0, -1, 0};
        case WaterFlowDirection::PosY:
            return {0, 1, 0};
        default:
            return {0, 0, 0};
    }
}

}  // namespace realcraft::world

// ============================================================================
// Hash Functions for using coordinates as map keys
// ============================================================================

namespace std {

template <>
struct hash<realcraft::world::ChunkPos> {
    size_t operator()(const realcraft::world::ChunkPos& pos) const noexcept {
        // Combine x and y using a good hash mixing function
        size_t h1 = std::hash<int32_t>{}(pos.x);
        size_t h2 = std::hash<int32_t>{}(pos.y);
        return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};

template <>
struct hash<realcraft::world::WorldBlockPos> {
    size_t operator()(const realcraft::world::WorldBlockPos& pos) const noexcept {
        size_t h1 = std::hash<int64_t>{}(pos.x);
        size_t h2 = std::hash<int64_t>{}(pos.y);
        size_t h3 = std::hash<int64_t>{}(pos.z);
        size_t result = h1;
        result ^= h2 * 0x9e3779b97f4a7c15ULL + (result << 6) + (result >> 2);
        result ^= h3 * 0x9e3779b97f4a7c15ULL + (result << 6) + (result >> 2);
        return result;
    }
};

}  // namespace std
