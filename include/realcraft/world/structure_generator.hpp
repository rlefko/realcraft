// RealCraft World System
// structure_generator.hpp - Natural formation generation (rock spires, boulders)

#pragma once

#include "biome.hpp"
#include "chunk.hpp"
#include "types.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace realcraft::world {

// ============================================================================
// Structure Types
// ============================================================================

enum class StructureType : uint8_t {
    RockSpire = 0,  // Tall stone pillar
    Boulder,        // Large rock formation
    StoneOutcrop,   // Rocky outcrop
    Count
};

// ============================================================================
// Structure Configuration
// ============================================================================

struct StructureConfig {
    uint32_t seed = 0;
    bool enabled = true;

    struct RockSpireConfig {
        bool enabled = true;
        float chance_per_chunk = 0.03f;
        int32_t min_height = 5;
        int32_t max_height = 15;
        int32_t min_radius = 1;
        int32_t max_radius = 3;
    } rock_spire;

    struct BoulderConfig {
        bool enabled = true;
        float chance_per_chunk = 0.05f;
        int32_t min_size = 2;
        int32_t max_size = 5;
    } boulder;

    struct OutcropConfig {
        bool enabled = true;
        float chance_per_chunk = 0.04f;
        int32_t min_size = 3;
        int32_t max_size = 8;
    } outcrop;
};

// ============================================================================
// Structure Generator
// ============================================================================

/// Represents a single structure block to place
struct StructureBlock {
    LocalBlockPos offset;  // Offset from structure base
    BlockId block;
};

class StructureGenerator {
public:
    explicit StructureGenerator(const StructureConfig& config = {});
    ~StructureGenerator();

    // Non-copyable but movable
    StructureGenerator(const StructureGenerator&) = delete;
    StructureGenerator& operator=(const StructureGenerator&) = delete;
    StructureGenerator(StructureGenerator&&) noexcept;
    StructureGenerator& operator=(StructureGenerator&&) noexcept;

    // ========================================================================
    // Query Interface
    // ========================================================================

    /// Check if a structure should spawn in this chunk
    [[nodiscard]] bool should_place_structure(const ChunkPos& chunk_pos, StructureType type, BiomeType biome) const;

    /// Get structure position within chunk (if any)
    [[nodiscard]] std::optional<LocalBlockPos> get_structure_position(const ChunkPos& chunk_pos,
                                                                      StructureType type) const;

    /// Generate structure blocks for a position
    [[nodiscard]] std::vector<StructureBlock> generate_structure(const ChunkPos& chunk_pos, StructureType type,
                                                                 int32_t surface_height) const;

    // ========================================================================
    // Configuration
    // ========================================================================

    [[nodiscard]] const StructureConfig& get_config() const;
    void set_config(const StructureConfig& config);
    void rebuild();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::world
