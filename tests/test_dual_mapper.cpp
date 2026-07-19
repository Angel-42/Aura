#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include "Aura/Input/DualMapper.hpp"
#include "Aura/Core/GestureEvent.hpp"

using namespace Aura::Input;
using namespace Aura::Core;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class DualMapperTest : public ::testing::Test {
protected:
    std::filesystem::path tmpFile;

    void SetUp() override {
        tmpFile = std::filesystem::temp_directory_path() / "aura_test_dual.txt";
    }
    void TearDown() override {
        std::filesystem::remove(tmpFile);
    }

    DualMapper load(const std::string& content) {
        { std::ofstream f(tmpFile); f << content; }
        DualMapper m;
        m.load(tmpFile);
        return m;
    }
};

// ---------------------------------------------------------------------------
// Chargement
// ---------------------------------------------------------------------------

TEST_F(DualMapperTest, NonExistentFileReturnsFalse) {
    DualMapper m;
    EXPECT_FALSE(m.load("/tmp/aura_nonexistent_dual_xyz.txt"));
    EXPECT_TRUE(m.empty());
}

TEST_F(DualMapperTest, EmptyFileIsEmpty) {
    auto m = load("");
    EXPECT_TRUE(m.empty());
}

TEST_F(DualMapperTest, LoadsValidKeyCombo) {
    auto m = load("FIST + FIST = KEY_COMBO CMD Z\n");
    EXPECT_FALSE(m.empty());
    auto a = m.lookup(GestureType::FIST, GestureType::FIST);
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(a->type, ActionType::KEY_COMBO);
    ASSERT_EQ(a->params.size(), 2u);
    EXPECT_EQ(a->params[0], "CMD");
    EXPECT_EQ(a->params[1], "Z");
}

TEST_F(DualMapperTest, LoadsMouseClickAction) {
    auto m = load("PINCH + FIST = MOUSE_CLICK LEFT\n");
    auto a = m.lookup(GestureType::PINCH, GestureType::FIST);
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(a->type, ActionType::MOUSE_CLICK);
    ASSERT_EQ(a->params.size(), 1u);
    EXPECT_EQ(a->params[0], "LEFT");
}

TEST_F(DualMapperTest, LoadsScrollAction) {
    auto m = load("TWO_FINGERS + TWO_FINGERS = SCROLL UP 5\n");
    auto a = m.lookup(GestureType::TWO_FINGERS, GestureType::TWO_FINGERS);
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(a->type, ActionType::SCROLL);
    ASSERT_EQ(a->params.size(), 2u);
    EXPECT_EQ(a->params[0], "UP");
    EXPECT_EQ(a->params[1], "5");
}

TEST_F(DualMapperTest, ThreeParamKeyCombo) {
    auto m = load("FIST + OPEN_PALM = KEY_COMBO CMD SHIFT Z\n");
    auto a = m.lookup(GestureType::FIST, GestureType::OPEN_PALM);
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(a->type, ActionType::KEY_COMBO);
    ASSERT_EQ(a->params.size(), 3u);
    EXPECT_EQ(a->params[0], "CMD");
    EXPECT_EQ(a->params[1], "SHIFT");
    EXPECT_EQ(a->params[2], "Z");
}

TEST_F(DualMapperTest, LoadsMultipleBindings) {
    auto m = load(
        "FIST + FIST             = KEY_COMBO CMD Z\n"
        "PINCH + PINCH           = KEY_COMBO CMD C\n"
        "OPEN_PALM + OPEN_PALM   = KEY_COMBO CMD A\n"
        "PINCH_DOUBLE + PINCH_DOUBLE = KEY_COMBO CMD V\n"
    );
    EXPECT_TRUE(m.lookup(GestureType::FIST,         GestureType::FIST).has_value());
    EXPECT_TRUE(m.lookup(GestureType::PINCH,        GestureType::PINCH).has_value());
    EXPECT_TRUE(m.lookup(GestureType::OPEN_PALM,    GestureType::OPEN_PALM).has_value());
    EXPECT_TRUE(m.lookup(GestureType::PINCH_DOUBLE, GestureType::PINCH_DOUBLE).has_value());
    EXPECT_EQ(m.lookup(GestureType::PINCH, GestureType::PINCH)->params[0], "CMD");
    EXPECT_EQ(m.lookup(GestureType::PINCH, GestureType::PINCH)->params[1], "C");
}

// ---------------------------------------------------------------------------
// Lookup
// ---------------------------------------------------------------------------

TEST_F(DualMapperTest, UnknownComboReturnsNullopt) {
    auto m = load("FIST + FIST = KEY_COMBO CMD Z\n");
    EXPECT_FALSE(m.lookup(GestureType::PINCH, GestureType::FIST).has_value());
    EXPECT_FALSE(m.lookup(GestureType::FIST,  GestureType::PINCH).has_value());
    EXPECT_FALSE(m.lookup(GestureType::NONE,  GestureType::NONE).has_value());
}

TEST_F(DualMapperTest, ComboIsOrderSensitive) {
    // FIST+OPEN_PALM et OPEN_PALM+FIST sont deux bindings distincts
    auto m = load(
        "FIST + OPEN_PALM  = KEY_COMBO CMD S\n"
        "OPEN_PALM + FIST  = KEY_COMBO CMD Z\n"
    );
    auto a = m.lookup(GestureType::FIST, GestureType::OPEN_PALM);
    auto b = m.lookup(GestureType::OPEN_PALM, GestureType::FIST);
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(a->params[1], "S");
    EXPECT_EQ(b->params[1], "Z");
}

TEST_F(DualMapperTest, LookupWithoutMirrorDoesNotMatch) {
    auto m = load("FIST + OPEN_PALM = KEY_COMBO CMD S\n");
    EXPECT_TRUE( m.lookup(GestureType::FIST,      GestureType::OPEN_PALM).has_value());
    EXPECT_FALSE(m.lookup(GestureType::OPEN_PALM, GestureType::FIST).has_value());
}

// ---------------------------------------------------------------------------
// Parsing robustesse
// ---------------------------------------------------------------------------

TEST_F(DualMapperTest, IgnoresComments) {
    auto m = load(
        "# binding undo\n"
        "FIST + FIST = KEY_COMBO CMD Z  # inline\n"
        "\n"
        "PINCH + PINCH = KEY_COMBO CMD C\n"
    );
    EXPECT_TRUE(m.lookup(GestureType::FIST,  GestureType::FIST).has_value());
    EXPECT_TRUE(m.lookup(GestureType::PINCH, GestureType::PINCH).has_value());
}

TEST_F(DualMapperTest, SkipsLinesWithoutPlus) {
    auto m = load(
        "FIST FIST = KEY_COMBO CMD Z\n"   // pas de +
        "PINCH + PINCH = KEY_COMBO CMD C\n"
    );
    EXPECT_FALSE(m.lookup(GestureType::FIST,  GestureType::FIST).has_value());
    EXPECT_TRUE( m.lookup(GestureType::PINCH, GestureType::PINCH).has_value());
}

TEST_F(DualMapperTest, SkipsLinesWithUnknownGesture) {
    auto m = load(
        "GESTURE_INCONNU + FIST = KEY_COMBO CMD Z\n"
        "PINCH + PINCH = KEY_COMBO CMD C\n"
    );
    EXPECT_TRUE(m.lookup(GestureType::PINCH, GestureType::PINCH).has_value());
}

TEST_F(DualMapperTest, SkipsLinesWithUnknownAction) {
    auto m = load(
        "FIST + FIST = ACTION_INCONNUE XYZ\n"
        "PINCH + PINCH = KEY_COMBO CMD C\n"
    );
    EXPECT_FALSE(m.lookup(GestureType::FIST,  GestureType::FIST).has_value());
    EXPECT_TRUE( m.lookup(GestureType::PINCH, GestureType::PINCH).has_value());
}

TEST_F(DualMapperTest, AllGestureTypesUsableAsKeys) {
    auto m = load(
        "OPEN_PALM + OPEN_PALM       = KEY_COMBO CMD A\n"
        "FOUR_FINGERS + FOUR_FINGERS = KEY_COMBO CMD TAB\n"
        "POINT + POINT               = MOUSE_CLICK LEFT\n"
        "PINCH_RING + PINCH_RING     = KEY_PRESS ESCAPE\n"
        "PINCH_PINKY + PINCH_PINKY   = KEY_PRESS BACKSPACE\n"
        "PINCH_SIDE + PINCH_SIDE     = KEY_COMBO CMD V\n"
        "ZTAP + ZTAP                 = KEY_PRESS ESCAPE\n"
        "THREE_FINGERS + THREE_FINGERS = KEY_PRESS SPACE\n"
    );
    EXPECT_TRUE(m.lookup(GestureType::OPEN_PALM,     GestureType::OPEN_PALM).has_value());
    EXPECT_TRUE(m.lookup(GestureType::FOUR_FINGERS,  GestureType::FOUR_FINGERS).has_value());
    EXPECT_TRUE(m.lookup(GestureType::POINT,         GestureType::POINT).has_value());
    EXPECT_TRUE(m.lookup(GestureType::PINCH_RING,    GestureType::PINCH_RING).has_value());
    EXPECT_TRUE(m.lookup(GestureType::PINCH_PINKY,   GestureType::PINCH_PINKY).has_value());
    EXPECT_TRUE(m.lookup(GestureType::PINCH_SIDE,    GestureType::PINCH_SIDE).has_value());
    EXPECT_TRUE(m.lookup(GestureType::ZTAP,          GestureType::ZTAP).has_value());
    EXPECT_TRUE(m.lookup(GestureType::THREE_FINGERS,  GestureType::THREE_FINGERS).has_value());
}
