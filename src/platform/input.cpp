// RealCraft Platform Abstraction Layer
// input.cpp - GLFW-based input implementation

#include <realcraft/platform/input.hpp>

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include <array>
#include <cstring>

namespace realcraft::platform {

struct Input::Impl {
    GLFWwindow* window = nullptr;

    // Key states
    std::array<KeyState, static_cast<size_t>(KeyCode::MaxValue)> key_states{};
    std::array<KeyState, static_cast<size_t>(KeyCode::MaxValue)> prev_key_states{};

    // Mouse button states
    std::array<bool, static_cast<size_t>(MouseButton::Count)> mouse_states{};
    std::array<bool, static_cast<size_t>(MouseButton::Count)> prev_mouse_states{};

    // Mouse position
    glm::dvec2 mouse_position{0.0, 0.0};
    glm::dvec2 prev_mouse_position{0.0, 0.0};
    glm::dvec2 mouse_delta{0.0, 0.0};
    bool first_mouse_move = true;

    // Scroll
    glm::dvec2 scroll_delta{0.0, 0.0};

    // Capture state
    bool mouse_captured = false;
    bool cursor_visible = true;

    // Callbacks
    KeyCallback key_callback;
    CharCallback char_callback;
    MouseMoveCallback mouse_move_callback;
    MouseButtonCallback mouse_button_callback;
    ScrollCallback scroll_callback;
};

Input::Input() : impl_(std::make_unique<Impl>()) {
    impl_->key_states.fill(KeyState::Released);
    impl_->prev_key_states.fill(KeyState::Released);
    impl_->mouse_states.fill(false);
    impl_->prev_mouse_states.fill(false);
}

Input::~Input() = default;

Input::Input(Input&&) noexcept = default;

Input& Input::operator=(Input&&) noexcept = default;

KeyState Input::get_key_state(KeyCode key) const {
    auto index = static_cast<size_t>(key);
    if (index >= impl_->key_states.size()) {
        return KeyState::Released;
    }
    return impl_->key_states[index];
}

bool Input::is_key_pressed(KeyCode key) const {
    auto state = get_key_state(key);
    return state == KeyState::Pressed || state == KeyState::Repeat;
}

bool Input::is_key_just_pressed(KeyCode key) const {
    auto index = static_cast<size_t>(key);
    if (index >= impl_->key_states.size()) {
        return false;
    }
    return impl_->key_states[index] == KeyState::Pressed &&
           impl_->prev_key_states[index] == KeyState::Released;
}

bool Input::is_key_just_released(KeyCode key) const {
    auto index = static_cast<size_t>(key);
    if (index >= impl_->key_states.size()) {
        return false;
    }
    return impl_->key_states[index] == KeyState::Released &&
           (impl_->prev_key_states[index] == KeyState::Pressed ||
            impl_->prev_key_states[index] == KeyState::Repeat);
}

glm::dvec2 Input::get_mouse_position() const {
    return impl_->mouse_position;
}

glm::dvec2 Input::get_mouse_delta() const {
    return impl_->mouse_delta;
}

bool Input::is_mouse_button_pressed(MouseButton button) const {
    auto index = static_cast<size_t>(button);
    if (index >= impl_->mouse_states.size()) {
        return false;
    }
    return impl_->mouse_states[index];
}

bool Input::is_mouse_button_just_pressed(MouseButton button) const {
    auto index = static_cast<size_t>(button);
    if (index >= impl_->mouse_states.size()) {
        return false;
    }
    return impl_->mouse_states[index] && !impl_->prev_mouse_states[index];
}

bool Input::is_mouse_button_just_released(MouseButton button) const {
    auto index = static_cast<size_t>(button);
    if (index >= impl_->mouse_states.size()) {
        return false;
    }
    return !impl_->mouse_states[index] && impl_->prev_mouse_states[index];
}

glm::dvec2 Input::get_scroll_delta() const {
    return impl_->scroll_delta;
}

void Input::set_mouse_captured(bool captured) {
    impl_->mouse_captured = captured;

    if (impl_->window != nullptr) {
        if (captured) {
            glfwSetInputMode(impl_->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            // Enable raw mouse motion if available
            if (glfwRawMouseMotionSupported()) {
                glfwSetInputMode(impl_->window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            }
        } else {
            glfwSetInputMode(impl_->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            glfwSetInputMode(impl_->window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
        }
        impl_->first_mouse_move = true;
    }
}

bool Input::is_mouse_captured() const {
    return impl_->mouse_captured;
}

void Input::set_cursor_visible(bool visible) {
    impl_->cursor_visible = visible;

    if (impl_->window != nullptr && !impl_->mouse_captured) {
        glfwSetInputMode(impl_->window, GLFW_CURSOR,
                         visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
    }
}

bool Input::is_cursor_visible() const {
    return impl_->cursor_visible;
}

void Input::end_frame() {
    // Copy current states to previous
    impl_->prev_key_states = impl_->key_states;
    impl_->prev_mouse_states = impl_->mouse_states;

    // Reset scroll delta
    impl_->scroll_delta = {0.0, 0.0};

    // Reset mouse delta
    impl_->prev_mouse_position = impl_->mouse_position;
    impl_->mouse_delta = {0.0, 0.0};
}

void Input::set_key_callback(KeyCallback callback) {
    impl_->key_callback = std::move(callback);
}

void Input::set_char_callback(CharCallback callback) {
    impl_->char_callback = std::move(callback);
}

void Input::set_mouse_move_callback(MouseMoveCallback callback) {
    impl_->mouse_move_callback = std::move(callback);
}

void Input::set_mouse_button_callback(MouseButtonCallback callback) {
    impl_->mouse_button_callback = std::move(callback);
}

void Input::set_scroll_callback(ScrollCallback callback) {
    impl_->scroll_callback = std::move(callback);
}

void Input::on_key_event(int key, [[maybe_unused]] int scancode, int action, int mods) {
    if (key < 0 || key >= static_cast<int>(KeyCode::MaxValue)) {
        return;
    }

    KeyState state = KeyState::Released;
    if (action == GLFW_PRESS) {
        state = KeyState::Pressed;
    } else if (action == GLFW_REPEAT) {
        state = KeyState::Repeat;
    }

    impl_->key_states[static_cast<size_t>(key)] = state;

    if (impl_->key_callback) {
        impl_->key_callback(static_cast<KeyCode>(key), state, static_cast<KeyMod>(mods));
    }
}

void Input::on_char_event(uint32_t codepoint) {
    if (impl_->char_callback) {
        impl_->char_callback(codepoint);
    }
}

void Input::on_mouse_move_event(double x, double y) {
    if (impl_->first_mouse_move) {
        impl_->prev_mouse_position = {x, y};
        impl_->first_mouse_move = false;
    }

    impl_->mouse_position = {x, y};
    impl_->mouse_delta = impl_->mouse_position - impl_->prev_mouse_position;

    if (impl_->mouse_move_callback) {
        impl_->mouse_move_callback(x, y);
    }
}

void Input::on_mouse_button_event(int button, int action, [[maybe_unused]] int mods) {
    if (button < 0 || button >= static_cast<int>(MouseButton::Count)) {
        return;
    }

    bool pressed = (action == GLFW_PRESS);
    impl_->mouse_states[static_cast<size_t>(button)] = pressed;

    if (impl_->mouse_button_callback) {
        impl_->mouse_button_callback(static_cast<MouseButton>(button), pressed);
    }
}

void Input::on_scroll_event(double x_offset, double y_offset) {
    impl_->scroll_delta = {x_offset, y_offset};

    if (impl_->scroll_callback) {
        impl_->scroll_callback(x_offset, y_offset);
    }
}

void Input::set_glfw_window(void* window) {
    impl_->window = static_cast<GLFWwindow*>(window);
}

}  // namespace realcraft::platform
