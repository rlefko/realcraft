// RealCraft Platform Tests
// input_mapper_test.cpp - Input mapper unit tests

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <realcraft/platform/input_action.hpp>

using namespace realcraft::platform;

class InputMapperTest : public ::testing::Test {
protected:
    InputMapper mapper_;
};

TEST_F(InputMapperTest, BindKeyAction) {
    mapper_.bind_action("jump", KeyCode::Space);

    EXPECT_TRUE(mapper_.has_action("jump"));
    auto bindings = mapper_.get_bindings("jump");
    EXPECT_EQ(bindings.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<KeyCode>(bindings[0]));
    EXPECT_EQ(std::get<KeyCode>(bindings[0]), KeyCode::Space);
}

TEST_F(InputMapperTest, BindMouseAction) {
    mapper_.bind_action("attack", MouseButton::Left);

    EXPECT_TRUE(mapper_.has_action("attack"));
    auto bindings = mapper_.get_bindings("attack");
    EXPECT_EQ(bindings.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<MouseButton>(bindings[0]));
    EXPECT_EQ(std::get<MouseButton>(bindings[0]), MouseButton::Left);
}

TEST_F(InputMapperTest, MultipleBindingsForAction) {
    mapper_.bind_action("jump", KeyCode::Space);
    mapper_.bind_action("jump", KeyCode::W);

    auto bindings = mapper_.get_bindings("jump");
    EXPECT_EQ(bindings.size(), 2u);
}

TEST_F(InputMapperTest, DuplicateBindingIgnored) {
    mapper_.bind_action("jump", KeyCode::Space);
    mapper_.bind_action("jump", KeyCode::Space);

    auto bindings = mapper_.get_bindings("jump");
    EXPECT_EQ(bindings.size(), 1u);
}

TEST_F(InputMapperTest, UnbindKey) {
    mapper_.bind_action("move", KeyCode::W);
    mapper_.bind_action("move", KeyCode::Up);

    mapper_.unbind_key("move", KeyCode::W);

    auto bindings = mapper_.get_bindings("move");
    EXPECT_EQ(bindings.size(), 1u);
    EXPECT_EQ(std::get<KeyCode>(bindings[0]), KeyCode::Up);
}

TEST_F(InputMapperTest, UnbindMouseButton) {
    mapper_.bind_action("action", MouseButton::Left);
    mapper_.bind_action("action", MouseButton::Right);

    mapper_.unbind_button("action", MouseButton::Left);

    auto bindings = mapper_.get_bindings("action");
    EXPECT_EQ(bindings.size(), 1u);
    EXPECT_EQ(std::get<MouseButton>(bindings[0]), MouseButton::Right);
}

TEST_F(InputMapperTest, ClearAction) {
    mapper_.bind_action("jump", KeyCode::Space);
    mapper_.bind_action("jump", KeyCode::W);

    mapper_.clear_action("jump");

    EXPECT_FALSE(mapper_.has_action("jump"));
    auto bindings = mapper_.get_bindings("jump");
    EXPECT_TRUE(bindings.empty());
}

TEST_F(InputMapperTest, ClearAll) {
    mapper_.bind_action("jump", KeyCode::Space);
    mapper_.bind_action("move", KeyCode::W);
    mapper_.bind_action("attack", MouseButton::Left);

    mapper_.clear_all();

    EXPECT_FALSE(mapper_.has_action("jump"));
    EXPECT_FALSE(mapper_.has_action("move"));
    EXPECT_FALSE(mapper_.has_action("attack"));

    auto actions = mapper_.get_all_actions();
    EXPECT_TRUE(actions.empty());
}

TEST_F(InputMapperTest, GetAllActions) {
    mapper_.bind_action("jump", KeyCode::Space);
    mapper_.bind_action("move", KeyCode::W);
    mapper_.bind_action("attack", MouseButton::Left);

    auto actions = mapper_.get_all_actions();
    EXPECT_EQ(actions.size(), 3u);

    // Check all expected actions are present
    EXPECT_TRUE(std::find(actions.begin(), actions.end(), "jump") != actions.end());
    EXPECT_TRUE(std::find(actions.begin(), actions.end(), "move") != actions.end());
    EXPECT_TRUE(std::find(actions.begin(), actions.end(), "attack") != actions.end());
}

TEST_F(InputMapperTest, SaveToJson) {
    mapper_.bind_action("jump", KeyCode::Space);
    mapper_.bind_action("attack", MouseButton::Left);

    auto json = mapper_.save_to_json();

    EXPECT_TRUE(json.is_object());
    EXPECT_TRUE(json.contains("jump"));
    EXPECT_TRUE(json.contains("attack"));

    EXPECT_TRUE(json["jump"].is_array());
    EXPECT_EQ(json["jump"].size(), 1u);
    EXPECT_EQ(json["jump"][0]["key"], "Space");

    EXPECT_TRUE(json["attack"].is_array());
    EXPECT_EQ(json["attack"].size(), 1u);
    EXPECT_EQ(json["attack"][0]["mouse"], "left");
}

TEST_F(InputMapperTest, LoadFromJson) {
    nlohmann::json config = {{"jump", {{{"key", "Space"}}, {{"key", "W"}}}},
                             {"attack", {{{"mouse", "left"}}}}};

    mapper_.load_from_json(config);

    EXPECT_TRUE(mapper_.has_action("jump"));
    EXPECT_TRUE(mapper_.has_action("attack"));

    auto jump_bindings = mapper_.get_bindings("jump");
    EXPECT_EQ(jump_bindings.size(), 2u);

    auto attack_bindings = mapper_.get_bindings("attack");
    EXPECT_EQ(attack_bindings.size(), 1u);
    EXPECT_EQ(std::get<MouseButton>(attack_bindings[0]), MouseButton::Left);
}

TEST_F(InputMapperTest, LoadDefaults) {
    mapper_.load_defaults();

    // Check some expected default bindings
    EXPECT_TRUE(mapper_.has_action("move_forward"));
    EXPECT_TRUE(mapper_.has_action("move_backward"));
    EXPECT_TRUE(mapper_.has_action("move_left"));
    EXPECT_TRUE(mapper_.has_action("move_right"));
    EXPECT_TRUE(mapper_.has_action("jump"));
    EXPECT_TRUE(mapper_.has_action("primary_action"));
    EXPECT_TRUE(mapper_.has_action("secondary_action"));
    EXPECT_TRUE(mapper_.has_action("toggle_fullscreen"));

    // Check WASD bindings
    auto forward = mapper_.get_bindings("move_forward");
    EXPECT_FALSE(forward.empty());
    EXPECT_EQ(std::get<KeyCode>(forward[0]), KeyCode::W);

    // Check mouse bindings
    auto primary = mapper_.get_bindings("primary_action");
    EXPECT_FALSE(primary.empty());
    EXPECT_EQ(std::get<MouseButton>(primary[0]), MouseButton::Left);
}

TEST_F(InputMapperTest, JsonRoundTrip) {
    mapper_.bind_action("test1", KeyCode::A);
    mapper_.bind_action("test1", KeyCode::B);
    mapper_.bind_action("test2", MouseButton::Middle);

    auto json = mapper_.save_to_json();

    InputMapper mapper2;
    mapper2.load_from_json(json);

    EXPECT_TRUE(mapper2.has_action("test1"));
    EXPECT_TRUE(mapper2.has_action("test2"));

    auto bindings1 = mapper2.get_bindings("test1");
    EXPECT_EQ(bindings1.size(), 2u);

    auto bindings2 = mapper2.get_bindings("test2");
    EXPECT_EQ(bindings2.size(), 1u);
    EXPECT_EQ(std::get<MouseButton>(bindings2[0]), MouseButton::Middle);
}

TEST_F(InputMapperTest, GetNonexistentBindings) {
    auto bindings = mapper_.get_bindings("nonexistent");
    EXPECT_TRUE(bindings.empty());
}

TEST_F(InputMapperTest, HasAction) {
    EXPECT_FALSE(mapper_.has_action("test"));

    mapper_.bind_action("test", KeyCode::T);
    EXPECT_TRUE(mapper_.has_action("test"));

    mapper_.clear_action("test");
    EXPECT_FALSE(mapper_.has_action("test"));
}
