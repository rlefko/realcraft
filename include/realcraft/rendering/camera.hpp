// RealCraft Rendering System
// camera.hpp - First-person and spectator camera

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace realcraft::rendering {

// Camera mode affects movement and rotation constraints
enum class CameraMode { FirstPerson, Spectator };

// Camera configuration
struct CameraConfig {
    float fov_degrees = 70.0f;
    float near_plane = 0.1f;
    float far_plane = 1000.0f;
    float mouse_sensitivity = 0.1f;
    float move_speed = 10.0f;
    float sprint_multiplier = 2.0f;
};

// Camera with view/projection matrices and interpolation support
class Camera {
public:
    Camera();
    explicit Camera(const CameraConfig& config);
    ~Camera() = default;

    // ========================================================================
    // Position (double precision for world coordinates)
    // ========================================================================

    void set_position(const glm::dvec3& position);
    [[nodiscard]] glm::dvec3 get_position() const { return position_; }

    // Get interpolated position for smooth rendering
    [[nodiscard]] glm::dvec3 get_interpolated_position(double alpha) const;

    // Store current position as previous (call at fixed timestep)
    void store_previous_position();

    // ========================================================================
    // Orientation (Euler angles in degrees)
    // ========================================================================

    void set_rotation(float pitch, float yaw);
    void rotate(float delta_pitch, float delta_yaw);

    [[nodiscard]] float get_pitch() const { return pitch_; }
    [[nodiscard]] float get_yaw() const { return yaw_; }

    // ========================================================================
    // Direction Vectors
    // ========================================================================

    [[nodiscard]] glm::vec3 get_forward() const;
    [[nodiscard]] glm::vec3 get_right() const;
    [[nodiscard]] glm::vec3 get_up() const;

    // Horizontal forward (for movement, ignores pitch)
    [[nodiscard]] glm::vec3 get_forward_horizontal() const;

    // ========================================================================
    // Matrix Generation
    // ========================================================================

    // View matrix using interpolated position
    [[nodiscard]] glm::mat4 get_view_matrix(double interpolation = 1.0) const;

    // Projection matrix for given aspect ratio
    [[nodiscard]] glm::mat4 get_projection_matrix(float aspect_ratio) const;

    // Combined view-projection matrix
    [[nodiscard]] glm::mat4 get_view_projection_matrix(float aspect_ratio, double interpolation = 1.0) const;

    // ========================================================================
    // Configuration
    // ========================================================================

    void set_config(const CameraConfig& config) { config_ = config; }
    [[nodiscard]] const CameraConfig& get_config() const { return config_; }

    void set_fov(float fov_degrees) { config_.fov_degrees = fov_degrees; }
    [[nodiscard]] float get_fov() const { return config_.fov_degrees; }

    void set_mode(CameraMode mode) { mode_ = mode; }
    [[nodiscard]] CameraMode get_mode() const { return mode_; }

    // ========================================================================
    // Movement Processing
    // ========================================================================

    // Process movement input (WASD normalized to -1 to 1)
    // move_input: x = right/left, y = up/down (spectator), z = forward/back
    void process_movement(const glm::vec3& move_input, float delta_time, bool sprinting = false);

    // ========================================================================
    // Render Offset (for origin shifting)
    // ========================================================================

    void set_render_offset(const glm::dvec3& offset) { render_offset_ = offset; }
    [[nodiscard]] glm::dvec3 get_render_offset() const { return render_offset_; }

    // Get position relative to render offset (float precision for GPU)
    [[nodiscard]] glm::vec3 get_render_position(double interpolation = 1.0) const;

private:
    CameraConfig config_;
    CameraMode mode_ = CameraMode::FirstPerson;

    // Position in world coordinates (double precision)
    glm::dvec3 position_{0.0, 64.0, 0.0};
    glm::dvec3 previous_position_{0.0, 64.0, 0.0};

    // Render offset for origin shifting
    glm::dvec3 render_offset_{0.0, 0.0, 0.0};

    // Orientation (degrees)
    float pitch_ = 0.0f;
    float yaw_ = -90.0f;  // Start facing -Z

    void clamp_pitch();
    void update_vectors() const;

    // Cached direction vectors
    mutable glm::vec3 forward_{0.0f, 0.0f, -1.0f};
    mutable glm::vec3 right_{1.0f, 0.0f, 0.0f};
    mutable glm::vec3 up_{0.0f, 1.0f, 0.0f};
    mutable bool vectors_dirty_ = true;
};

}  // namespace realcraft::rendering
