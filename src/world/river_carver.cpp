// RealCraft World System
// river_carver.cpp - River channel carving based on flow accumulation

#include <algorithm>
#include <cmath>
#include <realcraft/world/erosion.hpp>
#include <vector>

namespace realcraft::world {

void RiverCarver::carve(ErosionHeightmap& heightmap, const ErosionConfig::RiverParams& config) {
    const int32_t width = heightmap.total_width();
    const int32_t height = heightmap.total_height();

    // Direction offsets: N, NE, E, SE, S, SW, W, NW
    constexpr int32_t dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    constexpr int32_t dz[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

    // Find cells with high flow accumulation (river cells)
    std::vector<bool> is_river(static_cast<size_t>(width * height), false);

    for (int32_t z = 1; z < height - 1; ++z) {
        for (int32_t x = 1; x < width - 1; ++x) {
            const float flow = heightmap.get_flow(x, z);
            if (flow >= config.min_flow_accumulation) {
                is_river[static_cast<size_t>(z * width + x)] = true;
            }
        }
    }

    // Carve channels for river cells
    for (int32_t z = 1; z < height - 1; ++z) {
        for (int32_t x = 1; x < width - 1; ++x) {
            const size_t idx = static_cast<size_t>(z * width + x);
            if (!is_river[idx]) {
                continue;
            }

            const float flow = heightmap.get_flow(x, z);

            // Calculate channel dimensions based on flow
            const float channel_depth = std::sqrt(flow) * config.channel_depth_factor;
            const float channel_width = std::sqrt(flow) * config.channel_width_factor;

            // Clamp to reasonable values
            const float max_depth = 8.0f;
            const float max_width = 6.0f;
            const float depth = std::min(channel_depth, max_depth);
            const float half_width = std::min(channel_width * 0.5f, max_width);

            // Carve the channel with smooth falloff
            const int32_t radius = static_cast<int32_t>(std::ceil(half_width)) + 1;

            for (int32_t dz_off = -radius; dz_off <= radius; ++dz_off) {
                for (int32_t dx_off = -radius; dx_off <= radius; ++dx_off) {
                    const int32_t nx = x + dx_off;
                    const int32_t nz = z + dz_off;

                    if (!heightmap.is_valid(nx, nz)) {
                        continue;
                    }

                    const float dist = std::sqrt(static_cast<float>(dx_off * dx_off + dz_off * dz_off));

                    if (dist <= half_width) {
                        // Smooth falloff from center
                        const float t = dist / half_width;
                        const float falloff = 1.0f - t * t;  // Quadratic falloff
                        const float carve_amount = depth * falloff;

                        const float current_h = heightmap.get(nx, nz);
                        heightmap.set(nx, nz, current_h - carve_amount);
                    }
                }
            }
        }
    }

    // Apply smoothing passes to reduce sharp edges
    for (int32_t pass = 0; pass < config.smoothing_passes; ++pass) {
        std::vector<float> smoothed(heightmap.heights());

        for (int32_t z = 1; z < height - 1; ++z) {
            for (int32_t x = 1; x < width - 1; ++x) {
                const size_t idx = static_cast<size_t>(z * width + x);

                // Only smooth river cells
                if (!is_river[idx]) {
                    continue;
                }

                // Average with neighbors
                float sum = 0.0f;
                int count = 0;

                for (int d = 0; d < 8; ++d) {
                    const int32_t nx = x + dx[d];
                    const int32_t nz = z + dz[d];
                    sum += heightmap.get(nx, nz);
                    count++;
                }

                const float center = heightmap.get(x, z);
                const float avg = sum / static_cast<float>(count);

                // Blend center with average (favor lower values for river beds)
                smoothed[idx] = std::min(center, (center + avg) * 0.5f);
            }
        }

        // Copy smoothed values back
        for (int32_t z = 1; z < height - 1; ++z) {
            for (int32_t x = 1; x < width - 1; ++x) {
                const size_t idx = static_cast<size_t>(z * width + x);
                if (is_river[idx]) {
                    heightmap.set(x, z, smoothed[idx]);
                }
            }
        }
    }
}

}  // namespace realcraft::world
