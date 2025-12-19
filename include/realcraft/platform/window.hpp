// RealCraft Platform Abstraction Layer
// window.hpp - Window management interface

#pragma once

#include "input.hpp"
#include "platform.hpp"

#include <glm/vec2.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace realcraft::platform {

// Window class wrapping GLFW window with platform-specific extensions
class Window {
public:
    // Configuration for window creation
    struct Config {
        std::string title = "RealCraft";
        uint32_t width = 1280;
        uint32_t height = 720;
        bool fullscreen = false;
        bool resizable = true;
        bool vsync = true;
        int32_t monitor_index = -1;  // -1 for primary monitor
    };

    Window();
    ~Window();

    // Non-copyable but movable
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) noexcept;
    Window& operator=(Window&&) noexcept;

    // Lifecycle
    bool initialize(const Config& config);
    void shutdown();
    [[nodiscard]] bool is_initialized() const;
    [[nodiscard]] bool should_close() const;
    void set_should_close(bool close);
    void poll_events();

    // Window properties
    void set_title(std::string_view title);
    [[nodiscard]] std::string get_title() const;

    // Size handling (window size vs framebuffer size for Retina displays)
    [[nodiscard]] glm::uvec2 get_size() const;
    [[nodiscard]] glm::uvec2 get_framebuffer_size() const;
    void set_size(uint32_t width, uint32_t height);
    [[nodiscard]] glm::ivec2 get_position() const;
    void set_position(int32_t x, int32_t y);

    // Display mode
    void set_fullscreen(bool fullscreen, int32_t monitor_index = -1);
    [[nodiscard]] bool is_fullscreen() const;
    void set_vsync(bool enabled);
    [[nodiscard]] bool is_vsync_enabled() const;

    // Multi-monitor support
    [[nodiscard]] std::vector<MonitorInfo> get_monitors() const;
    [[nodiscard]] std::optional<MonitorInfo> get_current_monitor() const;
    [[nodiscard]] std::vector<VideoMode> get_video_modes(int32_t monitor_index) const;

    // Visibility
    void show();
    void hide();
    [[nodiscard]] bool is_visible() const;
    void focus();
    [[nodiscard]] bool is_focused() const;
    void minimize();
    void maximize();
    void restore();

    // Native handles for graphics API integration
    [[nodiscard]] void* get_native_handle() const;  // GLFW window pointer
    [[nodiscard]] void* get_native_window() const;  // NSWindow* on macOS, HWND on Windows
    [[nodiscard]] void* get_metal_layer() const;    // CAMetalLayer* (macOS only)

    // Callbacks
    using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;
    using FramebufferResizeCallback = std::function<void(uint32_t width, uint32_t height)>;
    using CloseCallback = std::function<void()>;
    using FocusCallback = std::function<void(bool focused)>;

    void set_resize_callback(ResizeCallback callback);
    void set_framebuffer_resize_callback(FramebufferResizeCallback callback);
    void set_close_callback(CloseCallback callback);
    void set_focus_callback(FocusCallback callback);

    // Access to associated input system
    [[nodiscard]] Input* get_input();
    [[nodiscard]] const Input* get_input() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Factory function - creates a new window instance
std::unique_ptr<Window> create_window();

}  // namespace realcraft::platform
