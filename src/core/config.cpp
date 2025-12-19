// RealCraft Engine Core
// config.cpp - JSON-based configuration system implementation

#include <nlohmann/json.hpp>

#include <realcraft/core/config.hpp>
#include <realcraft/core/logger.hpp>
#include <realcraft/platform/file_io.hpp>

namespace realcraft::core {

using json = nlohmann::json;

struct Config::Impl {
    json data;
    std::filesystem::path path;
    ChangeCallback change_callback;
    bool dirty = false;
};

Config::Config() : impl_(std::make_unique<Impl>()) {
    set_defaults();
}

Config::~Config() = default;

Config::Config(Config&&) noexcept = default;
Config& Config::operator=(Config&&) noexcept = default;

bool Config::load(const std::filesystem::path& path) {
    auto content = platform::FileSystem::read_text(path);
    if (!content) {
        REALCRAFT_LOG_ERROR(log_category::CONFIG, "Failed to read config file: {}", path.string());
        return false;
    }

    try {
        impl_->data = json::parse(*content);
        impl_->path = path;
        impl_->dirty = false;
        REALCRAFT_LOG_INFO(log_category::CONFIG, "Loaded config from: {}", path.string());
        return true;
    } catch (const json::parse_error& e) {
        REALCRAFT_LOG_ERROR(log_category::CONFIG, "Failed to parse config file: {}", e.what());
        return false;
    }
}

bool Config::save(const std::filesystem::path& path) const {
    // Ensure parent directory exists
    auto parent = path.parent_path();
    if (!parent.empty() && !platform::FileSystem::exists(parent)) {
        if (!platform::FileSystem::create_directories(parent)) {
            REALCRAFT_LOG_ERROR(log_category::CONFIG, "Failed to create config directory: {}", parent.string());
            return false;
        }
    }

    // Pretty print with 4 spaces indent
    std::string content = impl_->data.dump(4);

    if (!platform::FileSystem::write_text(path, content)) {
        REALCRAFT_LOG_ERROR(log_category::CONFIG, "Failed to write config file: {}", path.string());
        return false;
    }

    REALCRAFT_LOG_INFO(log_category::CONFIG, "Saved config to: {}", path.string());
    return true;
}

bool Config::save() const {
    if (impl_->path.empty()) {
        REALCRAFT_LOG_ERROR(log_category::CONFIG, "Cannot save config: no path specified");
        return false;
    }
    return save(impl_->path);
}

bool Config::load_or_create_default(const std::filesystem::path& path) {
    if (platform::FileSystem::exists(path)) {
        return load(path);
    }

    // Create default config
    set_defaults();
    impl_->path = path;

    if (!save(path)) {
        REALCRAFT_LOG_WARN(log_category::CONFIG, "Failed to save default config, using in-memory defaults");
    }

    return true;
}

std::filesystem::path Config::get_path() const {
    return impl_->path;
}

int Config::get_int(std::string_view section, std::string_view key, int default_value) const {
    try {
        if (impl_->data.contains(section) && impl_->data[std::string(section)].contains(key)) {
            return impl_->data[std::string(section)][std::string(key)].get<int>();
        }
    } catch (const json::exception&) {
        // Type mismatch, return default
    }
    return default_value;
}

double Config::get_double(std::string_view section, std::string_view key, double default_value) const {
    try {
        if (impl_->data.contains(section) && impl_->data[std::string(section)].contains(key)) {
            return impl_->data[std::string(section)][std::string(key)].get<double>();
        }
    } catch (const json::exception&) {
        // Type mismatch, return default
    }
    return default_value;
}

float Config::get_float(std::string_view section, std::string_view key, float default_value) const {
    return static_cast<float>(get_double(section, key, static_cast<double>(default_value)));
}

bool Config::get_bool(std::string_view section, std::string_view key, bool default_value) const {
    try {
        if (impl_->data.contains(section) && impl_->data[std::string(section)].contains(key)) {
            return impl_->data[std::string(section)][std::string(key)].get<bool>();
        }
    } catch (const json::exception&) {
        // Type mismatch, return default
    }
    return default_value;
}

std::string Config::get_string(std::string_view section, std::string_view key, std::string_view default_value) const {
    try {
        if (impl_->data.contains(section) && impl_->data[std::string(section)].contains(key)) {
            return impl_->data[std::string(section)][std::string(key)].get<std::string>();
        }
    } catch (const json::exception&) {
        // Type mismatch, return default
    }
    return std::string(default_value);
}

void Config::set_int(std::string_view section, std::string_view key, int value) {
    impl_->data[std::string(section)][std::string(key)] = value;
    impl_->dirty = true;
    if (impl_->change_callback) {
        impl_->change_callback(section, key);
    }
}

void Config::set_double(std::string_view section, std::string_view key, double value) {
    impl_->data[std::string(section)][std::string(key)] = value;
    impl_->dirty = true;
    if (impl_->change_callback) {
        impl_->change_callback(section, key);
    }
}

void Config::set_float(std::string_view section, std::string_view key, float value) {
    set_double(section, key, static_cast<double>(value));
}

void Config::set_bool(std::string_view section, std::string_view key, bool value) {
    impl_->data[std::string(section)][std::string(key)] = value;
    impl_->dirty = true;
    if (impl_->change_callback) {
        impl_->change_callback(section, key);
    }
}

void Config::set_string(std::string_view section, std::string_view key, std::string_view value) {
    impl_->data[std::string(section)][std::string(key)] = std::string(value);
    impl_->dirty = true;
    if (impl_->change_callback) {
        impl_->change_callback(section, key);
    }
}

bool Config::has(std::string_view section, std::string_view key) const {
    return impl_->data.contains(section) && impl_->data[std::string(section)].contains(key);
}

bool Config::has_section(std::string_view section) const {
    return impl_->data.contains(section);
}

bool Config::remove(std::string_view section, std::string_view key) {
    if (!has(section, key)) {
        return false;
    }
    impl_->data[std::string(section)].erase(std::string(key));
    impl_->dirty = true;
    return true;
}

bool Config::remove_section(std::string_view section) {
    if (!has_section(section)) {
        return false;
    }
    impl_->data.erase(std::string(section));
    impl_->dirty = true;
    return true;
}

void Config::set_change_callback(ChangeCallback callback) {
    impl_->change_callback = std::move(callback);
}

bool Config::is_dirty() const {
    return impl_->dirty;
}

void Config::mark_clean() {
    impl_->dirty = false;
}

void Config::set_defaults() {
    impl_->data =
        json{{config_section::GRAPHICS,
              {{config_key::FULLSCREEN, false},
               {config_key::VSYNC, true},
               {config_key::RESOLUTION_WIDTH, 1280},
               {config_key::RESOLUTION_HEIGHT, 720},
               {config_key::TARGET_FPS, 0},
               {config_key::VALIDATION_ENABLED, false}}},
             {config_section::AUDIO,
              {{config_key::MASTER_VOLUME, 1.0}, {config_key::MUSIC_VOLUME, 0.7}, {config_key::SFX_VOLUME, 1.0}}},
             {config_section::INPUT, {{config_key::MOUSE_SENSITIVITY, 1.0}, {config_key::INVERT_Y, false}}},
             {config_section::GAMEPLAY, {{config_key::VIEW_DISTANCE, 8}}},
             {config_section::DEBUG, {{config_key::SHOW_FPS, true}, {config_key::LOG_LEVEL, "info"}}}};
    impl_->dirty = true;
}

}  // namespace realcraft::core
