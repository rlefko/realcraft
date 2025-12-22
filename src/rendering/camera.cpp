// RealCraft Rendering System
// camera.cpp - Camera implementation

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <realcraft/rendering/camera.hpp>

namespace realcraft::rendering {

Camera::Camera() : Camera(CameraConfig{}) {}

Camera::Camera(const CameraConfig& config) : config_(config) {
    update_vectors();
}

void Camera::set_position(const glm::dvec3& position) {
    position_ = position;
}

glm::dvec3 Camera::get_interpolated_position(double alpha) const {
    return glm::mix(previous_position_, position_, alpha);
}

void Camera::store_previous_position() {
    previous_position_ = position_;
}

void Camera::set_rotation(float pitch, float yaw) {
    pitch_ = pitch;
    yaw_ = yaw;
    clamp_pitch();
    vectors_dirty_ = true;
}

void Camera::rotate(float delta_pitch, float delta_yaw) {
    pitch_ += delta_pitch * config_.mouse_sensitivity;
    yaw_ += delta_yaw * config_.mouse_sensitivity;
    clamp_pitch();
    vectors_dirty_ = true;
}

glm::vec3 Camera::get_forward() const {
    if (vectors_dirty_) {
        update_vectors();
    }
    return forward_;
}

glm::vec3 Camera::get_right() const {
    if (vectors_dirty_) {
        update_vectors();
    }
    return right_;
}

glm::vec3 Camera::get_up() const {
    if (vectors_dirty_) {
        update_vectors();
    }
    return up_;
}

glm::vec3 Camera::get_forward_horizontal() const {
    if (vectors_dirty_) {
        update_vectors();
    }
    glm::vec3 forward_h = forward_;
    forward_h.y = 0.0f;
    float len = glm::length(forward_h);
    if (len > 0.0001f) {
        forward_h /= len;
    } else {
        forward_h = glm::vec3(0.0f, 0.0f, -1.0f);
    }
    return forward_h;
}

glm::mat4 Camera::get_view_matrix(double interpolation) const {
    if (vectors_dirty_) {
        update_vectors();
    }

    glm::vec3 pos = get_render_position(interpolation);
    // Use right-handed lookAt (standard GLM convention)
    return glm::lookAt(pos, pos + forward_, up_);
}

glm::mat4 Camera::get_projection_matrix(float aspect_ratio) const {
    // Use RH_ZO (Right-Handed, Zero-to-One depth) for Metal's coordinate system
    // Metal uses [0,1] depth range; RH matches our view matrix and frustum conventions
    return glm::perspectiveRH_ZO(glm::radians(config_.fov_degrees), aspect_ratio, config_.near_plane,
                                 config_.far_plane);
}

glm::mat4 Camera::get_view_projection_matrix(float aspect_ratio, double interpolation) const {
    return get_projection_matrix(aspect_ratio) * get_view_matrix(interpolation);
}

void Camera::process_movement(const glm::vec3& move_input, float delta_time, bool sprinting) {
    if (vectors_dirty_) {
        update_vectors();
    }

    float speed = config_.move_speed;
    if (sprinting) {
        speed *= config_.sprint_multiplier;
    }

    glm::dvec3 velocity{0.0};

    if (mode_ == CameraMode::FirstPerson) {
        // First person: move on XZ plane
        glm::vec3 forward_h = get_forward_horizontal();
        velocity += glm::dvec3(right_) * static_cast<double>(move_input.x);
        velocity += glm::dvec3(forward_h) * static_cast<double>(move_input.z);
    } else {
        // Spectator: full 3D movement
        velocity += glm::dvec3(right_) * static_cast<double>(move_input.x);
        velocity += glm::dvec3(0.0, 1.0, 0.0) * static_cast<double>(move_input.y);
        velocity += glm::dvec3(forward_) * static_cast<double>(move_input.z);
    }

    double len = glm::length(velocity);
    if (len > 0.0001) {
        velocity /= len;  // Normalize
        position_ += velocity * static_cast<double>(speed * delta_time);
    }
}

glm::vec3 Camera::get_render_position(double interpolation) const {
    glm::dvec3 interp_pos = get_interpolated_position(interpolation);
    glm::dvec3 render_pos = interp_pos - render_offset_;
    return glm::vec3(render_pos);
}

void Camera::clamp_pitch() {
    if (mode_ == CameraMode::FirstPerson) {
        pitch_ = std::clamp(pitch_, -89.0f, 89.0f);
    }
}

void Camera::update_vectors() const {
    float pitch_rad = glm::radians(pitch_);
    float yaw_rad = glm::radians(yaw_);

    forward_.x = std::cos(yaw_rad) * std::cos(pitch_rad);
    forward_.y = std::sin(pitch_rad);
    forward_.z = std::sin(yaw_rad) * std::cos(pitch_rad);
    forward_ = glm::normalize(forward_);

    // Right is perpendicular to forward on XZ plane
    right_ = glm::normalize(glm::cross(forward_, glm::vec3(0.0f, 1.0f, 0.0f)));

    // Up is perpendicular to both
    up_ = glm::normalize(glm::cross(right_, forward_));

    vectors_dirty_ = false;
}

}  // namespace realcraft::rendering
