// RealCraft Platform Abstraction Layer
// window.cpp - GLFW-based window implementation

#include <realcraft/platform/window.hpp>

#include <GLFW/glfw3.h>

// Native window handles require platform-specific headers
#if defined(REALCRAFT_PLATFORM_MACOS)
#define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(REALCRAFT_PLATFORM_WINDOWS)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(REALCRAFT_PLATFORM_LINUX)
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>

#include <spdlog/spdlog.h>

#if defined(REALCRAFT_PLATFORM_MACOS)
// Forward declaration for macOS-specific Metal layer setup
extern "C" void* realcraft_setup_metal_layer(void* native_window);
#endif

namespace realcraft::platform {

namespace {
bool g_platform_initialized = false;
int g_window_count = 0;
}  // namespace

bool initialize() {
    if (g_platform_initialized) {
        return true;
    }

    if (glfwInit() == GLFW_FALSE) {
        spdlog::error("Failed to initialize GLFW");
        return false;
    }

    // Set error callback
    glfwSetErrorCallback([](int error, const char* description) {
        spdlog::error("GLFW Error {}: {}", error, description);
    });

    g_platform_initialized = true;
    spdlog::info("Platform layer initialized (GLFW {})", glfwGetVersionString());
    return true;
}

void shutdown() {
    if (!g_platform_initialized) {
        return;
    }

    glfwTerminate();
    g_platform_initialized = false;
    spdlog::info("Platform layer shut down");
}

bool is_initialized() {
    return g_platform_initialized;
}

struct Window::Impl {
    GLFWwindow* window = nullptr;
    GLFWmonitor* fullscreen_monitor = nullptr;
    Input input;
    std::string title;
    bool is_fullscreen = false;
    bool vsync_enabled = true;

    // Windowed mode state (for restoring from fullscreen)
    int windowed_x = 0;
    int windowed_y = 0;
    int windowed_width = 0;
    int windowed_height = 0;

    // Metal layer (macOS only)
    void* metal_layer = nullptr;

    // Callbacks
    ResizeCallback resize_callback;
    FramebufferResizeCallback framebuffer_resize_callback;
    CloseCallback close_callback;
    FocusCallback focus_callback;
};

Window::Window() : impl_(std::make_unique<Impl>()) {}

Window::~Window() {
    shutdown();
}

Window::Window(Window&&) noexcept = default;

Window& Window::operator=(Window&&) noexcept = default;

bool Window::initialize(const Config& config) {
    if (!is_initialized()) {
        if (!platform::initialize()) {
            return false;
        }
    }

    // Request no graphics API - we'll use Metal/Vulkan/DX12 directly
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);

#if defined(REALCRAFT_PLATFORM_MACOS)
    // macOS Retina support
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
#endif

    GLFWmonitor* monitor = nullptr;
    int width = static_cast<int>(config.width);
    int height = static_cast<int>(config.height);

    if (config.fullscreen) {
        // Get the monitor
        if (config.monitor_index >= 0) {
            int count = 0;
            GLFWmonitor** monitors = glfwGetMonitors(&count);
            if (config.monitor_index < count) {
                monitor = monitors[config.monitor_index];
            }
        }
        if (monitor == nullptr) {
            monitor = glfwGetPrimaryMonitor();
        }

        // Use native resolution for fullscreen
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        width = mode->width;
        height = mode->height;
        impl_->fullscreen_monitor = monitor;
    }

    // Create window
    impl_->window = glfwCreateWindow(width, height, config.title.c_str(), monitor, nullptr);
    if (impl_->window == nullptr) {
        spdlog::error("Failed to create GLFW window");
        return false;
    }

    impl_->title = config.title;
    impl_->is_fullscreen = config.fullscreen;
    impl_->vsync_enabled = config.vsync;

    // Store initial windowed position for fullscreen toggle
    if (!config.fullscreen) {
        glfwGetWindowPos(impl_->window, &impl_->windowed_x, &impl_->windowed_y);
        impl_->windowed_width = width;
        impl_->windowed_height = height;
    }

    // Set up input
    impl_->input.set_glfw_window(impl_->window);

    // Store this pointer in window for callbacks
    glfwSetWindowUserPointer(impl_->window, this);

    // Set up callbacks
    glfwSetKeyCallback(impl_->window, [](GLFWwindow* win, int key, int scancode, int action,
                                         int mods) {
        auto* window = static_cast<Window*>(glfwGetWindowUserPointer(win));
        window->impl_->input.on_key_event(key, scancode, action, mods);
    });

    glfwSetCharCallback(impl_->window, [](GLFWwindow* win, unsigned int codepoint) {
        auto* window = static_cast<Window*>(glfwGetWindowUserPointer(win));
        window->impl_->input.on_char_event(codepoint);
    });

    glfwSetCursorPosCallback(impl_->window, [](GLFWwindow* win, double x, double y) {
        auto* window = static_cast<Window*>(glfwGetWindowUserPointer(win));
        window->impl_->input.on_mouse_move_event(x, y);
    });

    glfwSetMouseButtonCallback(impl_->window, [](GLFWwindow* win, int button, int action,
                                                  int mods) {
        auto* window = static_cast<Window*>(glfwGetWindowUserPointer(win));
        window->impl_->input.on_mouse_button_event(button, action, mods);
    });

    glfwSetScrollCallback(impl_->window, [](GLFWwindow* win, double x, double y) {
        auto* window = static_cast<Window*>(glfwGetWindowUserPointer(win));
        window->impl_->input.on_scroll_event(x, y);
    });

    glfwSetWindowSizeCallback(impl_->window, [](GLFWwindow* win, int w, int h) {
        auto* window = static_cast<Window*>(glfwGetWindowUserPointer(win));
        if (window->impl_->resize_callback) {
            window->impl_->resize_callback(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
        }
    });

    glfwSetFramebufferSizeCallback(impl_->window, [](GLFWwindow* win, int w, int h) {
        auto* window = static_cast<Window*>(glfwGetWindowUserPointer(win));
        if (window->impl_->framebuffer_resize_callback) {
            window->impl_->framebuffer_resize_callback(static_cast<uint32_t>(w),
                                                       static_cast<uint32_t>(h));
        }
    });

    glfwSetWindowCloseCallback(impl_->window, [](GLFWwindow* win) {
        auto* window = static_cast<Window*>(glfwGetWindowUserPointer(win));
        if (window->impl_->close_callback) {
            window->impl_->close_callback();
        }
    });

    glfwSetWindowFocusCallback(impl_->window, [](GLFWwindow* win, int focused) {
        auto* window = static_cast<Window*>(glfwGetWindowUserPointer(win));
        if (window->impl_->focus_callback) {
            window->impl_->focus_callback(focused == GLFW_TRUE);
        }
    });

#if defined(REALCRAFT_PLATFORM_MACOS)
    // Set up Metal layer
    void* native_window = get_native_window();
    if (native_window != nullptr) {
        impl_->metal_layer = realcraft_setup_metal_layer(native_window);
    }
#endif

    ++g_window_count;
    spdlog::info("Window created: {}x{} ({})", width, height,
                 config.fullscreen ? "fullscreen" : "windowed");

    return true;
}

void Window::shutdown() {
    if (impl_->window != nullptr) {
        glfwDestroyWindow(impl_->window);
        impl_->window = nullptr;
        --g_window_count;
    }
}

bool Window::is_initialized() const {
    return impl_->window != nullptr;
}

bool Window::should_close() const {
    if (impl_->window == nullptr) {
        return true;
    }
    return glfwWindowShouldClose(impl_->window) == GLFW_TRUE;
}

void Window::set_should_close(bool close) {
    if (impl_->window != nullptr) {
        glfwSetWindowShouldClose(impl_->window, close ? GLFW_TRUE : GLFW_FALSE);
    }
}

void Window::poll_events() {
    glfwPollEvents();
}

void Window::set_title(std::string_view title) {
    impl_->title = std::string(title);
    if (impl_->window != nullptr) {
        glfwSetWindowTitle(impl_->window, impl_->title.c_str());
    }
}

std::string Window::get_title() const {
    return impl_->title;
}

glm::uvec2 Window::get_size() const {
    if (impl_->window == nullptr) {
        return {0, 0};
    }
    int w = 0;
    int h = 0;
    glfwGetWindowSize(impl_->window, &w, &h);
    return {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
}

glm::uvec2 Window::get_framebuffer_size() const {
    if (impl_->window == nullptr) {
        return {0, 0};
    }
    int w = 0;
    int h = 0;
    glfwGetFramebufferSize(impl_->window, &w, &h);
    return {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
}

void Window::set_size(uint32_t width, uint32_t height) {
    if (impl_->window != nullptr && !impl_->is_fullscreen) {
        glfwSetWindowSize(impl_->window, static_cast<int>(width), static_cast<int>(height));
    }
}

glm::ivec2 Window::get_position() const {
    if (impl_->window == nullptr) {
        return {0, 0};
    }
    int x = 0;
    int y = 0;
    glfwGetWindowPos(impl_->window, &x, &y);
    return {x, y};
}

void Window::set_position(int32_t x, int32_t y) {
    if (impl_->window != nullptr && !impl_->is_fullscreen) {
        glfwSetWindowPos(impl_->window, x, y);
    }
}

void Window::set_fullscreen(bool fullscreen, int32_t monitor_index) {
    if (impl_->window == nullptr) {
        return;
    }

    if (fullscreen == impl_->is_fullscreen) {
        return;
    }

    if (fullscreen) {
        // Save windowed position
        glfwGetWindowPos(impl_->window, &impl_->windowed_x, &impl_->windowed_y);
        glfwGetWindowSize(impl_->window, &impl_->windowed_width, &impl_->windowed_height);

        // Get monitor
        GLFWmonitor* monitor = nullptr;
        if (monitor_index >= 0) {
            int count = 0;
            GLFWmonitor** monitors = glfwGetMonitors(&count);
            if (monitor_index < count) {
                monitor = monitors[monitor_index];
            }
        }
        if (monitor == nullptr) {
            monitor = glfwGetPrimaryMonitor();
        }

        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(impl_->window, monitor, 0, 0, mode->width, mode->height,
                             mode->refreshRate);
        impl_->fullscreen_monitor = monitor;
    } else {
        // Restore windowed position
        glfwSetWindowMonitor(impl_->window, nullptr, impl_->windowed_x, impl_->windowed_y,
                             impl_->windowed_width, impl_->windowed_height, 0);
        impl_->fullscreen_monitor = nullptr;
    }

    impl_->is_fullscreen = fullscreen;
}

bool Window::is_fullscreen() const {
    return impl_->is_fullscreen;
}

void Window::set_vsync([[maybe_unused]] bool enabled) {
    impl_->vsync_enabled = enabled;
    // Note: VSync is typically handled by the graphics API (Metal/Vulkan)
    // GLFW swap interval only applies to OpenGL contexts
}

bool Window::is_vsync_enabled() const {
    return impl_->vsync_enabled;
}

std::vector<MonitorInfo> Window::get_monitors() const {
    std::vector<MonitorInfo> result;

    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    GLFWmonitor* primary = glfwGetPrimaryMonitor();

    for (int i = 0; i < count; ++i) {
        MonitorInfo info;
        info.index = i;
        info.is_primary = (monitors[i] == primary);

        const char* name = glfwGetMonitorName(monitors[i]);
        info.name = name != nullptr ? name : "Unknown";

        const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
        info.width = mode->width;
        info.height = mode->height;
        info.refresh_rate = mode->refreshRate;

        glfwGetMonitorPos(monitors[i], &info.position_x, &info.position_y);

        result.push_back(info);
    }

    return result;
}

std::optional<MonitorInfo> Window::get_current_monitor() const {
    if (impl_->window == nullptr) {
        return std::nullopt;
    }

    // Find which monitor the window is currently on
    int wx = 0;
    int wy = 0;
    glfwGetWindowPos(impl_->window, &wx, &wy);

    int ww = 0;
    int wh = 0;
    glfwGetWindowSize(impl_->window, &ww, &wh);

    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    GLFWmonitor* best_monitor = nullptr;
    int best_overlap = 0;

    for (int i = 0; i < count; ++i) {
        int mx = 0;
        int my = 0;
        glfwGetMonitorPos(monitors[i], &mx, &my);

        const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
        int mw = mode->width;
        int mh = mode->height;

        // Calculate overlap
        int overlap_x = std::max(0, std::min(wx + ww, mx + mw) - std::max(wx, mx));
        int overlap_y = std::max(0, std::min(wy + wh, my + mh) - std::max(wy, my));
        int overlap = overlap_x * overlap_y;

        if (overlap > best_overlap) {
            best_overlap = overlap;
            best_monitor = monitors[i];
        }
    }

    if (best_monitor == nullptr) {
        return std::nullopt;
    }

    // Find info for best monitor
    auto all_monitors = get_monitors();
    for (const auto& info : all_monitors) {
        int mx = 0;
        int my = 0;
        glfwGetMonitorPos(monitors[info.index], &mx, &my);
        if (mx == info.position_x && my == info.position_y) {
            return info;
        }
    }

    return std::nullopt;
}

std::vector<VideoMode> Window::get_video_modes(int32_t monitor_index) const {
    std::vector<VideoMode> result;

    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);

    if (monitor_index < 0 || monitor_index >= count) {
        return result;
    }

    int mode_count = 0;
    const GLFWvidmode* modes = glfwGetVideoModes(monitors[monitor_index], &mode_count);

    for (int i = 0; i < mode_count; ++i) {
        VideoMode mode;
        mode.width = static_cast<uint32_t>(modes[i].width);
        mode.height = static_cast<uint32_t>(modes[i].height);
        mode.refresh_rate = static_cast<uint32_t>(modes[i].refreshRate);
        result.push_back(mode);
    }

    return result;
}

void Window::show() {
    if (impl_->window != nullptr) {
        glfwShowWindow(impl_->window);
    }
}

void Window::hide() {
    if (impl_->window != nullptr) {
        glfwHideWindow(impl_->window);
    }
}

bool Window::is_visible() const {
    if (impl_->window == nullptr) {
        return false;
    }
    return glfwGetWindowAttrib(impl_->window, GLFW_VISIBLE) == GLFW_TRUE;
}

void Window::focus() {
    if (impl_->window != nullptr) {
        glfwFocusWindow(impl_->window);
    }
}

bool Window::is_focused() const {
    if (impl_->window == nullptr) {
        return false;
    }
    return glfwGetWindowAttrib(impl_->window, GLFW_FOCUSED) == GLFW_TRUE;
}

void Window::minimize() {
    if (impl_->window != nullptr) {
        glfwIconifyWindow(impl_->window);
    }
}

void Window::maximize() {
    if (impl_->window != nullptr) {
        glfwMaximizeWindow(impl_->window);
    }
}

void Window::restore() {
    if (impl_->window != nullptr) {
        glfwRestoreWindow(impl_->window);
    }
}

void* Window::get_native_handle() const {
    return impl_->window;
}

void* Window::get_native_window() const {
    if (impl_->window == nullptr) {
        return nullptr;
    }

#if defined(REALCRAFT_PLATFORM_MACOS)
    // Returns NSWindow*
    return glfwGetCocoaWindow(impl_->window);
#elif defined(REALCRAFT_PLATFORM_WINDOWS)
    // Returns HWND
    return glfwGetWin32Window(impl_->window);
#elif defined(REALCRAFT_PLATFORM_LINUX)
    // Returns Window (X11)
    return reinterpret_cast<void*>(glfwGetX11Window(impl_->window));
#else
    return nullptr;
#endif
}

void* Window::get_metal_layer() const {
    return impl_->metal_layer;
}

void Window::set_resize_callback(ResizeCallback callback) {
    impl_->resize_callback = std::move(callback);
}

void Window::set_framebuffer_resize_callback(FramebufferResizeCallback callback) {
    impl_->framebuffer_resize_callback = std::move(callback);
}

void Window::set_close_callback(CloseCallback callback) {
    impl_->close_callback = std::move(callback);
}

void Window::set_focus_callback(FocusCallback callback) {
    impl_->focus_callback = std::move(callback);
}

Input* Window::get_input() {
    return &impl_->input;
}

const Input* Window::get_input() const {
    return &impl_->input;
}

std::unique_ptr<Window> create_window() {
    return std::make_unique<Window>();
}

}  // namespace realcraft::platform
