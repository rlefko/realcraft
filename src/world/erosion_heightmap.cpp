// RealCraft World System
// erosion_heightmap.cpp - Heightmap data structure for erosion simulation

#include <algorithm>
#include <cmath>
#include <numeric>
#include <realcraft/world/erosion_heightmap.hpp>
#include <realcraft/world/terrain_generator.hpp>

namespace realcraft::world {

ErosionHeightmap::ErosionHeightmap(int32_t chunk_width, int32_t chunk_height, int32_t border)
    : chunk_width_(chunk_width), chunk_height_(chunk_height), border_(border), total_width_(chunk_width + 2 * border),
      total_height_(chunk_height + 2 * border) {
    const size_t total_size = static_cast<size_t>(total_width_) * static_cast<size_t>(total_height_);
    heights_.resize(total_size, 0.0f);
    sediment_.resize(total_size, 0.0f);
    flow_.resize(total_size, 1.0f);  // Each cell starts with flow=1
}

void ErosionHeightmap::populate_from_generator(const TerrainGenerator& generator, const ChunkPos& chunk_pos) {
    const int64_t base_x = static_cast<int64_t>(chunk_pos.x) * CHUNK_SIZE_X - border_;
    const int64_t base_z = static_cast<int64_t>(chunk_pos.y) * CHUNK_SIZE_Z - border_;

    for (int32_t z = 0; z < total_height_; ++z) {
        for (int32_t x = 0; x < total_width_; ++x) {
            const int64_t world_x = base_x + x;
            const int64_t world_z = base_z + z;
            heights_[index(x, z)] = static_cast<float>(generator.get_height(world_x, world_z));
        }
    }
}

void ErosionHeightmap::apply_to_heights(std::array<int32_t, CHUNK_SIZE_X * CHUNK_SIZE_Z>& heights) const {
    for (int32_t z = 0; z < chunk_height_; ++z) {
        for (int32_t x = 0; x < chunk_width_; ++x) {
            // Map from core area (with border offset) to output array
            const float h = heights_[index(x + border_, z + border_)];
            heights[static_cast<size_t>(z * chunk_width_ + x)] = static_cast<int32_t>(std::round(h));
        }
    }
}

float ErosionHeightmap::get(int32_t x, int32_t z) const {
    if (!is_valid(x, z)) {
        return 0.0f;
    }
    return heights_[index(x, z)];
}

void ErosionHeightmap::set(int32_t x, int32_t z, float height) {
    if (is_valid(x, z)) {
        heights_[index(x, z)] = height;
    }
}

float ErosionHeightmap::sample_bilinear(float x, float z) const {
    // Clamp to valid range
    x = std::clamp(x, 0.0f, static_cast<float>(total_width_ - 2));
    z = std::clamp(z, 0.0f, static_cast<float>(total_height_ - 2));

    const int32_t ix = static_cast<int32_t>(x);
    const int32_t iz = static_cast<int32_t>(z);
    const float fx = x - static_cast<float>(ix);
    const float fz = z - static_cast<float>(iz);

    // Get four corner heights
    const float h00 = get(ix, iz);
    const float h10 = get(ix + 1, iz);
    const float h01 = get(ix, iz + 1);
    const float h11 = get(ix + 1, iz + 1);

    // Bilinear interpolation
    const float h0 = h00 * (1.0f - fx) + h10 * fx;
    const float h1 = h01 * (1.0f - fx) + h11 * fx;
    return h0 * (1.0f - fz) + h1 * fz;
}

glm::vec3 ErosionHeightmap::calculate_gradient(float x, float z) const {
    // Use central differences for gradient
    const float epsilon = 1.0f;
    const float height = sample_bilinear(x, z);
    const float hx_plus = sample_bilinear(x + epsilon, z);
    const float hx_minus = sample_bilinear(x - epsilon, z);
    const float hz_plus = sample_bilinear(x, z + epsilon);
    const float hz_minus = sample_bilinear(x, z - epsilon);

    // Gradient points in direction of steepest ascent
    // For erosion we want steepest descent, so negate when using
    const float gx = (hx_plus - hx_minus) / (2.0f * epsilon);
    const float gz = (hz_plus - hz_minus) / (2.0f * epsilon);

    return glm::vec3(gx, height, gz);
}

void ErosionHeightmap::add_height(int32_t x, int32_t z, float delta) {
    if (is_valid(x, z)) {
        heights_[index(x, z)] += delta;
    }
}

float ErosionHeightmap::get_sediment(int32_t x, int32_t z) const {
    if (!is_valid(x, z)) {
        return 0.0f;
    }
    return sediment_[index(x, z)];
}

void ErosionHeightmap::add_sediment(int32_t x, int32_t z, float amount) {
    if (is_valid(x, z)) {
        sediment_[index(x, z)] += amount;
    }
}

void ErosionHeightmap::deposit_sediment_bilinear(float x, float z, float amount) {
    // Clamp to valid range
    x = std::clamp(x, 0.0f, static_cast<float>(total_width_ - 2));
    z = std::clamp(z, 0.0f, static_cast<float>(total_height_ - 2));

    const int32_t ix = static_cast<int32_t>(x);
    const int32_t iz = static_cast<int32_t>(z);
    const float fx = x - static_cast<float>(ix);
    const float fz = z - static_cast<float>(iz);

    // Distribute sediment to four corners based on bilinear weights
    const float w00 = (1.0f - fx) * (1.0f - fz);
    const float w10 = fx * (1.0f - fz);
    const float w01 = (1.0f - fx) * fz;
    const float w11 = fx * fz;

    add_sediment(ix, iz, amount * w00);
    add_sediment(ix + 1, iz, amount * w10);
    add_sediment(ix, iz + 1, amount * w01);
    add_sediment(ix + 1, iz + 1, amount * w11);
}

float ErosionHeightmap::get_flow(int32_t x, int32_t z) const {
    if (!is_valid(x, z)) {
        return 0.0f;
    }
    return flow_[index(x, z)];
}

void ErosionHeightmap::set_flow(int32_t x, int32_t z, float flow) {
    if (is_valid(x, z)) {
        flow_[index(x, z)] = flow;
    }
}

void ErosionHeightmap::compute_flow_accumulation() {
    // D8 flow direction algorithm
    // Direction offsets: N, NE, E, SE, S, SW, W, NW
    constexpr int32_t dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    constexpr int32_t dz[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    // Distance weights (1.0 for cardinal, ~1.414 for diagonal)
    constexpr float dist[8] = {1.0f, 1.414f, 1.0f, 1.414f, 1.0f, 1.414f, 1.0f, 1.414f};

    const size_t total_size = static_cast<size_t>(total_width_) * static_cast<size_t>(total_height_);

    // Step 1: Calculate flow direction for each cell
    std::vector<int8_t> flow_dir(total_size, -1);

    for (int32_t z = 1; z < total_height_ - 1; ++z) {
        for (int32_t x = 1; x < total_width_ - 1; ++x) {
            const float h = get(x, z);
            float max_slope = 0.0f;
            int8_t best_dir = -1;

            for (int d = 0; d < 8; ++d) {
                const int32_t nx = x + dx[d];
                const int32_t nz = z + dz[d];
                const float nh = get(nx, nz);
                const float slope = (h - nh) / dist[d];

                if (slope > max_slope) {
                    max_slope = slope;
                    best_dir = static_cast<int8_t>(d);
                }
            }

            flow_dir[index(x, z)] = best_dir;
        }
    }

    // Step 2: Create sorted list of cells by height (descending)
    std::vector<std::pair<float, size_t>> sorted_cells;
    sorted_cells.reserve(total_size);
    for (size_t i = 0; i < total_size; ++i) {
        sorted_cells.emplace_back(heights_[i], i);
    }
    std::sort(sorted_cells.begin(), sorted_cells.end(), [](const auto& a, const auto& b) { return a.first > b.first; });

    // Step 3: Initialize flow (each cell contributes 1)
    std::fill(flow_.begin(), flow_.end(), 1.0f);

    // Step 4: Accumulate flow from high to low
    for (const auto& [height, idx] : sorted_cells) {
        const int32_t x = static_cast<int32_t>(idx % static_cast<size_t>(total_width_));
        const int32_t z = static_cast<int32_t>(idx / static_cast<size_t>(total_width_));
        const int8_t dir = flow_dir[idx];

        if (dir >= 0) {
            const int32_t nx = x + dx[dir];
            const int32_t nz = z + dz[dir];
            if (is_valid(nx, nz)) {
                flow_[index(nx, nz)] += flow_[idx];
            }
        }
    }
}

bool ErosionHeightmap::is_valid(int32_t x, int32_t z) const {
    return x >= 0 && x < total_width_ && z >= 0 && z < total_height_;
}

bool ErosionHeightmap::is_in_core(int32_t x, int32_t z) const {
    return x >= border_ && x < border_ + chunk_width_ && z >= border_ && z < border_ + chunk_height_;
}

size_t ErosionHeightmap::index(int32_t x, int32_t z) const {
    return static_cast<size_t>(z) * static_cast<size_t>(total_width_) + static_cast<size_t>(x);
}

}  // namespace realcraft::world
