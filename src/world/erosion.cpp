// RealCraft World System
// erosion.cpp - ErosionSimulator orchestrator implementation

#include <realcraft/core/logger.hpp>
#include <realcraft/world/erosion.hpp>

namespace realcraft::world {

ErosionSimulator::ErosionSimulator(graphics::GraphicsDevice* device) {
    // Always create CPU engine as fallback
    cpu_engine_ = std::make_unique<CPUErosionEngine>();

    // Create GPU engine if device is available
    if (device) {
        gpu_engine_ = std::make_unique<GPUErosionEngine>(device);
    }
}

ErosionSimulator::~ErosionSimulator() = default;

void ErosionSimulator::simulate(ErosionHeightmap& heightmap, const ErosionConfig& config, uint32_t seed) {
    if (!config.enabled) {
        return;
    }

    // Select engine based on preference and availability
    IErosionEngine* engine = nullptr;

    if (config.prefer_gpu && gpu_engine_ && gpu_engine_->is_available()) {
        engine = gpu_engine_.get();
    } else {
        engine = cpu_engine_.get();
    }

    REALCRAFT_LOG_DEBUG(core::log_category::WORLD, "Running erosion simulation using {} engine", engine->get_name());

    // Run erosion simulation
    engine->erode(heightmap, config, seed);

    // Run flow accumulation and river carving
    if (config.river.enabled) {
        heightmap.compute_flow_accumulation();
        river_carver_.carve(heightmap, config.river);
    }
}

bool ErosionSimulator::has_gpu_support() const {
    return gpu_engine_ && gpu_engine_->is_available();
}

}  // namespace realcraft::world
