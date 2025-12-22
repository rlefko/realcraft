// RealCraft Rendering System
// render_system.cpp - Main rendering orchestrator

#include <realcraft/core/logger.hpp>
#include <realcraft/graphics/shader_compiler.hpp>
#include <realcraft/graphics/swap_chain.hpp>
#include <realcraft/rendering/render_system.hpp>

namespace realcraft::rendering {

RenderSystem::RenderSystem() = default;

RenderSystem::~RenderSystem() {
    shutdown();
}

bool RenderSystem::initialize(core::Engine* engine, world::WorldManager* world, const RenderSystemConfig& config) {
    if (engine_ != nullptr) {
        REALCRAFT_LOG_WARN(core::log_category::GRAPHICS, "RenderSystem already initialized");
        return true;
    }

    engine_ = engine;
    world_ = world;
    device_ = engine->get_graphics_device();
    config_ = config;

    camera_ = Camera(config.camera);

    // Register as world observer
    world_->add_observer(this);

    // Initialize subsystems
    mesh_manager_ = std::make_unique<MeshManager>();
    if (!mesh_manager_->initialize(device_, world_, config_.mesh_manager)) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to initialize MeshManager");
        shutdown();
        return false;
    }

    texture_manager_ = std::make_unique<TextureManager>();
    if (!texture_manager_->initialize(device_, config_.texture_manager)) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to initialize TextureManager");
        shutdown();
        return false;
    }

    // Create shaders and pipelines
    if (!create_shaders()) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create shaders");
        shutdown();
        return false;
    }

    if (!create_pipelines()) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create pipelines");
        shutdown();
        return false;
    }

    if (!create_uniform_buffers()) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create uniform buffers");
        shutdown();
        return false;
    }

    // Create depth buffer (use framebuffer size for Retina displays)
    auto* window = engine_->get_window();
    auto fb_size = window->get_framebuffer_size();
    if (!create_depth_texture(fb_size.x, fb_size.y)) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create depth texture");
        shutdown();
        return false;
    }

    // Create selection highlight resources
    if (!create_selection_resources()) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create selection resources");
        shutdown();
        return false;
    }

    // Initialize HUD renderer
    hud_renderer_ = std::make_unique<HUDRenderer>();
    if (!hud_renderer_->initialize(device_, texture_manager_->get_block_atlas())) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to initialize HUDRenderer");
        shutdown();
        return false;
    }

    REALCRAFT_LOG_INFO(core::log_category::GRAPHICS, "RenderSystem initialized");
    return true;
}

void RenderSystem::shutdown() {
    if (engine_ == nullptr) {
        return;
    }

    if (world_) {
        world_->remove_observer(this);
    }

    // HUD renderer
    hud_renderer_.reset();

    depth_texture_.reset();
    camera_uniform_buffer_.reset();
    lighting_uniform_buffer_.reset();

    // Selection resources
    selection_vertex_buffer_.reset();
    selection_index_buffer_.reset();
    selection_uniform_buffer_.reset();
    selection_pipeline_.reset();
    selection_vertex_shader_.reset();
    selection_fragment_shader_.reset();

    voxel_pipeline_.reset();
    voxel_wireframe_pipeline_.reset();
    sky_pipeline_.reset();

    voxel_vertex_shader_.reset();
    voxel_fragment_shader_.reset();
    sky_vertex_shader_.reset();
    sky_fragment_shader_.reset();

    texture_manager_.reset();
    mesh_manager_.reset();

    engine_ = nullptr;
    world_ = nullptr;
    device_ = nullptr;

    REALCRAFT_LOG_INFO(core::log_category::GRAPHICS, "RenderSystem shutdown");
}

void RenderSystem::fixed_update(double /*dt*/) {
    camera_.store_previous_position();
}

void RenderSystem::update(double dt) {
    total_time_ += static_cast<float>(dt);
    day_night_cycle_.update(dt);
    mesh_manager_->update();
}

void RenderSystem::render(double interpolation) {
    stats_ = RenderStats{};

    auto* swap_chain = device_->get_swap_chain();
    if (!swap_chain || !swap_chain->get_current_texture()) {
        return;
    }

    // Check if window was resized (use framebuffer size for Retina displays)
    auto* window = engine_->get_window();
    auto window_size = window->get_framebuffer_size();
    uint32_t width = window_size.x;
    uint32_t height = window_size.y;

    if (!depth_texture_ || depth_texture_->get_width() != width || depth_texture_->get_height() != height) {
        create_depth_texture(width, height);
    }

    // Begin frame
    device_->begin_frame();

    auto cmd = device_->create_command_buffer();
    cmd->begin();

    // Update uniforms
    update_uniform_buffers(interpolation);

    // Update frustum culler
    float aspect = static_cast<float>(width) / static_cast<float>(height);
    glm::mat4 vp = camera_.get_view_projection_matrix(aspect, interpolation);
    culler_.update(vp);

    // Begin main render pass
    graphics::RenderPassDesc pass_desc;
    pass_desc.color_attachments.push_back(
        {graphics::TextureFormat::BGRA8Unorm, graphics::LoadOp::Clear, graphics::StoreOp::Store});
    pass_desc.depth_attachment = graphics::AttachmentDesc{graphics::TextureFormat::Depth32Float,
                                                          graphics::LoadOp::Clear, graphics::StoreOp::DontCare};

    // Clear to sky color
    auto lighting = day_night_cycle_.get_uniforms();
    graphics::ClearValue color_clear =
        graphics::ClearValue::Color(lighting.sky_horizon.r, lighting.sky_horizon.g, lighting.sky_horizon.b, 1.0f);
    graphics::ClearValue depth_clear = graphics::ClearValue::DepthStencil(1.0f, 0);

    cmd->begin_render_pass(pass_desc, swap_chain->get_current_texture(), depth_texture_.get(), color_clear,
                           depth_clear);

    // Set viewport and scissor
    graphics::Viewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<float>(width);
    viewport.height = static_cast<float>(height);
    viewport.min_depth = 0.0f;
    viewport.max_depth = 1.0f;
    cmd->set_viewport(viewport);

    graphics::Rect scissor;
    scissor.x = 0;
    scissor.y = 0;
    scissor.width = width;
    scissor.height = height;
    cmd->set_scissor(scissor);

    // Render chunks
    render_chunks(cmd.get());

    // Render block selection highlight (if any)
    render_selection_highlight(cmd.get());

    // Render HUD (crosshair, hotbar, health/hunger, debug overlay)
    // Use window size (not framebuffer size) for HUD so it scales correctly on Retina displays
    if (hud_renderer_) {
        auto hud_size = window->get_size();
        hud_renderer_->render(cmd.get(), hud_size.x, hud_size.y);
    }

    cmd->end_render_pass();

    cmd->end();
    device_->submit(cmd.get(), false);

    device_->end_frame();
}

void RenderSystem::set_render_distance(float chunks) {
    config_.render_distance = chunks;
}

void RenderSystem::set_wireframe(bool enabled) {
    config_.enable_wireframe = enabled;
}

void RenderSystem::on_chunk_loaded(const world::ChunkPos& pos, world::Chunk& chunk) {
    mesh_manager_->on_chunk_loaded(pos, chunk);
}

void RenderSystem::on_chunk_unloading(const world::ChunkPos& pos, const world::Chunk& chunk) {
    mesh_manager_->on_chunk_unloading(pos, chunk);
}

void RenderSystem::on_block_changed(const world::BlockChangeEvent& event) {
    mesh_manager_->on_block_changed(event);
}

void RenderSystem::on_origin_shifted(const world::WorldBlockPos& old_origin, const world::WorldBlockPos& new_origin) {
    origin_offset_ = new_origin;
    camera_.set_render_offset(glm::dvec3(new_origin.x, new_origin.y, new_origin.z));
    mesh_manager_->on_origin_shifted(old_origin, new_origin);
}

bool RenderSystem::create_shaders() {
    // For now, create simple passthrough shaders
    // In a full implementation, these would be loaded from files and compiled

    graphics::ShaderCompiler compiler;

    // Simple voxel vertex shader
    const char* voxel_vert_source = R"(
#version 450

layout(location = 0) in vec4 in_position;
layout(location = 1) in vec4 in_normal_ao;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in uvec4 in_tex_light;
layout(location = 4) in vec4 in_color;

layout(location = 0) out vec3 frag_position;
layout(location = 1) out vec3 frag_normal;
layout(location = 2) out vec2 frag_uv;
layout(location = 3) out float frag_ao;
layout(location = 4) out vec4 frag_color;

layout(set = 0, binding = 0) uniform CameraUniforms {
    mat4 view;
    mat4 projection;
    mat4 view_projection;
    vec3 camera_position;
    float time;
} camera;

layout(push_constant) uniform PushConstants {
    vec3 chunk_offset;
} push;

void main() {
    vec3 world_pos = in_position.xyz + push.chunk_offset;

    // Transform to clip space using view-projection matrix
    gl_Position = camera.view_projection * vec4(world_pos, 1.0);

    frag_position = world_pos;
    // Normal is already normalized to [-1, 1] by RGBA8Snorm vertex format
    frag_normal = in_normal_ao.xyz;
    frag_uv = in_uv;
    // AO: stored as 0-127 int8_t, normalized to [0, 1] by Snorm
    // Note: AO values > 127 will wrap negative due to signed interpretation
    frag_ao = max(in_normal_ao.w, 0.0);
    frag_color = in_color;
}
)";

    // Simple voxel fragment shader
    const char* voxel_frag_source = R"(
#version 450

layout(location = 0) in vec3 frag_position;
layout(location = 1) in vec3 frag_normal;
layout(location = 2) in vec2 frag_uv;
layout(location = 3) in float frag_ao;
layout(location = 4) in vec4 frag_color;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 1) uniform LightingUniforms {
    vec3 sun_direction;
    float sun_intensity;
    vec3 sun_color;
    float moon_intensity;
    vec3 moon_color;
    float ambient_intensity;
    vec3 ambient_color;
    float time_of_day;
    vec3 sky_zenith;
    float fog_density;
    vec3 sky_horizon;
    float fog_start;
    vec3 fog_color;
    float fog_end;
} lighting;

layout(set = 0, binding = 0) uniform CameraUniforms {
    mat4 view;
    mat4 projection;
    mat4 view_projection;
    vec3 camera_position;
    float time;
} camera;

void main() {
    vec3 N = normalize(frag_normal);

    // Directional lighting
    float NdotL = max(dot(N, lighting.sun_direction), 0.0);
    vec3 diffuse = lighting.sun_color * lighting.sun_intensity * NdotL;

    // Ambient
    vec3 ambient = lighting.ambient_color * lighting.ambient_intensity * frag_ao;

    // Base color from vertex color (white for now)
    vec3 albedo = frag_color.rgb;

    // Combine
    vec3 final_color = albedo * (ambient + diffuse);

    // Fog
    float distance = length(frag_position - camera.camera_position);
    float fog_factor = clamp((distance - lighting.fog_start) / (lighting.fog_end - lighting.fog_start), 0.0, 1.0);
    final_color = mix(final_color, lighting.fog_color, fog_factor);

    out_color = vec4(final_color, 1.0);
}
)";

    // Compile shaders
    graphics::ShaderCompileOptions vert_options;
    vert_options.stage = graphics::ShaderStage::Vertex;
    vert_options.entry_point = "main";

    auto voxel_vert_result = compiler.compile_glsl(voxel_vert_source, vert_options);
    if (!voxel_vert_result.success) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to compile voxel vertex shader: {}",
                            voxel_vert_result.error_message);
        return false;
    }

    graphics::ShaderCompileOptions frag_options;
    frag_options.stage = graphics::ShaderStage::Fragment;
    frag_options.entry_point = "main";

    auto voxel_frag_result = compiler.compile_glsl(voxel_frag_source, frag_options);
    if (!voxel_frag_result.success) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to compile voxel fragment shader: {}",
                            voxel_frag_result.error_message);
        return false;
    }

    // Create shader objects
    graphics::ShaderDesc vert_desc;
    vert_desc.stage = graphics::ShaderStage::Vertex;
    vert_desc.bytecode =
        voxel_vert_result.msl_bytecode.empty()
            ? std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(voxel_vert_result.msl_source.data()),
                                       voxel_vert_result.msl_source.size())
            : std::span<const uint8_t>(voxel_vert_result.msl_bytecode);
    vert_desc.entry_point = "main0";
    vert_desc.debug_name = "voxel_vertex";

    voxel_vertex_shader_ = device_->create_shader(vert_desc);
    if (!voxel_vertex_shader_) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create voxel vertex shader");
        return false;
    }

    graphics::ShaderDesc frag_desc;
    frag_desc.stage = graphics::ShaderStage::Fragment;
    frag_desc.bytecode =
        voxel_frag_result.msl_bytecode.empty()
            ? std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(voxel_frag_result.msl_source.data()),
                                       voxel_frag_result.msl_source.size())
            : std::span<const uint8_t>(voxel_frag_result.msl_bytecode);
    frag_desc.entry_point = "main0";
    frag_desc.debug_name = "voxel_fragment";

    voxel_fragment_shader_ = device_->create_shader(frag_desc);
    if (!voxel_fragment_shader_) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create voxel fragment shader");
        return false;
    }

    REALCRAFT_LOG_INFO(core::log_category::GRAPHICS, "Shaders compiled successfully");
    return true;
}

bool RenderSystem::create_pipelines() {
    graphics::PipelineDesc pipeline_desc;
    pipeline_desc.vertex_shader = voxel_vertex_shader_.get();
    pipeline_desc.fragment_shader = voxel_fragment_shader_.get();

    // Vertex attributes
    pipeline_desc.vertex_attributes = VoxelVertex::get_attributes();
    pipeline_desc.vertex_bindings = {VoxelVertex::get_binding()};

    pipeline_desc.topology = graphics::PrimitiveTopology::TriangleList;

    // Rasterizer
    pipeline_desc.rasterizer.cull_mode = graphics::CullMode::Back;
    pipeline_desc.rasterizer.front_face = graphics::FrontFace::CounterClockwise;

    // Depth
    pipeline_desc.depth_stencil.depth_test_enable = true;
    pipeline_desc.depth_stencil.depth_write_enable = true;
    pipeline_desc.depth_stencil.depth_compare = graphics::CompareOp::Less;

    // Blend (no blending for opaque)
    pipeline_desc.color_blend.push_back(graphics::BlendState{});

    // Formats
    pipeline_desc.color_formats = {graphics::TextureFormat::BGRA8Unorm};
    pipeline_desc.depth_format = graphics::TextureFormat::Depth32Float;

    pipeline_desc.debug_name = "voxel_pipeline";

    voxel_pipeline_ = device_->create_pipeline(pipeline_desc);
    if (!voxel_pipeline_) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create voxel pipeline");
        return false;
    }

    // Wireframe pipeline
    pipeline_desc.rasterizer.wireframe = true;
    pipeline_desc.debug_name = "voxel_wireframe_pipeline";

    voxel_wireframe_pipeline_ = device_->create_pipeline(pipeline_desc);
    if (!voxel_wireframe_pipeline_) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create wireframe pipeline");
        return false;
    }

    REALCRAFT_LOG_INFO(core::log_category::GRAPHICS, "Pipelines created successfully");
    return true;
}

bool RenderSystem::create_uniform_buffers() {
    graphics::BufferDesc camera_desc;
    camera_desc.size = sizeof(CameraUniforms);
    camera_desc.usage = graphics::BufferUsage::Uniform;
    camera_desc.host_visible = true;
    camera_desc.debug_name = "CameraUniforms";

    camera_uniform_buffer_ = device_->create_buffer(camera_desc);
    if (!camera_uniform_buffer_) {
        return false;
    }

    graphics::BufferDesc lighting_desc;
    lighting_desc.size = sizeof(LightingUniforms);
    lighting_desc.usage = graphics::BufferUsage::Uniform;
    lighting_desc.host_visible = true;
    lighting_desc.debug_name = "LightingUniforms";

    lighting_uniform_buffer_ = device_->create_buffer(lighting_desc);
    if (!lighting_uniform_buffer_) {
        return false;
    }

    return true;
}

bool RenderSystem::create_depth_texture(uint32_t width, uint32_t height) {
    graphics::TextureDesc depth_desc;
    depth_desc.type = graphics::TextureType::Texture2D;
    depth_desc.format = graphics::TextureFormat::Depth32Float;
    depth_desc.width = width;
    depth_desc.height = height;
    depth_desc.usage = graphics::TextureUsage::DepthStencil;
    depth_desc.debug_name = "DepthBuffer";

    depth_texture_ = device_->create_texture(depth_desc);
    return depth_texture_ != nullptr;
}

void RenderSystem::update_uniform_buffers(double interpolation) {
    auto* window = engine_->get_window();
    // Use framebuffer size for correct aspect ratio on Retina displays
    auto fb_size = window->get_framebuffer_size();
    float aspect = static_cast<float>(fb_size.x) / static_cast<float>(fb_size.y);

    // Camera uniforms
    CameraUniforms cam;
    cam.view = camera_.get_view_matrix(interpolation);
    cam.projection = camera_.get_projection_matrix(aspect);
    cam.view_projection = cam.projection * cam.view;
    cam.camera_position = camera_.get_render_position(interpolation);
    cam.time = total_time_;

    camera_uniform_buffer_->write(&cam, sizeof(cam));

    // Lighting uniforms
    LightingUniforms light = day_night_cycle_.get_uniforms();
    lighting_uniform_buffer_->write(&light, sizeof(light));
}

void RenderSystem::render_chunks(graphics::CommandBuffer* cmd) {
    auto* pipeline = config_.enable_wireframe ? voxel_wireframe_pipeline_.get() : voxel_pipeline_.get();
    cmd->bind_pipeline(pipeline);

    // Bind uniform buffers
    cmd->bind_uniform_buffer(0, camera_uniform_buffer_.get());
    cmd->bind_uniform_buffer(1, lighting_uniform_buffer_.get());

    // Render each chunk mesh
    mesh_manager_->for_each_mesh([&](const world::ChunkPos& pos, const ChunkMesh& mesh) {
        if (mesh.is_empty()) {
            return;
        }

        // Calculate chunk render position
        glm::vec3 chunk_offset(static_cast<float>(pos.x * world::CHUNK_SIZE_X - origin_offset_.x),
                               static_cast<float>(-origin_offset_.y),
                               static_cast<float>(pos.y * world::CHUNK_SIZE_Z - origin_offset_.z));

        // Frustum cull
        if (!culler_.is_chunk_visible(chunk_offset, world::CHUNK_SIZE_X, world::CHUNK_SIZE_Y, world::CHUNK_SIZE_Z)) {
            stats_.chunks_culled++;
            return;
        }

        // Set chunk offset via push constants
        cmd->push_constants(graphics::ShaderStage::Vertex, 0, sizeof(chunk_offset), &chunk_offset);

        // Draw opaque geometry
        if (mesh.has_opaque()) {
            cmd->bind_vertex_buffer(0, mesh.get_opaque_vertex_buffer());
            cmd->bind_index_buffer(mesh.get_opaque_index_buffer(), graphics::IndexType::Uint32);
            cmd->draw_indexed(mesh.get_opaque_index_count(), 1, 0, 0, 0);

            stats_.draw_calls++;
            stats_.triangles_rendered += mesh.get_opaque_index_count() / 3;
        }

        stats_.chunks_rendered++;
    });
}

void RenderSystem::render_sky(graphics::CommandBuffer* /*cmd*/) {
    // Sky rendering would go here - for now, we just clear to sky color
}

void RenderSystem::set_block_selection(const world::WorldBlockPos* block_pos, float break_progress) {
    if (block_pos) {
        has_selection_ = true;
        selection_block_pos_ = *block_pos;
        selection_break_progress_ = break_progress;
    } else {
        has_selection_ = false;
    }
}

void RenderSystem::clear_block_selection() {
    has_selection_ = false;
}

bool RenderSystem::create_selection_resources() {
    graphics::ShaderCompiler compiler;

    // Selection highlight vertex shader
    const char* selection_vert_source = R"(
#version 450

layout(location = 0) in vec4 in_position;

layout(location = 0) out vec3 frag_local_pos;

layout(set = 0, binding = 0) uniform CameraUniforms {
    mat4 view;
    mat4 projection;
    mat4 view_projection;
    vec3 camera_position;
    float time;
} camera;

layout(push_constant) uniform SelectionPushConstants {
    vec3 block_position;
    float break_progress;
} selection;

void main() {
    // Scale slightly to avoid z-fighting (1.004x)
    vec3 pos = in_position.xyz;
    vec3 scaled_pos = (pos - 0.5) * 1.004 + 0.5;
    vec3 world_pos = scaled_pos + selection.block_position;

    gl_Position = camera.view_projection * vec4(world_pos, 1.0);
    frag_local_pos = pos;
}
)";

    // Selection highlight fragment shader
    const char* selection_frag_source = R"(
#version 450

layout(location = 0) in vec3 frag_local_pos;

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform SelectionPushConstants {
    vec3 block_position;
    float break_progress;
} selection;

void main() {
    // Calculate edge factor for wireframe effect
    vec3 edge_dist = min(frag_local_pos, 1.0 - frag_local_pos);
    float min_edge = min(min(edge_dist.x, edge_dist.y), edge_dist.z);

    // Line width (thicker when breaking)
    float line_width = mix(0.02, 0.04, selection.break_progress);
    float edge_factor = smoothstep(0.0, line_width, min_edge);

    // Black outline
    vec3 line_color = vec3(0.0, 0.0, 0.0);

    // Interior shows break progress as darkening
    vec3 fill_color = vec3(0.0, 0.0, 0.0);
    float fill_alpha = selection.break_progress * 0.4;

    // Combine: edges are black lines, interior shows cracks
    if (edge_factor < 0.5) {
        // On edge - draw black line
        out_color = vec4(line_color, 0.8);
    } else {
        // Interior - show break progress
        out_color = vec4(fill_color, fill_alpha);
    }
}
)";

    // Compile selection vertex shader
    graphics::ShaderCompileOptions vert_options;
    vert_options.stage = graphics::ShaderStage::Vertex;
    vert_options.entry_point = "main";

    auto sel_vert_result = compiler.compile_glsl(selection_vert_source, vert_options);
    if (!sel_vert_result.success) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to compile selection vertex shader: {}",
                            sel_vert_result.error_message);
        return false;
    }

    graphics::ShaderCompileOptions frag_options;
    frag_options.stage = graphics::ShaderStage::Fragment;
    frag_options.entry_point = "main";

    auto sel_frag_result = compiler.compile_glsl(selection_frag_source, frag_options);
    if (!sel_frag_result.success) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to compile selection fragment shader: {}",
                            sel_frag_result.error_message);
        return false;
    }

    // Create shader objects
    graphics::ShaderDesc vert_desc;
    vert_desc.stage = graphics::ShaderStage::Vertex;
    vert_desc.bytecode =
        sel_vert_result.msl_bytecode.empty()
            ? std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(sel_vert_result.msl_source.data()),
                                       sel_vert_result.msl_source.size())
            : std::span<const uint8_t>(sel_vert_result.msl_bytecode);
    vert_desc.entry_point = "main0";
    vert_desc.debug_name = "selection_vertex";

    selection_vertex_shader_ = device_->create_shader(vert_desc);
    if (!selection_vertex_shader_) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create selection vertex shader");
        return false;
    }

    graphics::ShaderDesc frag_desc;
    frag_desc.stage = graphics::ShaderStage::Fragment;
    frag_desc.bytecode =
        sel_frag_result.msl_bytecode.empty()
            ? std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(sel_frag_result.msl_source.data()),
                                       sel_frag_result.msl_source.size())
            : std::span<const uint8_t>(sel_frag_result.msl_bytecode);
    frag_desc.entry_point = "main0";
    frag_desc.debug_name = "selection_fragment";

    selection_fragment_shader_ = device_->create_shader(frag_desc);
    if (!selection_fragment_shader_) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create selection fragment shader");
        return false;
    }

    // Create selection pipeline
    graphics::PipelineDesc pipeline_desc;
    pipeline_desc.vertex_shader = selection_vertex_shader_.get();
    pipeline_desc.fragment_shader = selection_fragment_shader_.get();

    // vec4 vertex attribute (xyz position, w unused)
    pipeline_desc.vertex_attributes = {{0, 0, graphics::TextureFormat::RGBA32Float, 0}};
    pipeline_desc.vertex_bindings = {{0, sizeof(float) * 4, false}};

    pipeline_desc.topology = graphics::PrimitiveTopology::TriangleList;

    // Rasterizer - no culling to see all faces of selection
    pipeline_desc.rasterizer.cull_mode = graphics::CullMode::None;
    pipeline_desc.rasterizer.front_face = graphics::FrontFace::CounterClockwise;

    // Depth - test but don't write (render on top of world)
    pipeline_desc.depth_stencil.depth_test_enable = true;
    pipeline_desc.depth_stencil.depth_write_enable = false;
    pipeline_desc.depth_stencil.depth_compare = graphics::CompareOp::LessOrEqual;

    // Alpha blending
    graphics::BlendState blend;
    blend.enable = true;
    blend.src_color = graphics::BlendFactor::SrcAlpha;
    blend.dst_color = graphics::BlendFactor::OneMinusSrcAlpha;
    blend.color_op = graphics::BlendOp::Add;
    blend.src_alpha = graphics::BlendFactor::One;
    blend.dst_alpha = graphics::BlendFactor::Zero;
    blend.alpha_op = graphics::BlendOp::Add;
    pipeline_desc.color_blend.push_back(blend);

    pipeline_desc.color_formats = {graphics::TextureFormat::BGRA8Unorm};
    pipeline_desc.depth_format = graphics::TextureFormat::Depth32Float;
    pipeline_desc.debug_name = "selection_pipeline";

    selection_pipeline_ = device_->create_pipeline(pipeline_desc);
    if (!selection_pipeline_) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create selection pipeline");
        return false;
    }

    // Create cube vertex buffer (unit cube from 0 to 1, vec4 with w=0)
    // clang-format off
    float cube_vertices[] = {
        // Front face (z = 1)
        0.0f, 0.0f, 1.0f, 0.0f,  1.0f, 0.0f, 1.0f, 0.0f,  1.0f, 1.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,  1.0f, 1.0f, 1.0f, 0.0f,  0.0f, 1.0f, 1.0f, 0.0f,
        // Back face (z = 0)
        1.0f, 0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f, 0.0f,  1.0f, 1.0f, 0.0f, 0.0f,
        // Top face (y = 1)
        0.0f, 1.0f, 1.0f, 0.0f,  1.0f, 1.0f, 1.0f, 0.0f,  1.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 1.0f, 0.0f,  1.0f, 1.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f, 0.0f,
        // Bottom face (y = 0)
        0.0f, 0.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f, 0.0f,  1.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,  1.0f, 0.0f, 1.0f, 0.0f,  0.0f, 0.0f, 1.0f, 0.0f,
        // Right face (x = 1)
        1.0f, 0.0f, 1.0f, 0.0f,  1.0f, 0.0f, 0.0f, 0.0f,  1.0f, 1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 1.0f, 0.0f,  1.0f, 1.0f, 0.0f, 0.0f,  1.0f, 1.0f, 1.0f, 0.0f,
        // Left face (x = 0)
        0.0f, 0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f, 0.0f,  0.0f, 1.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,  0.0f, 1.0f, 1.0f, 0.0f,  0.0f, 1.0f, 0.0f, 0.0f,
    };
    // clang-format on

    graphics::BufferDesc vb_desc;
    vb_desc.size = sizeof(cube_vertices);
    vb_desc.usage = graphics::BufferUsage::Vertex;
    vb_desc.host_visible = true;
    vb_desc.initial_data = cube_vertices;
    vb_desc.debug_name = "SelectionCubeVB";

    selection_vertex_buffer_ = device_->create_buffer(vb_desc);
    if (!selection_vertex_buffer_) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create selection vertex buffer");
        return false;
    }

    REALCRAFT_LOG_INFO(core::log_category::GRAPHICS, "Selection resources created successfully");
    return true;
}

void RenderSystem::render_selection_highlight(graphics::CommandBuffer* cmd) {
    if (!has_selection_ || !selection_pipeline_) {
        return;
    }

    cmd->bind_pipeline(selection_pipeline_.get());

    // Bind camera uniform buffer (same as chunks)
    cmd->bind_uniform_buffer(0, camera_uniform_buffer_.get());

    // Calculate block position relative to render origin
    glm::vec3 block_pos(static_cast<float>(selection_block_pos_.x - origin_offset_.x),
                        static_cast<float>(selection_block_pos_.y - origin_offset_.y),
                        static_cast<float>(selection_block_pos_.z - origin_offset_.z));

    // Push constants: block position and break progress
    struct SelectionPushConstants {
        glm::vec3 block_position;
        float break_progress;
    } push;
    push.block_position = block_pos;
    push.break_progress = selection_break_progress_;

    cmd->push_constants(graphics::ShaderStage::Vertex, 0, sizeof(push), &push);
    cmd->push_constants(graphics::ShaderStage::Fragment, 0, sizeof(push), &push);

    // Draw the selection cube
    cmd->bind_vertex_buffer(0, selection_vertex_buffer_.get());
    cmd->draw(36, 1, 0, 0);  // 36 vertices (6 faces * 2 triangles * 3 vertices)
}

}  // namespace realcraft::rendering
