// RealCraft Platform Abstraction Layer
// input_action.hpp - Input mapping/rebinding system

#pragma once

#include "input.hpp"
#include "key_codes.hpp"

#include <nlohmann/json_fwd.hpp>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace realcraft::platform {

// Input mapper for action-based input binding
// Maps logical actions (e.g., "move_forward") to physical inputs (e.g., W key)
class InputMapper {
public:
    // Binding can be a key or mouse button
    using Binding = std::variant<KeyCode, MouseButton>;

    struct ActionBinding {
        std::string name;
        std::vector<Binding> bindings;
    };

    InputMapper();
    explicit InputMapper(Input* input);
    ~InputMapper();

    // Non-copyable but movable
    InputMapper(const InputMapper&) = delete;
    InputMapper& operator=(const InputMapper&) = delete;
    InputMapper(InputMapper&&) noexcept;
    InputMapper& operator=(InputMapper&&) noexcept;

    // Set the input source
    void set_input(Input* input);

    // Binding management
    void bind_action(const std::string& action, KeyCode key);
    void bind_action(const std::string& action, MouseButton button);
    void unbind_key(const std::string& action, KeyCode key);
    void unbind_button(const std::string& action, MouseButton button);
    void clear_action(const std::string& action);
    void clear_all();

    // Query action state
    [[nodiscard]] bool is_action_pressed(const std::string& action) const;
    [[nodiscard]] bool is_action_just_pressed(const std::string& action) const;
    [[nodiscard]] bool is_action_just_released(const std::string& action) const;

    // Axis-like queries (returns 1.0, -1.0, or 0.0)
    [[nodiscard]] float get_axis(const std::string& positive_action,
                                 const std::string& negative_action) const;

    // Get all bindings for an action
    [[nodiscard]] std::vector<Binding> get_bindings(const std::string& action) const;
    [[nodiscard]] std::vector<std::string> get_all_actions() const;

    // Check if an action exists
    [[nodiscard]] bool has_action(const std::string& action) const;

    // Serialization for settings persistence
    void load_from_json(const nlohmann::json& config);
    [[nodiscard]] nlohmann::json save_to_json() const;

    // Load default bindings (WASD movement, etc.)
    void load_defaults();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace realcraft::platform
