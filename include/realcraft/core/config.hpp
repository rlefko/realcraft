// RealCraft Engine Core
// config.hpp - JSON-based configuration system

#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace realcraft::core {

// Configuration system with JSON file persistence
class Config {
public:
    Config();
    ~Config();

    // Non-copyable but movable
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    Config(Config&&) noexcept;
    Config& operator=(Config&&) noexcept;

    // Load/Save operations
    bool load(const std::filesystem::path& path);
    bool save(const std::filesystem::path& path) const;
    bool save() const;  // Save to loaded path
    bool load_or_create_default(const std::filesystem::path& path);

    // Get the currently loaded file path
    [[nodiscard]] std::filesystem::path get_path() const;

    // Typed getters with defaults
    [[nodiscard]] int get_int(std::string_view section, std::string_view key,
                               int default_value = 0) const;
    [[nodiscard]] double get_double(std::string_view section, std::string_view key,
                                     double default_value = 0.0) const;
    [[nodiscard]] float get_float(std::string_view section, std::string_view key,
                                   float default_value = 0.0f) const;
    [[nodiscard]] bool get_bool(std::string_view section, std::string_view key,
                                 bool default_value = false) const;
    [[nodiscard]] std::string get_string(std::string_view section, std::string_view key,
                                          std::string_view default_value = "") const;

    // Setters
    void set_int(std::string_view section, std::string_view key, int value);
    void set_double(std::string_view section, std::string_view key, double value);
    void set_float(std::string_view section, std::string_view key, float value);
    void set_bool(std::string_view section, std::string_view key, bool value);
    void set_string(std::string_view section, std::string_view key, std::string_view value);

    // Check existence
    [[nodiscard]] bool has(std::string_view section, std::string_view key) const;
    [[nodiscard]] bool has_section(std::string_view section) const;

    // Remove entries
    bool remove(std::string_view section, std::string_view key);
    bool remove_section(std::string_view section);

    // Change notification callback
    using ChangeCallback = std::function<void(std::string_view section, std::string_view key)>;
    void set_change_callback(ChangeCallback callback);

    // Dirty tracking (for auto-save)
    [[nodiscard]] bool is_dirty() const;
    void mark_clean();

    // Create default configuration
    void set_defaults();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Pre-defined section names for consistency
namespace config_section {
    inline constexpr const char* GRAPHICS = "graphics";
    inline constexpr const char* AUDIO = "audio";
    inline constexpr const char* INPUT = "input";
    inline constexpr const char* GAMEPLAY = "gameplay";
    inline constexpr const char* DEBUG = "debug";
}  // namespace config_section

// Pre-defined key names for consistency
namespace config_key {
    // Graphics section
    inline constexpr const char* FULLSCREEN = "fullscreen";
    inline constexpr const char* VSYNC = "vsync";
    inline constexpr const char* RESOLUTION_WIDTH = "resolution_width";
    inline constexpr const char* RESOLUTION_HEIGHT = "resolution_height";
    inline constexpr const char* TARGET_FPS = "target_fps";
    inline constexpr const char* VALIDATION_ENABLED = "validation_enabled";

    // Audio section
    inline constexpr const char* MASTER_VOLUME = "master_volume";
    inline constexpr const char* MUSIC_VOLUME = "music_volume";
    inline constexpr const char* SFX_VOLUME = "sfx_volume";

    // Input section
    inline constexpr const char* MOUSE_SENSITIVITY = "mouse_sensitivity";
    inline constexpr const char* INVERT_Y = "invert_y";

    // Gameplay section
    inline constexpr const char* VIEW_DISTANCE = "view_distance";

    // Debug section
    inline constexpr const char* SHOW_FPS = "show_fps";
    inline constexpr const char* LOG_LEVEL = "log_level";
}  // namespace config_key

}  // namespace realcraft::core
