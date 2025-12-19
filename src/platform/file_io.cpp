// RealCraft Platform Abstraction Layer
// file_io.cpp - Cross-platform file system implementation

#include <realcraft/platform/file_io.hpp>

#include <spdlog/spdlog.h>
#include <algorithm>
#include <fstream>
#include <future>
#include <mutex>
#include <unordered_map>

#if defined(REALCRAFT_PLATFORM_MACOS)
#include <mach-o/dyld.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined(REALCRAFT_PLATFORM_WINDOWS)
#include <shlobj.h>
#include <windows.h>
#elif defined(REALCRAFT_PLATFORM_LINUX)
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace realcraft::platform {

// FileSystem implementation
fs::path FileSystem::get_executable_directory() {
#if defined(REALCRAFT_PLATFORM_MACOS)
    char path[1024];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        return fs::path(path).parent_path();
    }
    return fs::current_path();
#elif defined(REALCRAFT_PLATFORM_WINDOWS)
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(nullptr, path, MAX_PATH) != 0) {
        return fs::path(path).parent_path();
    }
    return fs::current_path();
#elif defined(REALCRAFT_PLATFORM_LINUX)
    char path[1024];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1) {
        path[len] = '\0';
        return fs::path(path).parent_path();
    }
    return fs::current_path();
#else
    return fs::current_path();
#endif
}

fs::path FileSystem::get_user_data_directory() {
#if defined(REALCRAFT_PLATFORM_MACOS)
    const char* home = getenv("HOME");
    if (home == nullptr) {
        struct passwd* pw = getpwuid(getuid());
        home = pw->pw_dir;
    }
    return fs::path(home) / "Library" / "Application Support" / "RealCraft";
#elif defined(REALCRAFT_PLATFORM_WINDOWS)
    wchar_t* path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path))) {
        fs::path result = fs::path(path) / "RealCraft";
        CoTaskMemFree(path);
        return result;
    }
    return get_executable_directory() / "data";
#elif defined(REALCRAFT_PLATFORM_LINUX)
    const char* xdg_data = getenv("XDG_DATA_HOME");
    if (xdg_data != nullptr) {
        return fs::path(xdg_data) / "RealCraft";
    }
    const char* home = getenv("HOME");
    if (home == nullptr) {
        struct passwd* pw = getpwuid(getuid());
        home = pw->pw_dir;
    }
    return fs::path(home) / ".local" / "share" / "RealCraft";
#else
    return get_executable_directory() / "data";
#endif
}

fs::path FileSystem::get_user_config_directory() {
#if defined(REALCRAFT_PLATFORM_MACOS)
    return get_user_data_directory();  // macOS stores config with app data
#elif defined(REALCRAFT_PLATFORM_WINDOWS)
    return get_user_data_directory() / "config";
#elif defined(REALCRAFT_PLATFORM_LINUX)
    const char* xdg_config = getenv("XDG_CONFIG_HOME");
    if (xdg_config != nullptr) {
        return fs::path(xdg_config) / "RealCraft";
    }
    const char* home = getenv("HOME");
    if (home == nullptr) {
        struct passwd* pw = getpwuid(getuid());
        home = pw->pw_dir;
    }
    return fs::path(home) / ".config" / "RealCraft";
#else
    return get_user_data_directory() / "config";
#endif
}

fs::path FileSystem::get_user_saves_directory() {
    return get_user_data_directory() / "saves";
}

fs::path FileSystem::get_temp_directory() {
    return fs::temp_directory_path() / "RealCraft";
}

fs::path FileSystem::normalize(const fs::path& path) {
    try {
        return fs::weakly_canonical(path);
    } catch (const std::exception& e) {
        spdlog::warn("Failed to normalize path '{}': {}", path.string(), e.what());
        return path;
    }
}

bool FileSystem::is_absolute(const fs::path& path) {
    return path.is_absolute();
}

fs::path FileSystem::make_absolute(const fs::path& path) {
    if (path.is_absolute()) {
        return path;
    }
    return fs::absolute(path);
}

std::optional<std::vector<uint8_t>> FileSystem::read_binary(const fs::path& path) {
    try {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            spdlog::warn("Failed to open file for reading: {}", path.string());
            return std::nullopt;
        }

        auto size = file.tellg();
        if (size < 0) {
            return std::nullopt;
        }

        std::vector<uint8_t> data(static_cast<size_t>(size));
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(data.data()), size);

        if (!file) {
            spdlog::warn("Error reading file: {}", path.string());
            return std::nullopt;
        }

        return data;
    } catch (const std::exception& e) {
        spdlog::error("Exception reading file '{}': {}", path.string(), e.what());
        return std::nullopt;
    }
}

std::optional<std::string> FileSystem::read_text(const fs::path& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            spdlog::warn("Failed to open file for reading: {}", path.string());
            return std::nullopt;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

        if (!file && !file.eof()) {
            spdlog::warn("Error reading file: {}", path.string());
            return std::nullopt;
        }

        return content;
    } catch (const std::exception& e) {
        spdlog::error("Exception reading file '{}': {}", path.string(), e.what());
        return std::nullopt;
    }
}

bool FileSystem::write_binary(const fs::path& path, std::span<const uint8_t> data) {
    try {
        // Ensure parent directory exists
        if (path.has_parent_path()) {
            create_directories(path.parent_path());
        }

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            spdlog::warn("Failed to open file for writing: {}", path.string());
            return false;
        }

        file.write(reinterpret_cast<const char*>(data.data()),
                   static_cast<std::streamsize>(data.size()));

        if (!file) {
            spdlog::warn("Error writing file: {}", path.string());
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Exception writing file '{}': {}", path.string(), e.what());
        return false;
    }
}

bool FileSystem::write_text(const fs::path& path, std::string_view content) {
    try {
        // Ensure parent directory exists
        if (path.has_parent_path()) {
            create_directories(path.parent_path());
        }

        std::ofstream file(path, std::ios::trunc);
        if (!file.is_open()) {
            spdlog::warn("Failed to open file for writing: {}", path.string());
            return false;
        }

        file << content;

        if (!file) {
            spdlog::warn("Error writing file: {}", path.string());
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Exception writing file '{}': {}", path.string(), e.what());
        return false;
    }
}

std::future<FileSystem::BinaryResult> FileSystem::read_binary_async(const fs::path& path) {
    return std::async(std::launch::async, [path]() { return read_binary(path); });
}

std::future<FileSystem::TextResult> FileSystem::read_text_async(const fs::path& path) {
    return std::async(std::launch::async, [path]() { return read_text(path); });
}

bool FileSystem::create_directories(const fs::path& path) {
    try {
        return fs::create_directories(path);
    } catch (const std::exception& e) {
        spdlog::error("Failed to create directories '{}': {}", path.string(), e.what());
        return false;
    }
}

bool FileSystem::exists(const fs::path& path) {
    try {
        return fs::exists(path);
    } catch (const std::exception& e) {
        spdlog::warn("Error checking existence of '{}': {}", path.string(), e.what());
        return false;
    }
}

bool FileSystem::is_directory(const fs::path& path) {
    try {
        return fs::is_directory(path);
    } catch (const std::exception& e) {
        spdlog::warn("Error checking if '{}' is directory: {}", path.string(), e.what());
        return false;
    }
}

bool FileSystem::is_file(const fs::path& path) {
    try {
        return fs::is_regular_file(path);
    } catch (const std::exception& e) {
        spdlog::warn("Error checking if '{}' is file: {}", path.string(), e.what());
        return false;
    }
}

bool FileSystem::remove(const fs::path& path) {
    try {
        return fs::remove(path);
    } catch (const std::exception& e) {
        spdlog::error("Failed to remove '{}': {}", path.string(), e.what());
        return false;
    }
}

bool FileSystem::remove_all(const fs::path& path) {
    try {
        fs::remove_all(path);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to remove all '{}': {}", path.string(), e.what());
        return false;
    }
}

std::vector<fs::path> FileSystem::list_directory(const fs::path& path, bool recursive) {
    std::vector<fs::path> result;
    try {
        if (!fs::exists(path) || !fs::is_directory(path)) {
            return result;
        }

        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(path)) {
                result.push_back(entry.path());
            }
        } else {
            for (const auto& entry : fs::directory_iterator(path)) {
                result.push_back(entry.path());
            }
        }
    } catch (const std::exception& e) {
        spdlog::warn("Error listing directory '{}': {}", path.string(), e.what());
    }
    return result;
}

std::vector<fs::path> FileSystem::list_files(const fs::path& path,
                                             std::string_view extension,
                                             bool recursive) {
    std::vector<fs::path> result;
    try {
        if (!fs::exists(path) || !fs::is_directory(path)) {
            return result;
        }

        auto process_entry = [&](const fs::directory_entry& entry) {
            if (!entry.is_regular_file()) {
                return;
            }
            if (extension.empty() || entry.path().extension() == extension) {
                result.push_back(entry.path());
            }
        };

        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(path)) {
                process_entry(entry);
            }
        } else {
            for (const auto& entry : fs::directory_iterator(path)) {
                process_entry(entry);
            }
        }
    } catch (const std::exception& e) {
        spdlog::warn("Error listing files in '{}': {}", path.string(), e.what());
    }
    return result;
}

std::optional<size_t> FileSystem::file_size(const fs::path& path) {
    try {
        if (!fs::exists(path) || !fs::is_regular_file(path)) {
            return std::nullopt;
        }
        return static_cast<size_t>(fs::file_size(path));
    } catch (const std::exception& e) {
        spdlog::warn("Error getting size of '{}': {}", path.string(), e.what());
        return std::nullopt;
    }
}

std::optional<fs::file_time_type> FileSystem::last_modified(const fs::path& path) {
    try {
        if (!fs::exists(path)) {
            return std::nullopt;
        }
        return fs::last_write_time(path);
    } catch (const std::exception& e) {
        spdlog::warn("Error getting last modified time of '{}': {}", path.string(), e.what());
        return std::nullopt;
    }
}

// ResourceCache implementation
struct ResourceCache::Impl {
    std::unordered_map<std::string, CacheEntry> entries;
    std::mutex mutex;
    size_t max_size = 64 * 1024 * 1024;  // 64 MB default
    size_t current_size = 0;
};

ResourceCache::ResourceCache() : impl_(std::make_unique<Impl>()) {}

ResourceCache::~ResourceCache() = default;

ResourceCache::ResourceCache(ResourceCache&&) noexcept = default;

ResourceCache& ResourceCache::operator=(ResourceCache&&) noexcept = default;

void ResourceCache::set_max_size(size_t bytes) {
    std::lock_guard lock(impl_->mutex);
    impl_->max_size = bytes;
}

size_t ResourceCache::get_max_size() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->max_size;
}

std::optional<std::span<const uint8_t>> ResourceCache::get(const std::string& key) const {
    std::lock_guard lock(impl_->mutex);
    auto it = impl_->entries.find(key);
    if (it != impl_->entries.end()) {
        ++it->second.access_count;
        return std::span<const uint8_t>(it->second.data);
    }
    return std::nullopt;
}

void ResourceCache::put(const std::string& key, std::span<const uint8_t> data) {
    std::vector<uint8_t> copy(data.begin(), data.end());
    put(key, std::move(copy));
}

void ResourceCache::put(const std::string& key, std::vector<uint8_t>&& data) {
    std::lock_guard lock(impl_->mutex);

    // Remove existing entry if present
    auto it = impl_->entries.find(key);
    if (it != impl_->entries.end()) {
        impl_->current_size -= it->second.data.size();
        impl_->entries.erase(it);
    }

    // Check if we need to evict entries
    while (impl_->current_size + data.size() > impl_->max_size && !impl_->entries.empty()) {
        // Simple LRU: find entry with lowest access count
        auto min_it = impl_->entries.begin();
        for (auto curr = impl_->entries.begin(); curr != impl_->entries.end(); ++curr) {
            if (curr->second.access_count < min_it->second.access_count) {
                min_it = curr;
            }
        }
        impl_->current_size -= min_it->second.data.size();
        impl_->entries.erase(min_it);
    }

    // Insert new entry
    CacheEntry entry;
    entry.data = std::move(data);
    entry.modified_time = fs::file_time_type::clock::now();
    entry.access_count = 1;

    impl_->current_size += entry.data.size();
    impl_->entries[key] = std::move(entry);
}

void ResourceCache::invalidate(const std::string& key) {
    std::lock_guard lock(impl_->mutex);
    auto it = impl_->entries.find(key);
    if (it != impl_->entries.end()) {
        impl_->current_size -= it->second.data.size();
        impl_->entries.erase(it);
    }
}

void ResourceCache::clear() {
    std::lock_guard lock(impl_->mutex);
    impl_->entries.clear();
    impl_->current_size = 0;
}

size_t ResourceCache::current_size() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->current_size;
}

size_t ResourceCache::entry_count() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->entries.size();
}

std::optional<std::span<const uint8_t>> ResourceCache::load_cached(const std::string& key,
                                                                   const fs::path& source_path,
                                                                   bool check_modified) {
    // Check if already cached
    {
        std::lock_guard lock(impl_->mutex);
        auto it = impl_->entries.find(key);
        if (it != impl_->entries.end()) {
            if (check_modified) {
                auto file_time = FileSystem::last_modified(source_path);
                if (file_time && *file_time > it->second.modified_time) {
                    // File was modified, invalidate cache
                    impl_->current_size -= it->second.data.size();
                    impl_->entries.erase(it);
                } else {
                    ++it->second.access_count;
                    return std::span<const uint8_t>(it->second.data);
                }
            } else {
                ++it->second.access_count;
                return std::span<const uint8_t>(it->second.data);
            }
        }
    }

    // Load from disk
    auto data = FileSystem::read_binary(source_path);
    if (!data) {
        return std::nullopt;
    }

    // Cache and return
    put(key, std::move(*data));
    return get(key);
}

}  // namespace realcraft::platform
