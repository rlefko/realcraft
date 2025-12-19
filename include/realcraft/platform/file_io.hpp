// RealCraft Platform Abstraction Layer
// file_io.hpp - Cross-platform file system operations

#pragma once

#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace realcraft::platform {

namespace fs = std::filesystem;

// Static utility class for file system operations
class FileSystem {
public:
    // Standard paths
    static fs::path get_executable_directory();
    static fs::path get_user_data_directory();    // ~/Library/Application Support/RealCraft (macOS)
    static fs::path get_user_config_directory();  // Config files
    static fs::path get_user_saves_directory();   // Save games
    static fs::path get_temp_directory();

    // Path utilities
    static fs::path normalize(const fs::path& path);
    static bool is_absolute(const fs::path& path);
    static fs::path make_absolute(const fs::path& path);

    // Synchronous file operations
    static std::optional<std::vector<uint8_t>> read_binary(const fs::path& path);
    static std::optional<std::string> read_text(const fs::path& path);
    static bool write_binary(const fs::path& path, std::span<const uint8_t> data);
    static bool write_text(const fs::path& path, std::string_view content);

    // Asynchronous file operations (returns std::future)
    using BinaryResult = std::optional<std::vector<uint8_t>>;
    using TextResult = std::optional<std::string>;
    using BinaryCallback = std::function<void(BinaryResult)>;
    using TextCallback = std::function<void(TextResult)>;

    static std::future<BinaryResult> read_binary_async(const fs::path& path);
    static std::future<TextResult> read_text_async(const fs::path& path);

    // Directory operations
    static bool create_directories(const fs::path& path);
    static bool exists(const fs::path& path);
    static bool is_directory(const fs::path& path);
    static bool is_file(const fs::path& path);
    static bool remove(const fs::path& path);
    static bool remove_all(const fs::path& path);
    static std::vector<fs::path> list_directory(const fs::path& path, bool recursive = false);
    static std::vector<fs::path> list_files(const fs::path& path, std::string_view extension = "",
                                            bool recursive = false);

    // File info
    static std::optional<size_t> file_size(const fs::path& path);
    static std::optional<fs::file_time_type> last_modified(const fs::path& path);

private:
    FileSystem() = delete;  // Static class, no instances
};

// Resource cache for commonly accessed files
class ResourceCache {
public:
    struct CacheEntry {
        std::vector<uint8_t> data;
        fs::file_time_type modified_time;
        size_t access_count = 0;
    };

    ResourceCache();
    ~ResourceCache();

    // Non-copyable but movable
    ResourceCache(const ResourceCache&) = delete;
    ResourceCache& operator=(const ResourceCache&) = delete;
    ResourceCache(ResourceCache&&) noexcept;
    ResourceCache& operator=(ResourceCache&&) noexcept;

    // Configure cache
    void set_max_size(size_t bytes);
    [[nodiscard]] size_t get_max_size() const;

    // Cache operations
    [[nodiscard]] std::optional<std::span<const uint8_t>> get(const std::string& key) const;
    void put(const std::string& key, std::span<const uint8_t> data);
    void put(const std::string& key, std::vector<uint8_t>&& data);
    void invalidate(const std::string& key);
    void clear();

    // Stats
    [[nodiscard]] size_t current_size() const;
    [[nodiscard]] size_t entry_count() const;

    // Load with automatic caching
    std::optional<std::span<const uint8_t>> load_cached(const std::string& key, const fs::path& source_path,
                                                        bool check_modified = true);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::platform
