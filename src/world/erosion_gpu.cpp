// RealCraft World System
// erosion_gpu.cpp - GPU compute shader-based erosion simulation

#include <realcraft/core/logger.hpp>
#include <realcraft/graphics/buffer.hpp>
#include <realcraft/graphics/command_buffer.hpp>
#include <realcraft/graphics/device.hpp>
#include <realcraft/graphics/pipeline.hpp>
#include <realcraft/graphics/shader.hpp>
#include <realcraft/graphics/shader_compiler.hpp>
#include <realcraft/world/erosion.hpp>

namespace realcraft::world {

// Uniform buffer structure matching the compute shader
struct ErosionParams {
    int32_t map_width;
    int32_t map_height;
    float inertia;
    float sediment_capacity;
    float min_sediment_capacity;
    float erosion_rate;
    float deposition_rate;
    float gravity;
    float evaporation_rate;
    int32_t max_lifetime;
    int32_t erosion_radius;
    uint32_t seed;
    int32_t droplet_offset;
    int32_t padding[3];  // Pad to 16-byte alignment
};

// GLSL compute shader source for erosion
static const char* erosion_shader_source = R"(
#version 450

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(std430, set = 0, binding = 0) buffer HeightmapBuffer {
    float heightmap[];
};

layout(std430, set = 0, binding = 1) buffer SedimentBuffer {
    float sediment[];
};

layout(std140, set = 0, binding = 2) uniform ErosionParams {
    int map_width;
    int map_height;
    float inertia;
    float sediment_capacity;
    float min_sediment_capacity;
    float erosion_rate;
    float deposition_rate;
    float gravity;
    float evaporation_rate;
    int max_lifetime;
    int erosion_radius;
    uint seed;
    int droplet_offset;
};

uint pcg_hash(uint input) {
    uint state = input * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float random_float(inout uint rng_state) {
    rng_state = pcg_hash(rng_state);
    return float(rng_state) / 4294967295.0;
}

int get_index(int x, int z) {
    return z * map_width + x;
}

float sample_height(float x, float z) {
    x = clamp(x, 0.0, float(map_width - 2));
    z = clamp(z, 0.0, float(map_height - 2));
    int ix = int(x);
    int iz = int(z);
    float fx = x - float(ix);
    float fz = z - float(iz);
    float h00 = heightmap[get_index(ix, iz)];
    float h10 = heightmap[get_index(ix + 1, iz)];
    float h01 = heightmap[get_index(ix, iz + 1)];
    float h11 = heightmap[get_index(ix + 1, iz + 1)];
    float h0 = mix(h00, h10, fx);
    float h1 = mix(h01, h11, fx);
    return mix(h0, h1, fz);
}

vec2 calculate_gradient(float x, float z) {
    float h = sample_height(x, z);
    float hx = sample_height(x + 1.0, z);
    float hz = sample_height(x, z + 1.0);
    return vec2(hx - h, hz - h);
}

void main() {
    uint droplet_id = gl_GlobalInvocationID.x + uint(droplet_offset);
    uint rng_state = seed ^ droplet_id;

    float pos_x = random_float(rng_state) * float(map_width - 3) + 1.0;
    float pos_z = random_float(rng_state) * float(map_height - 3) + 1.0;
    float dir_x = 0.0;
    float dir_z = 0.0;
    float speed = 1.0;
    float water = 1.0;
    float sed = 0.0;

    for (int i = 0; i < max_lifetime; ++i) {
        int node_x = int(pos_x);
        int node_z = int(pos_z);
        float cell_offset_x = pos_x - float(node_x);
        float cell_offset_z = pos_z - float(node_z);

        float height = sample_height(pos_x, pos_z);
        vec2 gradient = calculate_gradient(pos_x, pos_z);

        dir_x = dir_x * inertia - gradient.x * (1.0 - inertia);
        dir_z = dir_z * inertia - gradient.y * (1.0 - inertia);

        float len = sqrt(dir_x * dir_x + dir_z * dir_z);
        if (len > 0.0001) {
            dir_x /= len;
            dir_z /= len;
        } else {
            float angle = random_float(rng_state) * 6.28318530718;
            dir_x = cos(angle);
            dir_z = sin(angle);
        }

        float new_pos_x = pos_x + dir_x;
        float new_pos_z = pos_z + dir_z;

        if (new_pos_x < 1.0 || new_pos_x >= float(map_width - 2) ||
            new_pos_z < 1.0 || new_pos_z >= float(map_height - 2)) {
            // Deposit remaining sediment at boundary instead of discarding
            if (sed > 0.0) {
                float deposit = sed * 0.5;  // Deposit half at boundary
                float w00 = (1.0 - cell_offset_x) * (1.0 - cell_offset_z);
                float w10 = cell_offset_x * (1.0 - cell_offset_z);
                float w01 = (1.0 - cell_offset_x) * cell_offset_z;
                float w11 = cell_offset_x * cell_offset_z;

                int idx00 = get_index(node_x, node_z);
                int idx10 = get_index(node_x + 1, node_z);
                int idx01 = get_index(node_x, node_z + 1);
                int idx11 = get_index(node_x + 1, node_z + 1);

                heightmap[idx00] += deposit * w00;
                heightmap[idx10] += deposit * w10;
                heightmap[idx01] += deposit * w01;
                heightmap[idx11] += deposit * w11;

                sediment[idx00] += deposit * w00;
                sediment[idx10] += deposit * w10;
                sediment[idx01] += deposit * w01;
                sediment[idx11] += deposit * w11;
            }
            break;
        }

        float new_height = sample_height(new_pos_x, new_pos_z);
        float delta_height = new_height - height;

        float capacity = max(-delta_height * speed * water * sediment_capacity, min_sediment_capacity);

        float w00 = (1.0 - cell_offset_x) * (1.0 - cell_offset_z);
        float w10 = cell_offset_x * (1.0 - cell_offset_z);
        float w01 = (1.0 - cell_offset_x) * cell_offset_z;
        float w11 = cell_offset_x * cell_offset_z;

        int idx00 = get_index(node_x, node_z);
        int idx10 = get_index(node_x + 1, node_z);
        int idx01 = get_index(node_x, node_z + 1);
        int idx11 = get_index(node_x + 1, node_z + 1);

        if (sed > capacity || delta_height > 0.0) {
            float amount;
            if (delta_height > 0.0) {
                amount = min(delta_height, sed);
            } else {
                amount = (sed - capacity) * deposition_rate;
            }
            sed -= amount;

            heightmap[idx00] += amount * w00;
            heightmap[idx10] += amount * w10;
            heightmap[idx01] += amount * w01;
            heightmap[idx11] += amount * w11;

            sediment[idx00] += amount * w00;
            sediment[idx10] += amount * w10;
            sediment[idx01] += amount * w01;
            sediment[idx11] += amount * w11;
        } else {
            float amount = min((capacity - sed) * erosion_rate, -delta_height);
            sed += amount;

            heightmap[idx00] -= amount * w00;
            heightmap[idx10] -= amount * w10;
            heightmap[idx01] -= amount * w01;
            heightmap[idx11] -= amount * w11;
        }

        speed = sqrt(max(0.0, speed * speed + delta_height * gravity));
        water *= (1.0 - evaporation_rate);
        pos_x = new_pos_x;
        pos_z = new_pos_z;

        if (water < 0.01) {
            break;
        }
    }
}
)";

struct GPUErosionEngine::Impl {
    graphics::GraphicsDevice* device = nullptr;
    std::unique_ptr<graphics::Shader> compute_shader;
    std::unique_ptr<graphics::ComputePipeline> pipeline;
    std::unique_ptr<graphics::Buffer> heightmap_buffer;
    std::unique_ptr<graphics::Buffer> sediment_buffer;
    std::unique_ptr<graphics::Buffer> params_buffer;
    bool initialized = false;

    bool initialize() {
        if (!device) {
            return false;
        }

        // Compile compute shader
        graphics::ShaderCompiler compiler;
        graphics::ShaderCompileOptions options;
        options.stage = graphics::ShaderStage::Compute;
        options.entry_point = "main";

        auto result = compiler.compile_glsl(erosion_shader_source, options);
        if (!result.success) {
            REALCRAFT_LOG_ERROR(core::log_category::WORLD, "Failed to compile erosion compute shader: {}",
                                result.error_message);
            return false;
        }

        // Create shader object
        graphics::ShaderDesc shader_desc;
        shader_desc.stage = graphics::ShaderStage::Compute;
        shader_desc.bytecode =
            result.msl_bytecode.empty()
                ? std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(result.msl_source.data()),
                                           result.msl_source.size())
                : std::span<const uint8_t>(result.msl_bytecode);
        shader_desc.entry_point = "main0";
        shader_desc.debug_name = "erosion_compute";

        compute_shader = device->create_shader(shader_desc);
        if (!compute_shader) {
            REALCRAFT_LOG_ERROR(core::log_category::WORLD, "Failed to create erosion compute shader");
            return false;
        }

        // Create compute pipeline
        graphics::ComputePipelineDesc pipeline_desc;
        pipeline_desc.compute_shader = compute_shader.get();
        pipeline_desc.debug_name = "erosion_pipeline";

        pipeline = device->create_compute_pipeline(pipeline_desc);
        if (!pipeline) {
            REALCRAFT_LOG_ERROR(core::log_category::WORLD, "Failed to create erosion compute pipeline");
            return false;
        }

        // Create uniform buffer for params
        graphics::BufferDesc params_desc;
        params_desc.size = sizeof(ErosionParams);
        params_desc.usage = graphics::BufferUsage::Uniform;
        params_desc.host_visible = true;
        params_desc.debug_name = "erosion_params";

        params_buffer = device->create_buffer(params_desc);
        if (!params_buffer) {
            REALCRAFT_LOG_ERROR(core::log_category::WORLD, "Failed to create erosion params buffer");
            return false;
        }

        initialized = true;
        REALCRAFT_LOG_INFO(core::log_category::WORLD, "GPU erosion engine initialized");
        return true;
    }

    bool create_buffers(size_t heightmap_size) {
        // Create heightmap storage buffer
        graphics::BufferDesc heightmap_desc;
        heightmap_desc.size = heightmap_size * sizeof(float);
        heightmap_desc.usage =
            graphics::BufferUsage::Storage | graphics::BufferUsage::TransferSrc | graphics::BufferUsage::TransferDst;
        heightmap_desc.host_visible = true;
        heightmap_desc.debug_name = "erosion_heightmap";

        heightmap_buffer = device->create_buffer(heightmap_desc);
        if (!heightmap_buffer) {
            return false;
        }

        // Create sediment storage buffer
        graphics::BufferDesc sediment_desc;
        sediment_desc.size = heightmap_size * sizeof(float);
        sediment_desc.usage = graphics::BufferUsage::Storage;
        sediment_desc.host_visible = true;
        sediment_desc.debug_name = "erosion_sediment";

        sediment_buffer = device->create_buffer(sediment_desc);
        if (!sediment_buffer) {
            return false;
        }

        return true;
    }
};

GPUErosionEngine::GPUErosionEngine(graphics::GraphicsDevice* device) : impl_(std::make_unique<Impl>()) {
    impl_->device = device;
    if (device) {
        impl_->initialize();
    }
}

GPUErosionEngine::~GPUErosionEngine() = default;

bool GPUErosionEngine::is_available() const {
    return impl_->initialized && impl_->device != nullptr;
}

void GPUErosionEngine::erode(ErosionHeightmap& heightmap, const ErosionConfig& config, uint32_t seed) {
    if (!is_available()) {
        REALCRAFT_LOG_WARN(core::log_category::WORLD, "GPU erosion not available, skipping");
        return;
    }

    const size_t heightmap_size =
        static_cast<size_t>(heightmap.total_width()) * static_cast<size_t>(heightmap.total_height());

    // Ensure buffers are large enough
    if (!impl_->heightmap_buffer || impl_->heightmap_buffer->get_size() < heightmap_size * sizeof(float)) {
        if (!impl_->create_buffers(heightmap_size)) {
            REALCRAFT_LOG_ERROR(core::log_category::WORLD, "Failed to create GPU erosion buffers");
            return;
        }
    }

    // Upload heightmap to GPU
    impl_->heightmap_buffer->write(heightmap.heights().data(), heightmap_size * sizeof(float));

    // Clear sediment buffer
    std::vector<float> zero_sediment(heightmap_size, 0.0f);
    impl_->sediment_buffer->write(zero_sediment.data(), heightmap_size * sizeof(float));

    // Set up params
    ErosionParams params;
    params.map_width = heightmap.total_width();
    params.map_height = heightmap.total_height();
    params.inertia = config.particle.inertia;
    params.sediment_capacity = config.particle.sediment_capacity;
    params.min_sediment_capacity = config.particle.min_sediment_capacity;
    params.erosion_rate = config.particle.erosion_rate;
    params.deposition_rate = config.particle.deposition_rate;
    params.gravity = config.particle.gravity;
    params.evaporation_rate = config.particle.evaporation_rate;
    params.max_lifetime = config.particle.max_lifetime;
    params.erosion_radius = config.particle.erosion_radius;
    params.seed = seed;
    params.droplet_offset = 0;

    const int32_t batch_size = config.gpu.batch_size;
    const int32_t total_droplets = config.particle.droplet_count;
    const int32_t num_batches = (total_droplets + batch_size - 1) / batch_size;

    for (int32_t batch = 0; batch < num_batches; ++batch) {
        params.droplet_offset = batch * batch_size;
        impl_->params_buffer->write(&params, sizeof(params));

        // Create and execute command buffer
        auto cmd = impl_->device->create_command_buffer();
        cmd->begin();
        cmd->begin_compute_pass();
        cmd->bind_compute_pipeline(impl_->pipeline.get());
        cmd->bind_storage_buffer(0, impl_->heightmap_buffer.get());
        cmd->bind_storage_buffer(1, impl_->sediment_buffer.get());
        cmd->bind_uniform_buffer(2, impl_->params_buffer.get());

        // Dispatch with 64 threads per group
        const uint32_t num_groups = (batch_size + 63) / 64;
        cmd->dispatch(num_groups, 1, 1);

        cmd->end_compute_pass();
        cmd->end();

        impl_->device->submit(cmd.get(), true);  // Wait for completion between batches
    }

    // Download results back to CPU
    impl_->heightmap_buffer->read(heightmap.heights().data(), heightmap_size * sizeof(float));
    impl_->sediment_buffer->read(heightmap.sediment().data(), heightmap_size * sizeof(float));
}

}  // namespace realcraft::world
