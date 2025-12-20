// RealCraft World System
// erosion_cpu.cpp - CPU-based particle erosion simulation

#include <algorithm>
#include <cmath>
#include <mutex>
#include <random>
#include <realcraft/world/erosion.hpp>
#include <thread>
#include <vector>

namespace realcraft::world {

struct CPUErosionEngine::Impl {
    /// Simulate a single water droplet eroding the terrain
    void simulate_droplet(ErosionHeightmap& heightmap, const ErosionConfig::ParticleParams& params, std::mt19937& rng,
                          std::mutex& heightmap_mutex) {
        const float total_width = static_cast<float>(heightmap.total_width());
        const float total_height = static_cast<float>(heightmap.total_height());

        // Initialize droplet at random position within valid bounds
        std::uniform_real_distribution<float> dist_x(1.0f, total_width - 2.0f);
        std::uniform_real_distribution<float> dist_z(1.0f, total_height - 2.0f);

        float pos_x = dist_x(rng);
        float pos_z = dist_z(rng);
        float dir_x = 0.0f;
        float dir_z = 0.0f;
        float speed = 1.0f;
        float water = 1.0f;
        float sediment = 0.0f;

        for (int i = 0; i < params.max_lifetime; ++i) {
            const int32_t node_x = static_cast<int32_t>(pos_x);
            const int32_t node_z = static_cast<int32_t>(pos_z);

            // Calculate droplet offset within cell
            const float cell_offset_x = pos_x - static_cast<float>(node_x);
            const float cell_offset_z = pos_z - static_cast<float>(node_z);

            // Calculate height and gradient
            float height;
            glm::vec3 gradient;
            {
                std::lock_guard<std::mutex> lock(heightmap_mutex);
                height = heightmap.sample_bilinear(pos_x, pos_z);
                gradient = heightmap.calculate_gradient(pos_x, pos_z);
            }

            // Update direction with inertia (gradient points uphill, so negate for downhill)
            dir_x = dir_x * params.inertia - gradient.x * (1.0f - params.inertia);
            dir_z = dir_z * params.inertia - gradient.z * (1.0f - params.inertia);

            // Normalize direction
            const float len = std::sqrt(dir_x * dir_x + dir_z * dir_z);
            if (len > 0.0001f) {
                dir_x /= len;
                dir_z /= len;
            } else {
                // Random direction if on flat terrain
                std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * 3.14159265359f);
                const float angle = angle_dist(rng);
                dir_x = std::cos(angle);
                dir_z = std::sin(angle);
            }

            // Calculate new position
            const float new_pos_x = pos_x + dir_x;
            const float new_pos_z = pos_z + dir_z;

            // Check bounds (leave 1 cell margin)
            if (new_pos_x < 1.0f || new_pos_x >= total_width - 2.0f || new_pos_z < 1.0f ||
                new_pos_z >= total_height - 2.0f) {
                // Deposit remaining sediment at boundary instead of discarding
                if (sediment > 0.0f) {
                    const float deposit = sediment * 0.5f;  // Deposit half at boundary
                    const float w00 = (1.0f - cell_offset_x) * (1.0f - cell_offset_z);
                    const float w10 = cell_offset_x * (1.0f - cell_offset_z);
                    const float w01 = (1.0f - cell_offset_x) * cell_offset_z;
                    const float w11 = cell_offset_x * cell_offset_z;

                    std::lock_guard<std::mutex> lock(heightmap_mutex);
                    heightmap.add_height(node_x, node_z, deposit * w00);
                    heightmap.add_height(node_x + 1, node_z, deposit * w10);
                    heightmap.add_height(node_x, node_z + 1, deposit * w01);
                    heightmap.add_height(node_x + 1, node_z + 1, deposit * w11);
                    heightmap.deposit_sediment_bilinear(pos_x, pos_z, deposit);
                }
                break;
            }

            float new_height;
            {
                std::lock_guard<std::mutex> lock(heightmap_mutex);
                new_height = heightmap.sample_bilinear(new_pos_x, new_pos_z);
            }
            const float delta_height = new_height - height;

            // Calculate sediment capacity based on slope, speed, and water volume
            const float capacity =
                std::max(-delta_height * speed * water * params.sediment_capacity, params.min_sediment_capacity);

            // Deposit or erode
            if (sediment > capacity || delta_height > 0) {
                // Deposit sediment (either carrying too much or going uphill)
                float deposit_amount;
                if (delta_height > 0) {
                    // Going uphill - deposit at most the height difference
                    deposit_amount = std::min(delta_height, sediment);
                } else {
                    // Over capacity - deposit excess
                    deposit_amount = (sediment - capacity) * params.deposition_rate;
                }

                sediment -= deposit_amount;

                // Deposit using bilinear weights to four corners
                const float w00 = (1.0f - cell_offset_x) * (1.0f - cell_offset_z);
                const float w10 = cell_offset_x * (1.0f - cell_offset_z);
                const float w01 = (1.0f - cell_offset_x) * cell_offset_z;
                const float w11 = cell_offset_x * cell_offset_z;

                {
                    std::lock_guard<std::mutex> lock(heightmap_mutex);
                    heightmap.add_height(node_x, node_z, deposit_amount * w00);
                    heightmap.add_height(node_x + 1, node_z, deposit_amount * w10);
                    heightmap.add_height(node_x, node_z + 1, deposit_amount * w01);
                    heightmap.add_height(node_x + 1, node_z + 1, deposit_amount * w11);
                    heightmap.deposit_sediment_bilinear(pos_x, pos_z, deposit_amount);
                }
            } else {
                // Erode terrain
                float erode_amount = std::min((capacity - sediment) * params.erosion_rate, -delta_height);

                sediment += erode_amount;

                // Erode in a radius around the droplet position
                if (params.erosion_radius > 0) {
                    erode_with_radius(heightmap, node_x, node_z, cell_offset_x, cell_offset_z, erode_amount,
                                      params.erosion_radius, heightmap_mutex);
                } else {
                    // Simple bilinear erosion
                    const float w00 = (1.0f - cell_offset_x) * (1.0f - cell_offset_z);
                    const float w10 = cell_offset_x * (1.0f - cell_offset_z);
                    const float w01 = (1.0f - cell_offset_x) * cell_offset_z;
                    const float w11 = cell_offset_x * cell_offset_z;

                    std::lock_guard<std::mutex> lock(heightmap_mutex);
                    heightmap.add_height(node_x, node_z, -erode_amount * w00);
                    heightmap.add_height(node_x + 1, node_z, -erode_amount * w10);
                    heightmap.add_height(node_x, node_z + 1, -erode_amount * w01);
                    heightmap.add_height(node_x + 1, node_z + 1, -erode_amount * w11);
                }
            }

            // Update droplet physics
            speed = std::sqrt(std::max(0.0f, speed * speed + delta_height * params.gravity));
            water *= (1.0f - params.evaporation_rate);
            pos_x = new_pos_x;
            pos_z = new_pos_z;

            // Stop if water evaporated
            if (water < 0.01f) {
                break;
            }
        }
    }

    /// Erode terrain in a radius around a point
    void erode_with_radius(ErosionHeightmap& heightmap, int32_t center_x, int32_t center_z, float offset_x,
                           float offset_z, float amount, int32_t radius, std::mutex& heightmap_mutex) {
        // Calculate actual center position
        const float cx = static_cast<float>(center_x) + offset_x;
        const float cz = static_cast<float>(center_z) + offset_z;

        // Calculate weights for all cells in radius
        float total_weight = 0.0f;
        std::vector<std::tuple<int32_t, int32_t, float>> affected_cells;

        for (int32_t dz = -radius; dz <= radius; ++dz) {
            for (int32_t dx = -radius; dx <= radius; ++dx) {
                const int32_t x = center_x + dx;
                const int32_t z = center_z + dz;

                if (!heightmap.is_valid(x, z)) {
                    continue;
                }

                const float dist_x = static_cast<float>(x) - cx;
                const float dist_z = static_cast<float>(z) - cz;
                const float dist_sq = dist_x * dist_x + dist_z * dist_z;
                const float radius_sq = static_cast<float>(radius * radius);

                if (dist_sq < radius_sq) {
                    // Weight falls off with distance
                    const float weight = std::max(0.0f, 1.0f - std::sqrt(dist_sq) / static_cast<float>(radius));
                    total_weight += weight;
                    affected_cells.emplace_back(x, z, weight);
                }
            }
        }

        // Apply erosion
        if (total_weight > 0.0f) {
            std::lock_guard<std::mutex> lock(heightmap_mutex);
            for (const auto& [x, z, weight] : affected_cells) {
                const float erode = amount * weight / total_weight;
                heightmap.add_height(x, z, -erode);
            }
        }
    }
};

CPUErosionEngine::CPUErosionEngine() : impl_(std::make_unique<Impl>()) {}

CPUErosionEngine::~CPUErosionEngine() = default;

void CPUErosionEngine::erode(ErosionHeightmap& heightmap, const ErosionConfig& config, uint32_t seed) {
    const int32_t droplet_count = config.particle.droplet_count;
    const unsigned int num_threads = std::max(1u, std::thread::hardware_concurrency());
    const int32_t droplets_per_thread = droplet_count / static_cast<int32_t>(num_threads);

    std::mutex heightmap_mutex;
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (unsigned int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, &heightmap, &config, &heightmap_mutex, seed, t, droplets_per_thread]() {
            // Each thread gets a unique seed
            std::mt19937 rng(seed + t * 12345);

            for (int32_t i = 0; i < droplets_per_thread; ++i) {
                impl_->simulate_droplet(heightmap, config.particle, rng, heightmap_mutex);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

}  // namespace realcraft::world
