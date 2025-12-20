// RealCraft World System
// erosion.hpp - Hydraulic erosion simulation system

#pragma once

#include "erosion_heightmap.hpp"
#include "types.hpp"

#include <cstdint>
#include <memory>

namespace realcraft::graphics {
class GraphicsDevice;
}

namespace realcraft::world {

// ============================================================================
// Erosion Configuration
// ============================================================================

/// Configuration for hydraulic erosion simulation
struct ErosionConfig {
    /// Enable/disable erosion simulation (disabled by default for backward compatibility)
    bool enabled = false;

    /// Prefer GPU compute for erosion (fallback to CPU if unavailable)
    bool prefer_gpu = true;

    /// Border size around chunk for cross-chunk context (in blocks)
    int32_t border_size = 16;

    /// Particle simulation parameters
    struct ParticleParams {
        /// Number of water droplets to simulate per chunk region
        int32_t droplet_count = 50000;

        /// How much velocity is preserved between steps (0 = pure gradient descent, 1 = pure momentum)
        float inertia = 0.05f;

        /// Maximum sediment a droplet can carry (relative to speed and water volume)
        float sediment_capacity = 4.0f;

        /// Minimum sediment capacity (prevents division issues)
        float min_sediment_capacity = 0.01f;

        /// How quickly terrain erodes (0-1)
        float erosion_rate = 0.3f;

        /// How quickly sediment deposits (0-1)
        float deposition_rate = 0.3f;

        /// Gravity acceleration for speed calculation
        float gravity = 4.0f;

        /// Water evaporation rate per step (0-1)
        float evaporation_rate = 0.01f;

        /// Maximum lifetime of each droplet (steps)
        int32_t max_lifetime = 30;

        /// Radius around droplet for erosion/deposition effects
        int32_t erosion_radius = 3;
    } particle;

    /// GPU-specific parameters
    struct GPUParams {
        /// Droplets per compute dispatch batch
        int32_t batch_size = 1024;

        /// Number of dispatch iterations per erosion pass
        int32_t iterations = 50;
    } gpu;

    /// River carving parameters
    struct RiverParams {
        /// Enable river channel carving
        bool enabled = true;

        /// Minimum flow accumulation to form a river
        float min_flow_accumulation = 100.0f;

        /// Channel depth factor (depth = sqrt(flow) * factor)
        float channel_depth_factor = 0.3f;

        /// Channel width factor (width = sqrt(flow) * factor)
        float channel_width_factor = 0.2f;

        /// Number of smoothing passes for river paths
        int32_t smoothing_passes = 2;
    } river;

    /// Sediment deposition thresholds
    struct SedimentParams {
        /// Minimum sediment accumulation for silt blocks
        float silt_threshold = 0.5f;

        /// Minimum sediment for alluvium blocks
        float alluvium_threshold = 1.5f;

        /// Minimum sediment for river gravel
        float gravel_threshold = 3.0f;
    } sediment;
};

// ============================================================================
// Erosion Engine Interface
// ============================================================================

/// Abstract interface for erosion engines (CPU/GPU implementations)
class IErosionEngine {
public:
    virtual ~IErosionEngine() = default;

    /// Run erosion simulation on heightmap
    /// @param heightmap The heightmap to erode (modified in place)
    /// @param config Erosion parameters
    /// @param seed Random seed for deterministic results
    virtual void erode(ErosionHeightmap& heightmap, const ErosionConfig& config, uint32_t seed) = 0;

    /// Check if this engine is available for use
    [[nodiscard]] virtual bool is_available() const = 0;

    /// Get engine name for debugging/logging
    [[nodiscard]] virtual const char* get_name() const = 0;
};

// ============================================================================
// River Carver
// ============================================================================

/// Carves river channels based on flow accumulation
class RiverCarver {
public:
    RiverCarver() = default;

    /// Carve river channels into heightmap
    /// @param heightmap The heightmap to carve (modified in place)
    /// @param config River carving parameters
    void carve(ErosionHeightmap& heightmap, const ErosionConfig::RiverParams& config);
};

// ============================================================================
// CPU Erosion Engine
// ============================================================================

/// CPU-based particle erosion using multi-threading
class CPUErosionEngine : public IErosionEngine {
public:
    CPUErosionEngine();
    ~CPUErosionEngine() override;

    void erode(ErosionHeightmap& heightmap, const ErosionConfig& config, uint32_t seed) override;

    [[nodiscard]] bool is_available() const override { return true; }
    [[nodiscard]] const char* get_name() const override { return "CPU"; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// GPU Erosion Engine
// ============================================================================

/// GPU compute shader-based erosion
class GPUErosionEngine : public IErosionEngine {
public:
    explicit GPUErosionEngine(graphics::GraphicsDevice* device);
    ~GPUErosionEngine() override;

    void erode(ErosionHeightmap& heightmap, const ErosionConfig& config, uint32_t seed) override;

    [[nodiscard]] bool is_available() const override;
    [[nodiscard]] const char* get_name() const override { return "GPU"; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Erosion Simulator (Orchestrator)
// ============================================================================

/// Main erosion simulator that orchestrates CPU/GPU engines and river carving
class ErosionSimulator {
public:
    /// Create simulator with optional GPU device
    /// @param device Graphics device for GPU erosion (optional, CPU-only if nullptr)
    explicit ErosionSimulator(graphics::GraphicsDevice* device = nullptr);
    ~ErosionSimulator();

    /// Run full erosion simulation on heightmap
    /// @param heightmap The heightmap to erode
    /// @param config Erosion configuration
    /// @param seed Random seed for deterministic results
    void simulate(ErosionHeightmap& heightmap, const ErosionConfig& config, uint32_t seed);

    /// Check if GPU erosion is available
    [[nodiscard]] bool has_gpu_support() const;

private:
    std::unique_ptr<CPUErosionEngine> cpu_engine_;
    std::unique_ptr<GPUErosionEngine> gpu_engine_;
    RiverCarver river_carver_;
};

}  // namespace realcraft::world
