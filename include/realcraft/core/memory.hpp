// RealCraft Engine Core
// memory.hpp - Memory tracking and allocators

#pragma once

#include <cstddef>
#include <memory>

namespace realcraft::core {

// Memory statistics (debug builds only)
struct MemoryStats {
    size_t total_allocations = 0;
    size_t total_deallocations = 0;
    size_t active_allocations = 0;
    size_t active_bytes = 0;
    size_t peak_allocations = 0;
    size_t peak_bytes = 0;
};

// Static memory tracking interface
class MemoryTracker {
public:
    // Initialize/shutdown
    static void initialize();
    static void shutdown();
    [[nodiscard]] static bool is_initialized();

    // Statistics
    [[nodiscard]] static MemoryStats get_stats();
    static void reset_stats();
    static void log_stats();

    // Manual tracking (used by custom allocators)
    static void track_allocation(void* ptr, size_t size, const char* tag = nullptr);
    static void track_deallocation(void* ptr);

    // Leak detection (call at shutdown)
    static void report_leaks();

private:
    MemoryTracker() = delete;  // Static-only class
};

// Linear (bump) allocator for per-frame temporary data
class LinearAllocator {
public:
    explicit LinearAllocator(size_t capacity);
    ~LinearAllocator();

    // Non-copyable, non-movable
    LinearAllocator(const LinearAllocator&) = delete;
    LinearAllocator& operator=(const LinearAllocator&) = delete;
    LinearAllocator(LinearAllocator&&) = delete;
    LinearAllocator& operator=(LinearAllocator&&) = delete;

    // Allocate memory (no individual free)
    [[nodiscard]] void* allocate(size_t size, size_t alignment = alignof(std::max_align_t));

    // Typed allocation with construction
    template<typename T, typename... Args>
    [[nodiscard]] T* create(Args&&... args) {
        void* ptr = allocate(sizeof(T), alignof(T));
        if (ptr) {
            return new (ptr) T(std::forward<Args>(args)...);
        }
        return nullptr;
    }

    // Allocate array (no construction)
    template<typename T>
    [[nodiscard]] T* allocate_array(size_t count) {
        void* ptr = allocate(sizeof(T) * count, alignof(T));
        return static_cast<T*>(ptr);
    }

    // Reset all allocations (call at start of frame)
    void reset();

    // Stats
    [[nodiscard]] size_t capacity() const;
    [[nodiscard]] size_t used() const;
    [[nodiscard]] size_t remaining() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Per-frame allocator singleton access
LinearAllocator& get_frame_allocator();
void initialize_frame_allocator(size_t capacity = 4 * 1024 * 1024);  // 4 MB default
void shutdown_frame_allocator();
void reset_frame_allocator();  // Call at start of each frame

}  // namespace realcraft::core

// Debug allocation macros (track allocations in debug builds)
#ifdef REALCRAFT_DEBUG
    #define REALCRAFT_TRACK_ALLOC(ptr, size, tag) \
        ::realcraft::core::MemoryTracker::track_allocation(ptr, size, tag)
    #define REALCRAFT_TRACK_FREE(ptr) \
        ::realcraft::core::MemoryTracker::track_deallocation(ptr)
#else
    #define REALCRAFT_TRACK_ALLOC(ptr, size, tag) ((void)0)
    #define REALCRAFT_TRACK_FREE(ptr) ((void)0)
#endif
