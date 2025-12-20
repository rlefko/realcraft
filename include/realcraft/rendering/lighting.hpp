// RealCraft Rendering System
// lighting.hpp - Day/night cycle and lighting uniforms

#pragma once

#include <glm/glm.hpp>

#include <cstdint>

namespace realcraft::rendering {

// GPU-side lighting uniform buffer (std140 layout compatible)
struct LightingUniforms {
    // Sun light
    alignas(16) glm::vec3 sun_direction;
    float sun_intensity;

    alignas(16) glm::vec3 sun_color;
    float moon_intensity;

    alignas(16) glm::vec3 moon_color;
    float ambient_intensity;

    alignas(16) glm::vec3 ambient_color;
    float time_of_day;  // 0.0 = midnight, 0.5 = noon, 1.0 = midnight

    // Sky colors
    alignas(16) glm::vec3 sky_zenith;
    float fog_density;

    alignas(16) glm::vec3 sky_horizon;
    float fog_start;

    alignas(16) glm::vec3 fog_color;
    float fog_end;

    // Padding to ensure 16-byte alignment
    alignas(16) float padding[4];
};

static_assert(sizeof(LightingUniforms) % 16 == 0, "LightingUniforms must be 16-byte aligned");

// Manages day/night cycle and generates lighting uniforms
class DayNightCycle {
public:
    DayNightCycle();
    ~DayNightCycle() = default;

    // ========================================================================
    // Time Control
    // ========================================================================

    // Update time (delta in real seconds)
    void update(double delta_time);

    // Set time directly (0.0 = midnight, 0.5 = noon, 1.0 = midnight)
    void set_time(float time);
    [[nodiscard]] float get_time() const { return time_; }

    // Time scale (1.0 = real-time, 20.0 = 20x faster)
    void set_time_scale(float scale) { time_scale_ = scale; }
    [[nodiscard]] float get_time_scale() const { return time_scale_; }

    // Day length in real seconds (default: 20 minutes = 1200s)
    void set_day_length(float seconds) { day_length_ = seconds; }
    [[nodiscard]] float get_day_length() const { return day_length_; }

    // Pause/unpause time
    void set_paused(bool paused) { paused_ = paused; }
    [[nodiscard]] bool is_paused() const { return paused_; }

    // ========================================================================
    // Sun/Moon Direction
    // ========================================================================

    [[nodiscard]] glm::vec3 get_sun_direction() const;
    [[nodiscard]] glm::vec3 get_moon_direction() const;

    // ========================================================================
    // Time of Day Queries
    // ========================================================================

    [[nodiscard]] bool is_day() const { return time_ > 0.25f && time_ < 0.75f; }
    [[nodiscard]] bool is_night() const { return !is_day(); }
    [[nodiscard]] bool is_dawn() const { return time_ >= 0.20f && time_ <= 0.30f; }
    [[nodiscard]] bool is_dusk() const { return time_ >= 0.70f && time_ <= 0.80f; }

    // ========================================================================
    // Lighting Uniforms
    // ========================================================================

    // Get complete lighting uniform buffer for current time
    [[nodiscard]] LightingUniforms get_uniforms() const;

    // ========================================================================
    // Configuration
    // ========================================================================

    // Fog settings
    void set_fog_start(float distance) { fog_start_ = distance; }
    void set_fog_end(float distance) { fog_end_ = distance; }
    void set_fog_density(float density) { fog_density_ = density; }

private:
    float time_ = 0.30f;          // Start at early morning
    float time_scale_ = 1.0f;     // Real-time
    float day_length_ = 1200.0f;  // 20 minutes
    bool paused_ = false;

    // Fog
    float fog_start_ = 100.0f;
    float fog_end_ = 200.0f;
    float fog_density_ = 0.02f;

    // Helper methods for color interpolation
    [[nodiscard]] glm::vec3 get_sun_color() const;
    [[nodiscard]] glm::vec3 get_sky_zenith_color() const;
    [[nodiscard]] glm::vec3 get_sky_horizon_color() const;
    [[nodiscard]] glm::vec3 get_fog_color() const;
    [[nodiscard]] float get_sun_intensity() const;
    [[nodiscard]] float get_ambient_intensity() const;
};

}  // namespace realcraft::rendering
