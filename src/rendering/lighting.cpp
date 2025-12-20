// RealCraft Rendering System
// lighting.cpp - Day/night cycle implementation

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <realcraft/rendering/lighting.hpp>

namespace realcraft::rendering {

DayNightCycle::DayNightCycle() = default;

void DayNightCycle::update(double delta_time) {
    if (paused_) {
        return;
    }

    // Convert real time to game time
    float game_delta = static_cast<float>(delta_time) * time_scale_ / day_length_;
    time_ += game_delta;

    // Wrap around at 1.0
    while (time_ >= 1.0f) {
        time_ -= 1.0f;
    }
    while (time_ < 0.0f) {
        time_ += 1.0f;
    }
}

void DayNightCycle::set_time(float time) {
    time_ = std::fmod(time, 1.0f);
    if (time_ < 0.0f) {
        time_ += 1.0f;
    }
}

glm::vec3 DayNightCycle::get_sun_direction() const {
    // Sun moves in an arc:
    // time 0.0 = midnight (sun below horizon)
    // time 0.25 = sunrise (east)
    // time 0.5 = noon (zenith)
    // time 0.75 = sunset (west)

    float angle = (time_ - 0.25f) * 2.0f * glm::pi<float>();

    glm::vec3 dir;
    dir.x = std::cos(angle);                // East-West
    dir.y = std::sin(angle);                // Up-Down
    dir.z = 0.3f * std::sin(angle * 0.5f);  // Slight north-south wobble

    return glm::normalize(dir);
}

glm::vec3 DayNightCycle::get_moon_direction() const {
    return -get_sun_direction();
}

glm::vec3 DayNightCycle::get_sun_color() const {
    // Sunrise/sunset: warm orange
    // Midday: white-ish
    // Night: very dim blue

    if (is_dawn()) {
        float t = (time_ - 0.20f) / 0.10f;  // 0 to 1 during dawn
        return glm::mix(glm::vec3(0.8f, 0.3f, 0.1f), glm::vec3(1.0f, 0.95f, 0.9f), t);
    } else if (is_dusk()) {
        float t = (time_ - 0.70f) / 0.10f;  // 0 to 1 during dusk
        return glm::mix(glm::vec3(1.0f, 0.95f, 0.9f), glm::vec3(0.8f, 0.3f, 0.1f), t);
    } else if (is_day()) {
        return glm::vec3(1.0f, 0.98f, 0.95f);
    } else {
        return glm::vec3(0.1f, 0.1f, 0.2f);  // Moonlight color
    }
}

glm::vec3 DayNightCycle::get_sky_zenith_color() const {
    if (is_dawn()) {
        float t = (time_ - 0.20f) / 0.10f;
        return glm::mix(glm::vec3(0.1f, 0.1f, 0.2f), glm::vec3(0.4f, 0.6f, 0.9f), t);
    } else if (is_dusk()) {
        float t = (time_ - 0.70f) / 0.10f;
        return glm::mix(glm::vec3(0.4f, 0.6f, 0.9f), glm::vec3(0.1f, 0.1f, 0.2f), t);
    } else if (is_day()) {
        return glm::vec3(0.4f, 0.6f, 0.9f);  // Blue sky
    } else {
        return glm::vec3(0.02f, 0.02f, 0.05f);  // Night sky
    }
}

glm::vec3 DayNightCycle::get_sky_horizon_color() const {
    if (is_dawn()) {
        float t = (time_ - 0.20f) / 0.10f;
        return glm::mix(glm::vec3(0.1f, 0.1f, 0.15f), glm::vec3(0.7f, 0.5f, 0.3f), t);
    } else if (is_dusk()) {
        float t = (time_ - 0.70f) / 0.10f;
        return glm::mix(glm::vec3(0.8f, 0.5f, 0.3f), glm::vec3(0.1f, 0.1f, 0.15f), t);
    } else if (is_day()) {
        return glm::vec3(0.7f, 0.8f, 0.95f);  // Lighter at horizon
    } else {
        return glm::vec3(0.05f, 0.05f, 0.1f);  // Dark horizon
    }
}

glm::vec3 DayNightCycle::get_fog_color() const {
    // Fog blends with sky horizon
    return get_sky_horizon_color();
}

float DayNightCycle::get_sun_intensity() const {
    if (is_dawn()) {
        float t = (time_ - 0.20f) / 0.10f;
        return glm::mix(0.0f, 1.0f, t);
    } else if (is_dusk()) {
        float t = (time_ - 0.70f) / 0.10f;
        return glm::mix(1.0f, 0.0f, t);
    } else if (is_day()) {
        return 1.0f;
    } else {
        return 0.0f;
    }
}

float DayNightCycle::get_ambient_intensity() const {
    if (is_day()) {
        return 0.3f;
    } else if (is_dawn() || is_dusk()) {
        return 0.2f;
    } else {
        return 0.1f;
    }
}

LightingUniforms DayNightCycle::get_uniforms() const {
    LightingUniforms u;

    u.sun_direction = get_sun_direction();
    u.sun_intensity = get_sun_intensity();
    u.sun_color = get_sun_color();

    u.moon_intensity = is_night() ? 0.15f : 0.0f;
    u.moon_color = glm::vec3(0.6f, 0.6f, 0.8f);

    u.ambient_intensity = get_ambient_intensity();
    u.ambient_color = glm::vec3(0.6f, 0.7f, 0.9f);

    u.time_of_day = time_;

    u.sky_zenith = get_sky_zenith_color();
    u.sky_horizon = get_sky_horizon_color();

    u.fog_color = get_fog_color();
    u.fog_start = fog_start_;
    u.fog_end = fog_end_;
    u.fog_density = fog_density_;

    return u;
}

}  // namespace realcraft::rendering
