// RealCraft Platform Tests
// key_codes_test.cpp - Key code unit tests

#include <gtest/gtest.h>
#include <realcraft/platform/key_codes.hpp>

using namespace realcraft::platform;

class KeyCodesTest : public ::testing::Test {};

TEST_F(KeyCodesTest, KeyCodeToString) {
    EXPECT_EQ(key_code_to_string(KeyCode::A), "A");
    EXPECT_EQ(key_code_to_string(KeyCode::Space), "Space");
    EXPECT_EQ(key_code_to_string(KeyCode::Escape), "Escape");
    EXPECT_EQ(key_code_to_string(KeyCode::F1), "F1");
    EXPECT_EQ(key_code_to_string(KeyCode::LeftShift), "LeftShift");
}

TEST_F(KeyCodesTest, StringToKeyCode) {
    EXPECT_EQ(string_to_key_code("A"), KeyCode::A);
    EXPECT_EQ(string_to_key_code("Space"), KeyCode::Space);
    EXPECT_EQ(string_to_key_code("Escape"), KeyCode::Escape);
    EXPECT_EQ(string_to_key_code("F1"), KeyCode::F1);
}

TEST_F(KeyCodesTest, StringToKeyCodeCaseInsensitive) {
    EXPECT_EQ(string_to_key_code("a"), KeyCode::A);
    EXPECT_EQ(string_to_key_code("SPACE"), KeyCode::Space);
    EXPECT_EQ(string_to_key_code("escape"), KeyCode::Escape);
    EXPECT_EQ(string_to_key_code("f1"), KeyCode::F1);
    EXPECT_EQ(string_to_key_code("leftshift"), KeyCode::LeftShift);
}

TEST_F(KeyCodesTest, UnknownKey) {
    EXPECT_EQ(string_to_key_code("not_a_key"), KeyCode::Unknown);
    EXPECT_EQ(string_to_key_code(""), KeyCode::Unknown);
    EXPECT_EQ(key_code_to_string(KeyCode::Unknown), "Unknown");
}

TEST_F(KeyCodesTest, RoundTrip) {
    // Test that converting to string and back gives the same key
    auto test_roundtrip = [](KeyCode key) {
        auto str = key_code_to_string(key);
        auto result = string_to_key_code(str);
        EXPECT_EQ(result, key) << "Failed roundtrip for: " << str;
    };

    test_roundtrip(KeyCode::A);
    test_roundtrip(KeyCode::Z);
    test_roundtrip(KeyCode::Num0);
    test_roundtrip(KeyCode::Num9);
    test_roundtrip(KeyCode::Space);
    test_roundtrip(KeyCode::Enter);
    test_roundtrip(KeyCode::Tab);
    test_roundtrip(KeyCode::Escape);
    test_roundtrip(KeyCode::F1);
    test_roundtrip(KeyCode::F12);
    test_roundtrip(KeyCode::LeftShift);
    test_roundtrip(KeyCode::RightControl);
    test_roundtrip(KeyCode::Keypad0);
}

TEST_F(KeyCodesTest, KeyModOperators) {
    KeyMod shift = KeyMod::Shift;
    KeyMod ctrl = KeyMod::Control;

    KeyMod combined = shift | ctrl;
    EXPECT_TRUE(has_modifier(combined, KeyMod::Shift));
    EXPECT_TRUE(has_modifier(combined, KeyMod::Control));
    EXPECT_FALSE(has_modifier(combined, KeyMod::Alt));

    KeyMod result = combined & KeyMod::Shift;
    EXPECT_TRUE(has_modifier(result, KeyMod::Shift));
}

TEST_F(KeyCodesTest, AllLetters) {
    for (int i = 0; i < 26; ++i) {
        KeyCode key = static_cast<KeyCode>(static_cast<int>(KeyCode::A) + i);
        char expected = 'A' + i;
        auto str = key_code_to_string(key);
        EXPECT_EQ(str.size(), 1u);
        EXPECT_EQ(str[0], expected);
    }
}

TEST_F(KeyCodesTest, AllNumbers) {
    for (int i = 0; i < 10; ++i) {
        KeyCode key = static_cast<KeyCode>(static_cast<int>(KeyCode::Num0) + i);
        char expected = '0' + i;
        auto str = key_code_to_string(key);
        EXPECT_EQ(str.size(), 1u);
        EXPECT_EQ(str[0], expected);
    }
}
