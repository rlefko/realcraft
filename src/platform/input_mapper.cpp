// RealCraft Platform Abstraction Layer
// input_mapper.cpp - Input action mapping implementation

#include <realcraft/platform/input_action.hpp>

#include <nlohmann/json.hpp>
#include <algorithm>
#include <unordered_map>

namespace realcraft::platform {

struct InputMapper::Impl {
    Input* input = nullptr;
    std::unordered_map<std::string, ActionBinding> actions;
};

InputMapper::InputMapper() : impl_(std::make_unique<Impl>()) {}

InputMapper::InputMapper(Input* input) : impl_(std::make_unique<Impl>()) {
    impl_->input = input;
}

InputMapper::~InputMapper() = default;

InputMapper::InputMapper(InputMapper&&) noexcept = default;

InputMapper& InputMapper::operator=(InputMapper&&) noexcept = default;

void InputMapper::set_input(Input* input) {
    impl_->input = input;
}

void InputMapper::bind_action(const std::string& action, KeyCode key) {
    auto& binding = impl_->actions[action];
    binding.name = action;

    // Check if already bound
    for (const auto& b : binding.bindings) {
        if (auto* k = std::get_if<KeyCode>(&b)) {
            if (*k == key) {
                return;
            }
        }
    }

    binding.bindings.emplace_back(key);
}

void InputMapper::bind_action(const std::string& action, MouseButton button) {
    auto& binding = impl_->actions[action];
    binding.name = action;

    // Check if already bound
    for (const auto& b : binding.bindings) {
        if (auto* mb = std::get_if<MouseButton>(&b)) {
            if (*mb == button) {
                return;
            }
        }
    }

    binding.bindings.emplace_back(button);
}

void InputMapper::unbind_key(const std::string& action, KeyCode key) {
    auto it = impl_->actions.find(action);
    if (it == impl_->actions.end()) {
        return;
    }

    auto& bindings = it->second.bindings;
    bindings.erase(std::remove_if(bindings.begin(), bindings.end(),
                                  [key](const Binding& b) {
                                      if (auto* k = std::get_if<KeyCode>(&b)) {
                                          return *k == key;
                                      }
                                      return false;
                                  }),
                   bindings.end());
}

void InputMapper::unbind_button(const std::string& action, MouseButton button) {
    auto it = impl_->actions.find(action);
    if (it == impl_->actions.end()) {
        return;
    }

    auto& bindings = it->second.bindings;
    bindings.erase(std::remove_if(bindings.begin(), bindings.end(),
                                  [button](const Binding& b) {
                                      if (auto* mb = std::get_if<MouseButton>(&b)) {
                                          return *mb == button;
                                      }
                                      return false;
                                  }),
                   bindings.end());
}

void InputMapper::clear_action(const std::string& action) {
    impl_->actions.erase(action);
}

void InputMapper::clear_all() {
    impl_->actions.clear();
}

bool InputMapper::is_action_pressed(const std::string& action) const {
    if (impl_->input == nullptr) {
        return false;
    }

    auto it = impl_->actions.find(action);
    if (it == impl_->actions.end()) {
        return false;
    }

    for (const auto& binding : it->second.bindings) {
        if (auto* key = std::get_if<KeyCode>(&binding)) {
            if (impl_->input->is_key_pressed(*key)) {
                return true;
            }
        } else if (auto* button = std::get_if<MouseButton>(&binding)) {
            if (impl_->input->is_mouse_button_pressed(*button)) {
                return true;
            }
        }
    }

    return false;
}

bool InputMapper::is_action_just_pressed(const std::string& action) const {
    if (impl_->input == nullptr) {
        return false;
    }

    auto it = impl_->actions.find(action);
    if (it == impl_->actions.end()) {
        return false;
    }

    for (const auto& binding : it->second.bindings) {
        if (auto* key = std::get_if<KeyCode>(&binding)) {
            if (impl_->input->is_key_just_pressed(*key)) {
                return true;
            }
        } else if (auto* button = std::get_if<MouseButton>(&binding)) {
            if (impl_->input->is_mouse_button_just_pressed(*button)) {
                return true;
            }
        }
    }

    return false;
}

bool InputMapper::is_action_just_released(const std::string& action) const {
    if (impl_->input == nullptr) {
        return false;
    }

    auto it = impl_->actions.find(action);
    if (it == impl_->actions.end()) {
        return false;
    }

    for (const auto& binding : it->second.bindings) {
        if (auto* key = std::get_if<KeyCode>(&binding)) {
            if (impl_->input->is_key_just_released(*key)) {
                return true;
            }
        } else if (auto* button = std::get_if<MouseButton>(&binding)) {
            if (impl_->input->is_mouse_button_just_released(*button)) {
                return true;
            }
        }
    }

    return false;
}

float InputMapper::get_axis(const std::string& positive_action,
                            const std::string& negative_action) const {
    float value = 0.0f;
    if (is_action_pressed(positive_action)) {
        value += 1.0f;
    }
    if (is_action_pressed(negative_action)) {
        value -= 1.0f;
    }
    return value;
}

std::vector<InputMapper::Binding> InputMapper::get_bindings(const std::string& action) const {
    auto it = impl_->actions.find(action);
    if (it == impl_->actions.end()) {
        return {};
    }
    return it->second.bindings;
}

std::vector<std::string> InputMapper::get_all_actions() const {
    std::vector<std::string> result;
    result.reserve(impl_->actions.size());
    for (const auto& [name, binding] : impl_->actions) {
        result.push_back(name);
    }
    return result;
}

bool InputMapper::has_action(const std::string& action) const {
    return impl_->actions.find(action) != impl_->actions.end();
}

void InputMapper::load_from_json(const nlohmann::json& config) {
    clear_all();

    if (!config.is_object()) {
        return;
    }

    for (const auto& [action_name, bindings] : config.items()) {
        if (!bindings.is_array()) {
            continue;
        }

        for (const auto& binding : bindings) {
            if (!binding.is_object()) {
                continue;
            }

            if (binding.contains("key")) {
                std::string key_name = binding["key"].get<std::string>();
                KeyCode key = string_to_key_code(key_name);
                if (key != KeyCode::Unknown) {
                    bind_action(action_name, key);
                }
            } else if (binding.contains("mouse")) {
                std::string button_name = binding["mouse"].get<std::string>();
                if (button_name == "left") {
                    bind_action(action_name, MouseButton::Left);
                } else if (button_name == "right") {
                    bind_action(action_name, MouseButton::Right);
                } else if (button_name == "middle") {
                    bind_action(action_name, MouseButton::Middle);
                } else if (button_name == "button4") {
                    bind_action(action_name, MouseButton::Button4);
                } else if (button_name == "button5") {
                    bind_action(action_name, MouseButton::Button5);
                }
            }
        }
    }
}

nlohmann::json InputMapper::save_to_json() const {
    nlohmann::json result = nlohmann::json::object();

    for (const auto& [action_name, action_binding] : impl_->actions) {
        nlohmann::json bindings_json = nlohmann::json::array();

        for (const auto& binding : action_binding.bindings) {
            nlohmann::json binding_json;

            if (auto* key = std::get_if<KeyCode>(&binding)) {
                binding_json["key"] = std::string(key_code_to_string(*key));
            } else if (auto* button = std::get_if<MouseButton>(&binding)) {
                switch (*button) {
                    case MouseButton::Left:
                        binding_json["mouse"] = "left";
                        break;
                    case MouseButton::Right:
                        binding_json["mouse"] = "right";
                        break;
                    case MouseButton::Middle:
                        binding_json["mouse"] = "middle";
                        break;
                    case MouseButton::Button4:
                        binding_json["mouse"] = "button4";
                        break;
                    case MouseButton::Button5:
                        binding_json["mouse"] = "button5";
                        break;
                    default:
                        break;
                }
            }

            bindings_json.push_back(binding_json);
        }

        result[action_name] = bindings_json;
    }

    return result;
}

void InputMapper::load_defaults() {
    clear_all();

    // Movement
    bind_action("move_forward", KeyCode::W);
    bind_action("move_backward", KeyCode::S);
    bind_action("move_left", KeyCode::A);
    bind_action("move_right", KeyCode::D);

    // Jump and crouch
    bind_action("jump", KeyCode::Space);
    bind_action("crouch", KeyCode::LeftControl);
    bind_action("sprint", KeyCode::LeftShift);

    // Actions
    bind_action("primary_action", MouseButton::Left);    // Break block
    bind_action("secondary_action", MouseButton::Right);  // Place block

    // Hotbar
    bind_action("hotbar_1", KeyCode::Num1);
    bind_action("hotbar_2", KeyCode::Num2);
    bind_action("hotbar_3", KeyCode::Num3);
    bind_action("hotbar_4", KeyCode::Num4);
    bind_action("hotbar_5", KeyCode::Num5);
    bind_action("hotbar_6", KeyCode::Num6);
    bind_action("hotbar_7", KeyCode::Num7);
    bind_action("hotbar_8", KeyCode::Num8);
    bind_action("hotbar_9", KeyCode::Num9);

    // UI
    bind_action("toggle_inventory", KeyCode::E);
    bind_action("toggle_pause", KeyCode::Escape);
    bind_action("toggle_debug", KeyCode::F3);
    bind_action("toggle_fullscreen", KeyCode::F11);
}

}  // namespace realcraft::platform
