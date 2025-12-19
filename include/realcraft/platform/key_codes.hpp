// RealCraft Platform Abstraction Layer
// key_codes.hpp - Keyboard key code definitions

#pragma once

#include <cstdint>
#include <string_view>

namespace realcraft::platform {

// Key codes matching GLFW values for easy conversion
enum class KeyCode : int32_t {
    Unknown = -1,

    // Printable keys
    Space = 32,
    Apostrophe = 39,  // '
    Comma = 44,       // ,
    Minus = 45,       // -
    Period = 46,      // .
    Slash = 47,       // /

    Num0 = 48,
    Num1 = 49,
    Num2 = 50,
    Num3 = 51,
    Num4 = 52,
    Num5 = 53,
    Num6 = 54,
    Num7 = 55,
    Num8 = 56,
    Num9 = 57,

    Semicolon = 59,  // ;
    Equal = 61,      // =

    A = 65,
    B = 66,
    C = 67,
    D = 68,
    E = 69,
    F = 70,
    G = 71,
    H = 72,
    I = 73,
    J = 74,
    K = 75,
    L = 76,
    M = 77,
    N = 78,
    O = 79,
    P = 80,
    Q = 81,
    R = 82,
    S = 83,
    T = 84,
    U = 85,
    V = 86,
    W = 87,
    X = 88,
    Y = 89,
    Z = 90,

    LeftBracket = 91,   // [
    Backslash = 92,     // (backslash)
    RightBracket = 93,  // ]
    GraveAccent = 96,   // `

    // Function keys
    Escape = 256,
    Enter = 257,
    Tab = 258,
    Backspace = 259,
    Insert = 260,
    Delete = 261,
    Right = 262,
    Left = 263,
    Down = 264,
    Up = 265,
    PageUp = 266,
    PageDown = 267,
    Home = 268,
    End = 269,
    CapsLock = 280,
    ScrollLock = 281,
    NumLock = 282,
    PrintScreen = 283,
    Pause = 284,

    F1 = 290,
    F2 = 291,
    F3 = 292,
    F4 = 293,
    F5 = 294,
    F6 = 295,
    F7 = 296,
    F8 = 297,
    F9 = 298,
    F10 = 299,
    F11 = 300,
    F12 = 301,
    F13 = 302,
    F14 = 303,
    F15 = 304,
    F16 = 305,
    F17 = 306,
    F18 = 307,
    F19 = 308,
    F20 = 309,
    F21 = 310,
    F22 = 311,
    F23 = 312,
    F24 = 313,
    F25 = 314,

    // Keypad
    Keypad0 = 320,
    Keypad1 = 321,
    Keypad2 = 322,
    Keypad3 = 323,
    Keypad4 = 324,
    Keypad5 = 325,
    Keypad6 = 326,
    Keypad7 = 327,
    Keypad8 = 328,
    Keypad9 = 329,
    KeypadDecimal = 330,
    KeypadDivide = 331,
    KeypadMultiply = 332,
    KeypadSubtract = 333,
    KeypadAdd = 334,
    KeypadEnter = 335,
    KeypadEqual = 336,

    // Modifier keys
    LeftShift = 340,
    LeftControl = 341,
    LeftAlt = 342,
    LeftSuper = 343,  // Windows key / Command key
    RightShift = 344,
    RightControl = 345,
    RightAlt = 346,
    RightSuper = 347,
    Menu = 348,

    // Total count for array sizing
    MaxValue = 349
};

// Modifier key bit flags
enum class KeyMod : int32_t {
    None = 0,
    Shift = 0x0001,
    Control = 0x0002,
    Alt = 0x0004,
    Super = 0x0008,
    CapsLock = 0x0010,
    NumLock = 0x0020
};

// Bitwise operators for KeyMod
inline KeyMod operator|(KeyMod a, KeyMod b) {
    return static_cast<KeyMod>(static_cast<int32_t>(a) | static_cast<int32_t>(b));
}

inline KeyMod operator&(KeyMod a, KeyMod b) {
    return static_cast<KeyMod>(static_cast<int32_t>(a) & static_cast<int32_t>(b));
}

inline bool has_modifier(KeyMod mods, KeyMod flag) {
    return (static_cast<int32_t>(mods) & static_cast<int32_t>(flag)) != 0;
}

// Convert key code to human-readable name
std::string_view key_code_to_string(KeyCode key);

// Parse key name to code (for config loading)
KeyCode string_to_key_code(std::string_view name);

}  // namespace realcraft::platform
