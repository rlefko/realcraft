// RealCraft Platform Abstraction Layer
// input.hpp - Input handling interface

#pragma once

#include "key_codes.hpp"
#include "platform.hpp"

#include <glm/vec2.hpp>

#include <functional>
#include <memory>

namespace realcraft::platform {

// Mouse button identifiers
enum class MouseButton : uint8_t { Left = 0, Right = 1, Middle = 2, Button4 = 3, Button5 = 4, Count = 5 };

// Key state
enum class KeyState : uint8_t { Released = 0, Pressed = 1, Repeat = 2 };

// Input handling class - typically owned by a Window
class Input {
public:
    Input();
    ~Input();

    // Non-copyable but movable
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;
    Input(Input&&) noexcept;
    Input& operator=(Input&&) noexcept;

    // Keyboard state queries
    [[nodiscard]] KeyState get_key_state(KeyCode key) const;
    [[nodiscard]] bool is_key_pressed(KeyCode key) const;
    [[nodiscard]] bool is_key_just_pressed(KeyCode key) const;
    [[nodiscard]] bool is_key_just_released(KeyCode key) const;

    // Mouse position (in window coordinates)
    [[nodiscard]] glm::dvec2 get_mouse_position() const;
    [[nodiscard]] glm::dvec2 get_mouse_delta() const;

    // Mouse button state
    [[nodiscard]] bool is_mouse_button_pressed(MouseButton button) const;
    [[nodiscard]] bool is_mouse_button_just_pressed(MouseButton button) const;
    [[nodiscard]] bool is_mouse_button_just_released(MouseButton button) const;

    // Scroll wheel
    [[nodiscard]] glm::dvec2 get_scroll_delta() const;

    // Mouse capture (for FPS-style camera control)
    void set_mouse_captured(bool captured);
    [[nodiscard]] bool is_mouse_captured() const;

    // Cursor visibility (separate from capture)
    void set_cursor_visible(bool visible);
    [[nodiscard]] bool is_cursor_visible() const;

    // Called at end of frame to update "just pressed" states
    void end_frame();

    // Raw input callbacks (for UI systems, text input, etc.)
    using KeyCallback = std::function<void(KeyCode key, KeyState state, KeyMod mods)>;
    using CharCallback = std::function<void(uint32_t codepoint)>;
    using MouseMoveCallback = std::function<void(double x, double y)>;
    using MouseButtonCallback = std::function<void(MouseButton button, bool pressed)>;
    using ScrollCallback = std::function<void(double x_offset, double y_offset)>;

    void set_key_callback(KeyCallback callback);
    void set_char_callback(CharCallback callback);
    void set_mouse_move_callback(MouseMoveCallback callback);
    void set_mouse_button_callback(MouseButtonCallback callback);
    void set_scroll_callback(ScrollCallback callback);

    // Internal: called by Window to process GLFW events
    void on_key_event(int key, int scancode, int action, int mods);
    void on_char_event(uint32_t codepoint);
    void on_mouse_move_event(double x, double y);
    void on_mouse_button_event(int button, int action, int mods);
    void on_scroll_event(double x_offset, double y_offset);

    // Internal: set GLFW window pointer
    void set_glfw_window(void* window);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::platform
