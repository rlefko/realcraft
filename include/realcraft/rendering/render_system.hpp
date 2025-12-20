// RealCraft Rendering System
// render_system.hpp - Main rendering orchestrator

#pragma once

#include "camera.hpp"
#include "chunk_mesh.hpp"
#include "frustum.hpp"
#include "lighting.hpp"
#include "mesh_manager.hpp"
#include "texture_manager.hpp"

#include <memory>
#include <realcraft/core/engine.hpp>
#include <realcraft/graphics/command_buffer.hpp>
#include <realcraft/graphics/device.hpp>
#include <realcraft/graphics/pipeline.hpp>
#include <realcraft/graphics/shader.hpp>
#include <realcraft/world/world_manager.hpp>

namespace realcraft::rendering {

// Render system configuration
struct RenderSystemConfig {
    CameraConfig camera;
    MeshManagerConfig mesh_manager;
    TextureManagerConfig texture_manager;
    float render_distance = 8.0f;  // In chunks
    bool enable_wireframe = false;
};

// Rendering statistics
struct RenderStats {
    uint32_t chunks_rendered = 0;
    uint32_t chunks_culled = 0;
    uint32_t triangles_rendered = 0;
    uint32_t draw_calls = 0;
    double frame_time_ms = 0.0;
};

// GPU uniform buffers
struct CameraUniforms {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 projection;
    alignas(16) glm::mat4 view_projection;
    alignas(16) glm::vec3 camera_position;
    float time;
};

// Main render system - orchestrates all rendering
class RenderSystem : public world::IWorldObserver {
public:
    RenderSystem();
    ~RenderSystem() override;

    // Non-copyable
    RenderSystem(const RenderSystem&) = delete;
    RenderSystem& operator=(const RenderSystem&) = delete;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    bool initialize(core::Engine* engine, world::WorldManager* world, const RenderSystemConfig& config = {});
    void shutdown();
    [[nodiscard]] bool is_initialized() const { return engine_ != nullptr; }

    // ========================================================================
    // Update Functions
    // ========================================================================

    // Called at fixed timestep (for camera interpolation)
    void fixed_update(double dt);

    // Called every frame
    void update(double dt);

    // Render callback (registered with engine)
    void render(double interpolation);

    // ========================================================================
    // Camera Access
    // ========================================================================

    [[nodiscard]] Camera& get_camera() { return camera_; }
    [[nodiscard]] const Camera& get_camera() const { return camera_; }

    // ========================================================================
    // Lighting Access
    // ========================================================================

    [[nodiscard]] DayNightCycle& get_day_night_cycle() { return day_night_cycle_; }
    [[nodiscard]] const DayNightCycle& get_day_night_cycle() const { return day_night_cycle_; }

    // ========================================================================
    // Configuration
    // ========================================================================

    void set_render_distance(float chunks);
    [[nodiscard]] float get_render_distance() const { return config_.render_distance; }

    void set_wireframe(bool enabled);
    [[nodiscard]] bool is_wireframe() const { return config_.enable_wireframe; }

    // ========================================================================
    // Statistics
    // ========================================================================

    [[nodiscard]] const RenderStats& get_stats() const { return stats_; }

    // ========================================================================
    // IWorldObserver Implementation
    // ========================================================================

    void on_chunk_loaded(const world::ChunkPos& pos, world::Chunk& chunk) override;
    void on_chunk_unloading(const world::ChunkPos& pos, const world::Chunk& chunk) override;
    void on_block_changed(const world::BlockChangeEvent& event) override;
    void on_origin_shifted(const world::WorldBlockPos& old_origin, const world::WorldBlockPos& new_origin) override;

private:
    core::Engine* engine_ = nullptr;
    world::WorldManager* world_ = nullptr;
    graphics::GraphicsDevice* device_ = nullptr;

    RenderSystemConfig config_;
    RenderStats stats_;

    // Subsystems
    Camera camera_;
    ChunkCuller culler_;
    DayNightCycle day_night_cycle_;
    std::unique_ptr<MeshManager> mesh_manager_;
    std::unique_ptr<TextureManager> texture_manager_;

    // GPU Resources
    std::unique_ptr<graphics::Shader> voxel_vertex_shader_;
    std::unique_ptr<graphics::Shader> voxel_fragment_shader_;
    std::unique_ptr<graphics::Pipeline> voxel_pipeline_;
    std::unique_ptr<graphics::Pipeline> voxel_wireframe_pipeline_;

    std::unique_ptr<graphics::Shader> sky_vertex_shader_;
    std::unique_ptr<graphics::Shader> sky_fragment_shader_;
    std::unique_ptr<graphics::Pipeline> sky_pipeline_;

    std::unique_ptr<graphics::Buffer> camera_uniform_buffer_;
    std::unique_ptr<graphics::Buffer> lighting_uniform_buffer_;

    std::unique_ptr<graphics::Texture> depth_texture_;

    // Render state
    world::WorldBlockPos origin_offset_{0, 0, 0};
    float total_time_ = 0.0f;

    // Initialization helpers
    bool create_shaders();
    bool create_pipelines();
    bool create_uniform_buffers();
    bool create_depth_texture(uint32_t width, uint32_t height);

    // Render passes
    void update_uniform_buffers(double interpolation);
    void render_chunks(graphics::CommandBuffer* cmd);
    void render_sky(graphics::CommandBuffer* cmd);
};

}  // namespace realcraft::rendering
