// RealCraft Engine Core
// logger.cpp - Logging system implementation

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <mutex>
#include <realcraft/core/logger.hpp>
#include <realcraft/platform/file_io.hpp>
#include <unordered_map>

namespace realcraft::core {

namespace {

// Global logger state
struct LoggerState {
    bool initialized = false;
    LogLevel global_level = LogLevel::Info;
    std::unordered_map<std::string, LogLevel> category_levels;
    std::shared_ptr<spdlog::logger> console_logger;
    std::shared_ptr<spdlog::logger> file_logger;
    std::mutex mutex;
};

LoggerState& get_state() {
    static LoggerState state;
    return state;
}

spdlog::level::level_enum to_spdlog_level(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:
            return spdlog::level::trace;
        case LogLevel::Debug:
            return spdlog::level::debug;
        case LogLevel::Info:
            return spdlog::level::info;
        case LogLevel::Warn:
            return spdlog::level::warn;
        case LogLevel::Error:
            return spdlog::level::err;
        case LogLevel::Critical:
            return spdlog::level::critical;
        case LogLevel::Off:
            return spdlog::level::off;
        default:
            return spdlog::level::info;
    }
}

}  // namespace

void Logger::initialize(const LoggerConfig& config) {
    auto& state = get_state();
    std::filesystem::path log_path;  // Store for logging after lock release

    {
        std::lock_guard lock(state.mutex);

        if (state.initialized) {
            return;
        }

        try {
            // Create console sink with colors
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(to_spdlog_level(config.console_level));

            if (config.include_timestamps) {
                console_sink->set_pattern("[%H:%M:%S] [%^%l%$] [%n] %v");
            } else {
                console_sink->set_pattern("[%^%l%$] [%n] %v");
            }

            state.console_logger = std::make_shared<spdlog::logger>("console", console_sink);
            state.console_logger->set_level(spdlog::level::trace);  // Let sink filter
            state.console_logger->flush_on(spdlog::level::info);    // Flush on every info message

            // Create file sink if log directory is specified or use default
            std::filesystem::path log_dir = config.log_directory;
            if (log_dir.empty()) {
                log_dir = platform::FileSystem::get_user_data_directory() / "logs";
            }

            // Ensure log directory exists
            if (!platform::FileSystem::exists(log_dir)) {
                platform::FileSystem::create_directories(log_dir);
            }

            log_path = log_dir / config.log_filename;
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                log_path.string(), config.max_file_size, config.max_files);
            file_sink->set_level(to_spdlog_level(config.file_level));
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");

            state.file_logger = std::make_shared<spdlog::logger>("file", file_sink);
            state.file_logger->set_level(spdlog::level::trace);  // Let sink filter
            state.file_logger->flush_on(spdlog::level::info);    // Flush on every info message

            state.global_level = config.console_level;
            state.initialized = true;

        } catch (const spdlog::spdlog_ex& ex) {
            // Fallback to basic console logging if file creation fails
            spdlog::error("Logger initialization failed: {}", ex.what());

            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            state.console_logger = std::make_shared<spdlog::logger>("console", console_sink);
            state.global_level = config.console_level;
            state.initialized = true;
        }
    }  // Lock released here

    // Log initialization success AFTER releasing the lock to avoid deadlock
    info(log_category::ENGINE, "Logger initialized");
    if (!log_path.empty()) {
        info(log_category::ENGINE, "Log file: {}", log_path.string());
    }
}

void Logger::shutdown() {
    auto& state = get_state();
    std::lock_guard lock(state.mutex);

    if (!state.initialized) {
        return;
    }

    // Flush all pending logs
    if (state.console_logger) {
        state.console_logger->flush();
    }
    if (state.file_logger) {
        state.file_logger->flush();
    }

    state.console_logger.reset();
    state.file_logger.reset();
    state.category_levels.clear();
    state.initialized = false;

    spdlog::shutdown();
}

bool Logger::is_initialized() {
    auto& state = get_state();
    std::lock_guard lock(state.mutex);
    return state.initialized;
}

void Logger::set_category_level(std::string_view category, LogLevel level) {
    auto& state = get_state();
    std::lock_guard lock(state.mutex);
    state.category_levels[std::string(category)] = level;
}

LogLevel Logger::get_category_level(std::string_view category) {
    auto& state = get_state();
    std::lock_guard lock(state.mutex);

    auto it = state.category_levels.find(std::string(category));
    if (it != state.category_levels.end()) {
        return it->second;
    }
    return state.global_level;
}

void Logger::set_global_level(LogLevel level) {
    auto& state = get_state();
    std::lock_guard lock(state.mutex);
    state.global_level = level;

    if (state.console_logger) {
        // Update console sink level
        for (auto& sink : state.console_logger->sinks()) {
            sink->set_level(to_spdlog_level(level));
        }
    }
}

LogLevel Logger::get_global_level() {
    auto& state = get_state();
    std::lock_guard lock(state.mutex);
    return state.global_level;
}

void Logger::flush() {
    auto& state = get_state();
    std::lock_guard lock(state.mutex);

    if (state.console_logger) {
        state.console_logger->flush();
    }
    if (state.file_logger) {
        state.file_logger->flush();
    }
}

bool Logger::should_log(LogLevel level, std::string_view category) {
    auto& state = get_state();

    if (!state.initialized) {
        // Before initialization, use spdlog default
        return true;
    }

    LogLevel category_level;
    {
        std::lock_guard lock(state.mutex);
        auto it = state.category_levels.find(std::string(category));
        if (it != state.category_levels.end()) {
            category_level = it->second;
        } else {
            category_level = state.global_level;
        }
    }

    return static_cast<int>(level) >= static_cast<int>(category_level);
}

void Logger::log_message(LogLevel level, std::string_view category, std::string_view message) {
    auto& state = get_state();

    if (!state.initialized) {
        // Before initialization, use spdlog directly
        spdlog::log(to_spdlog_level(level), "[{}] {}", category, message);
        return;
    }

    auto spdlog_level = to_spdlog_level(level);
    std::string formatted_message = fmt::format("[{}] {}", category, message);

    std::lock_guard lock(state.mutex);

    if (state.console_logger) {
        state.console_logger->log(spdlog_level, "{}", formatted_message);
    }
    if (state.file_logger) {
        state.file_logger->log(spdlog_level, "{}", formatted_message);
    }
}

}  // namespace realcraft::core
