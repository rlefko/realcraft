// RealCraft Platform Abstraction Layer
// key_codes.cpp - Key code string conversion

#include <realcraft/platform/key_codes.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <unordered_map>

namespace realcraft::platform {

namespace {

// Key code to string mapping
struct KeyMapping {
    KeyCode code;
    std::string_view name;
};

// clang-format off
constexpr std::array<KeyMapping, 121> KEY_MAPPINGS = {{
    // Special keys
    {KeyCode::Unknown, "Unknown"},
    {KeyCode::Space, "Space"},
    {KeyCode::Apostrophe, "'"},
    {KeyCode::Comma, ","},
    {KeyCode::Minus, "-"},
    {KeyCode::Period, "."},
    {KeyCode::Slash, "/"},

    // Numbers
    {KeyCode::Num0, "0"},
    {KeyCode::Num1, "1"},
    {KeyCode::Num2, "2"},
    {KeyCode::Num3, "3"},
    {KeyCode::Num4, "4"},
    {KeyCode::Num5, "5"},
    {KeyCode::Num6, "6"},
    {KeyCode::Num7, "7"},
    {KeyCode::Num8, "8"},
    {KeyCode::Num9, "9"},

    {KeyCode::Semicolon, ";"},
    {KeyCode::Equal, "="},

    // Letters
    {KeyCode::A, "A"},
    {KeyCode::B, "B"},
    {KeyCode::C, "C"},
    {KeyCode::D, "D"},
    {KeyCode::E, "E"},
    {KeyCode::F, "F"},
    {KeyCode::G, "G"},
    {KeyCode::H, "H"},
    {KeyCode::I, "I"},
    {KeyCode::J, "J"},
    {KeyCode::K, "K"},
    {KeyCode::L, "L"},
    {KeyCode::M, "M"},
    {KeyCode::N, "N"},
    {KeyCode::O, "O"},
    {KeyCode::P, "P"},
    {KeyCode::Q, "Q"},
    {KeyCode::R, "R"},
    {KeyCode::S, "S"},
    {KeyCode::T, "T"},
    {KeyCode::U, "U"},
    {KeyCode::V, "V"},
    {KeyCode::W, "W"},
    {KeyCode::X, "X"},
    {KeyCode::Y, "Y"},
    {KeyCode::Z, "Z"},

    {KeyCode::LeftBracket, "["},
    {KeyCode::Backslash, "\\"},
    {KeyCode::RightBracket, "]"},
    {KeyCode::GraveAccent, "`"},

    // Function keys
    {KeyCode::Escape, "Escape"},
    {KeyCode::Enter, "Enter"},
    {KeyCode::Tab, "Tab"},
    {KeyCode::Backspace, "Backspace"},
    {KeyCode::Insert, "Insert"},
    {KeyCode::Delete, "Delete"},
    {KeyCode::Right, "Right"},
    {KeyCode::Left, "Left"},
    {KeyCode::Down, "Down"},
    {KeyCode::Up, "Up"},
    {KeyCode::PageUp, "PageUp"},
    {KeyCode::PageDown, "PageDown"},
    {KeyCode::Home, "Home"},
    {KeyCode::End, "End"},
    {KeyCode::CapsLock, "CapsLock"},
    {KeyCode::ScrollLock, "ScrollLock"},
    {KeyCode::NumLock, "NumLock"},
    {KeyCode::PrintScreen, "PrintScreen"},
    {KeyCode::Pause, "Pause"},

    // F keys
    {KeyCode::F1, "F1"},
    {KeyCode::F2, "F2"},
    {KeyCode::F3, "F3"},
    {KeyCode::F4, "F4"},
    {KeyCode::F5, "F5"},
    {KeyCode::F6, "F6"},
    {KeyCode::F7, "F7"},
    {KeyCode::F8, "F8"},
    {KeyCode::F9, "F9"},
    {KeyCode::F10, "F10"},
    {KeyCode::F11, "F11"},
    {KeyCode::F12, "F12"},
    {KeyCode::F13, "F13"},
    {KeyCode::F14, "F14"},
    {KeyCode::F15, "F15"},
    {KeyCode::F16, "F16"},
    {KeyCode::F17, "F17"},
    {KeyCode::F18, "F18"},
    {KeyCode::F19, "F19"},
    {KeyCode::F20, "F20"},
    {KeyCode::F21, "F21"},
    {KeyCode::F22, "F22"},
    {KeyCode::F23, "F23"},
    {KeyCode::F24, "F24"},
    {KeyCode::F25, "F25"},

    // Keypad
    {KeyCode::Keypad0, "Keypad0"},
    {KeyCode::Keypad1, "Keypad1"},
    {KeyCode::Keypad2, "Keypad2"},
    {KeyCode::Keypad3, "Keypad3"},
    {KeyCode::Keypad4, "Keypad4"},
    {KeyCode::Keypad5, "Keypad5"},
    {KeyCode::Keypad6, "Keypad6"},
    {KeyCode::Keypad7, "Keypad7"},
    {KeyCode::Keypad8, "Keypad8"},
    {KeyCode::Keypad9, "Keypad9"},
    {KeyCode::KeypadDecimal, "KeypadDecimal"},
    {KeyCode::KeypadDivide, "KeypadDivide"},
    {KeyCode::KeypadMultiply, "KeypadMultiply"},
    {KeyCode::KeypadSubtract, "KeypadSubtract"},
    {KeyCode::KeypadAdd, "KeypadAdd"},
    {KeyCode::KeypadEnter, "KeypadEnter"},
    {KeyCode::KeypadEqual, "KeypadEqual"},

    // Modifiers
    {KeyCode::LeftShift, "LeftShift"},
    {KeyCode::LeftControl, "LeftControl"},
    {KeyCode::LeftAlt, "LeftAlt"},
    {KeyCode::LeftSuper, "LeftSuper"},
    {KeyCode::RightShift, "RightShift"},
    {KeyCode::RightControl, "RightControl"},
    {KeyCode::RightAlt, "RightAlt"},
    {KeyCode::RightSuper, "RightSuper"},
    {KeyCode::Menu, "Menu"},
}};
// clang-format on

// Build reverse lookup map on first use
const std::unordered_map<std::string, KeyCode>& get_string_to_key_map() {
    static std::unordered_map<std::string, KeyCode> map = []() {
        std::unordered_map<std::string, KeyCode> m;
        for (const auto& mapping : KEY_MAPPINGS) {
            std::string lower_name(mapping.name);
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            m[lower_name] = mapping.code;
        }
        return m;
    }();
    return map;
}

}  // namespace

std::string_view key_code_to_string(KeyCode key) {
    for (const auto& mapping : KEY_MAPPINGS) {
        if (mapping.code == key) {
            return mapping.name;
        }
    }
    return "Unknown";
}

KeyCode string_to_key_code(std::string_view name) {
    if (name.empty()) {
        return KeyCode::Unknown;
    }

    std::string lower_name(name);
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    const auto& map = get_string_to_key_map();
    auto it = map.find(lower_name);
    if (it != map.end()) {
        return it->second;
    }
    return KeyCode::Unknown;
}

}  // namespace realcraft::platform
