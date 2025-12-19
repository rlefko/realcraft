// RealCraft Engine Core
// logger.hpp - Logging system with categories and file output

#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>

namespace realcraft::core {

// Log levels matching spdlog for easy conversion
enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Critical = 5,
    Off = 6
};

// Logger configuration
struct LoggerConfig {
    LogLevel console_level = LogLevel::Info;
    LogLevel file_level = LogLevel::Debug;
    std::filesystem::path log_directory;         // Empty = use default user data dir
    std::string log_filename = "realcraft.log";
    size_t max_file_size = 10 * 1024 * 1024;     // 10 MB
    size_t max_files = 3;                         // Rotating backup count
    bool include_timestamps = true;
    bool colored_output = true;
};

// Static logging interface
class Logger {
public:
    // Initialize/shutdown (call once at startup/exit)
    static void initialize(const LoggerConfig& config = {});
    static void shutdown();
    [[nodiscard]] static bool is_initialized();

    // Category-based level control
    static void set_category_level(std::string_view category, LogLevel level);
    [[nodiscard]] static LogLevel get_category_level(std::string_view category);

    // Global level (default for unconfigured categories)
    static void set_global_level(LogLevel level);
    [[nodiscard]] static LogLevel get_global_level();

    // Force flush to file
    static void flush();

    // Core logging functions
    template<typename... Args>
    static void trace(std::string_view category, fmt::format_string<Args...> fmt, Args&&... args) {
        log_impl(LogLevel::Trace, category, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void debug(std::string_view category, fmt::format_string<Args...> fmt, Args&&... args) {
        log_impl(LogLevel::Debug, category, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void info(std::string_view category, fmt::format_string<Args...> fmt, Args&&... args) {
        log_impl(LogLevel::Info, category, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void warn(std::string_view category, fmt::format_string<Args...> fmt, Args&&... args) {
        log_impl(LogLevel::Warn, category, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void error(std::string_view category, fmt::format_string<Args...> fmt, Args&&... args) {
        log_impl(LogLevel::Error, category, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void critical(std::string_view category, fmt::format_string<Args...> fmt, Args&&... args) {
        log_impl(LogLevel::Critical, category, fmt, std::forward<Args>(args)...);
    }

private:
    Logger() = delete;  // Static-only class

    template<typename... Args>
    static void log_impl(LogLevel level, std::string_view category,
                         fmt::format_string<Args...> fmt, Args&&... args) {
        if (!should_log(level, category)) {
            return;
        }
        auto message = fmt::format(fmt, std::forward<Args>(args)...);
        log_message(level, category, message);
    }

    [[nodiscard]] static bool should_log(LogLevel level, std::string_view category);
    static void log_message(LogLevel level, std::string_view category, std::string_view message);
};

// Pre-defined log categories for consistency
namespace log_category {
    inline constexpr const char* ENGINE = "engine";
    inline constexpr const char* GRAPHICS = "graphics";
    inline constexpr const char* PHYSICS = "physics";
    inline constexpr const char* AUDIO = "audio";
    inline constexpr const char* INPUT = "input";
    inline constexpr const char* WORLD = "world";
    inline constexpr const char* GAME = "game";
    inline constexpr const char* MEMORY = "memory";
    inline constexpr const char* CONFIG = "config";
}  // namespace log_category

}  // namespace realcraft::core

// Convenience logging macros - performance-friendly (check level before formatting)
#define REALCRAFT_LOG_TRACE(category, ...) \
    ::realcraft::core::Logger::trace(category, __VA_ARGS__)

#define REALCRAFT_LOG_DEBUG(category, ...) \
    ::realcraft::core::Logger::debug(category, __VA_ARGS__)

#define REALCRAFT_LOG_INFO(category, ...) \
    ::realcraft::core::Logger::info(category, __VA_ARGS__)

#define REALCRAFT_LOG_WARN(category, ...) \
    ::realcraft::core::Logger::warn(category, __VA_ARGS__)

#define REALCRAFT_LOG_ERROR(category, ...) \
    ::realcraft::core::Logger::error(category, __VA_ARGS__)

#define REALCRAFT_LOG_CRITICAL(category, ...) \
    ::realcraft::core::Logger::critical(category, __VA_ARGS__)
