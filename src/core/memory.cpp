// RealCraft Engine Core
// memory.cpp - Memory tracking and allocators implementation

#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <realcraft/core/logger.hpp>
#include <realcraft/core/memory.hpp>
#include <unordered_map>

namespace realcraft::core {

namespace {

// Memory tracker state
struct TrackerState {
    bool initialized = false;
    MemoryStats stats;
    std::mutex mutex;

#ifdef REALCRAFT_DEBUG
    struct AllocationInfo {
        size_t size;
        const char* tag;
    };
    std::unordered_map<void*, AllocationInfo> allocations;
#endif
};

TrackerState& get_tracker_state() {
    static TrackerState state;
    return state;
}

// Frame allocator singleton
std::unique_ptr<LinearAllocator> g_frame_allocator;
std::mutex g_frame_allocator_mutex;

}  // namespace

// MemoryTracker implementation
void MemoryTracker::initialize() {
    auto& state = get_tracker_state();
    std::lock_guard lock(state.mutex);

    if (state.initialized) {
        return;
    }

    state.stats = MemoryStats{};
    state.initialized = true;

#ifdef REALCRAFT_DEBUG
    state.allocations.clear();
#endif
}

void MemoryTracker::shutdown() {
    auto& state = get_tracker_state();
    std::lock_guard lock(state.mutex);

    if (!state.initialized) {
        return;
    }

#ifdef REALCRAFT_DEBUG
    // Report any remaining allocations as leaks
    if (!state.allocations.empty()) {
        report_leaks();
    }
    state.allocations.clear();
#endif

    state.initialized = false;
}

bool MemoryTracker::is_initialized() {
    auto& state = get_tracker_state();
    std::lock_guard lock(state.mutex);
    return state.initialized;
}

MemoryStats MemoryTracker::get_stats() {
    auto& state = get_tracker_state();
    std::lock_guard lock(state.mutex);
    return state.stats;
}

void MemoryTracker::reset_stats() {
    auto& state = get_tracker_state();
    std::lock_guard lock(state.mutex);
    state.stats = MemoryStats{};
}

void MemoryTracker::log_stats() {
    auto stats = get_stats();

    REALCRAFT_LOG_INFO(log_category::MEMORY, "Memory Stats - Active: {} allocs ({} bytes), Peak: {} allocs ({} bytes)",
                       stats.active_allocations, stats.active_bytes, stats.peak_allocations, stats.peak_bytes);
}

void MemoryTracker::track_allocation(void* ptr, size_t size, const char* tag) {
    if (!ptr)
        return;

    auto& state = get_tracker_state();
    std::lock_guard lock(state.mutex);

    if (!state.initialized) {
        return;
    }

    state.stats.total_allocations++;
    state.stats.active_allocations++;
    state.stats.active_bytes += size;
    state.stats.peak_allocations = std::max(state.stats.peak_allocations, state.stats.active_allocations);
    state.stats.peak_bytes = std::max(state.stats.peak_bytes, state.stats.active_bytes);

#ifdef REALCRAFT_DEBUG
    state.allocations[ptr] = {size, tag};
#else
    (void)tag;
#endif
}

void MemoryTracker::track_deallocation(void* ptr) {
    if (!ptr)
        return;

    auto& state = get_tracker_state();
    std::lock_guard lock(state.mutex);

    if (!state.initialized) {
        return;
    }

#ifdef REALCRAFT_DEBUG
    auto it = state.allocations.find(ptr);
    if (it != state.allocations.end()) {
        state.stats.total_deallocations++;
        state.stats.active_allocations--;
        state.stats.active_bytes -= it->second.size;
        state.allocations.erase(it);
    }
#else
    state.stats.total_deallocations++;
    if (state.stats.active_allocations > 0) {
        state.stats.active_allocations--;
    }
#endif
}

void MemoryTracker::report_leaks() {
#ifdef REALCRAFT_DEBUG
    auto& state = get_tracker_state();
    // Note: Called from shutdown, mutex should already be held

    if (state.allocations.empty()) {
        REALCRAFT_LOG_INFO(log_category::MEMORY, "No memory leaks detected");
        return;
    }

    REALCRAFT_LOG_WARN(log_category::MEMORY, "Memory leaks detected: {} allocations ({} bytes)",
                       state.allocations.size(), state.stats.active_bytes);

    size_t count = 0;
    for (const auto& [ptr, info] : state.allocations) {
        REALCRAFT_LOG_WARN(log_category::MEMORY, "  Leak: {} bytes at {} (tag: {})", info.size, ptr,
                           info.tag ? info.tag : "none");

        if (++count >= 10) {
            REALCRAFT_LOG_WARN(log_category::MEMORY, "  ... and {} more leaks", state.allocations.size() - 10);
            break;
        }
    }
#endif
}

// LinearAllocator implementation
struct LinearAllocator::Impl {
    uint8_t* buffer = nullptr;
    size_t capacity = 0;
    size_t offset = 0;
};

LinearAllocator::LinearAllocator(size_t capacity) : impl_(std::make_unique<Impl>()) {
    impl_->buffer = static_cast<uint8_t*>(std::malloc(capacity));
    impl_->capacity = capacity;
    impl_->offset = 0;

    if (!impl_->buffer) {
        REALCRAFT_LOG_ERROR(log_category::MEMORY, "Failed to allocate {} bytes for LinearAllocator", capacity);
    }
}

LinearAllocator::~LinearAllocator() {
    if (impl_->buffer) {
        std::free(impl_->buffer);
    }
}

void* LinearAllocator::allocate(size_t size, size_t alignment) {
    if (!impl_->buffer || size == 0) {
        return nullptr;
    }

    // Align the current offset
    uintptr_t current = reinterpret_cast<uintptr_t>(impl_->buffer + impl_->offset);
    uintptr_t aligned = (current + alignment - 1) & ~(alignment - 1);
    size_t padding = aligned - current;

    if (impl_->offset + padding + size > impl_->capacity) {
        REALCRAFT_LOG_ERROR(log_category::MEMORY,
                            "LinearAllocator out of memory: requested {} bytes, remaining {} bytes", size, remaining());
        return nullptr;
    }

    impl_->offset += padding + size;
    return reinterpret_cast<void*>(aligned);
}

void LinearAllocator::reset() {
    impl_->offset = 0;
}

size_t LinearAllocator::capacity() const {
    return impl_->capacity;
}

size_t LinearAllocator::used() const {
    return impl_->offset;
}

size_t LinearAllocator::remaining() const {
    return impl_->capacity - impl_->offset;
}

// Frame allocator singleton
LinearAllocator& get_frame_allocator() {
    std::lock_guard lock(g_frame_allocator_mutex);

    if (!g_frame_allocator) {
        // Auto-initialize with default size if not explicitly initialized
        g_frame_allocator = std::make_unique<LinearAllocator>(4 * 1024 * 1024);
    }

    return *g_frame_allocator;
}

void initialize_frame_allocator(size_t capacity) {
    std::lock_guard lock(g_frame_allocator_mutex);

    if (g_frame_allocator) {
        REALCRAFT_LOG_WARN(log_category::MEMORY, "Frame allocator already initialized, ignoring new capacity");
        return;
    }

    g_frame_allocator = std::make_unique<LinearAllocator>(capacity);
    REALCRAFT_LOG_INFO(log_category::MEMORY, "Frame allocator initialized with {} bytes", capacity);
}

void shutdown_frame_allocator() {
    std::lock_guard lock(g_frame_allocator_mutex);
    g_frame_allocator.reset();
}

void reset_frame_allocator() {
    std::lock_guard lock(g_frame_allocator_mutex);

    if (g_frame_allocator) {
        g_frame_allocator->reset();
    }
}

}  // namespace realcraft::core
