#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include "Aura/Input/Mapper.hpp"
#include "Aura/Core/GestureEvent.hpp"

using namespace Aura::Input;
using namespace Aura::Core;

// ---------------------------------------------------------------------------
// Fixture — écrit un fichier de mapping temporaire, le supprime après chaque test
// ---------------------------------------------------------------------------

class MapperTest : public ::testing::Test {
protected:
    std::filesystem::path tmpFile;

    void SetUp() override {
        tmpFile = std::filesystem::temp_directory_path() / "aura_test_mapping.txt";
    }
    void TearDown() override {
        std::filesystem::remove(tmpFile);
    }

    Mapper load(const std::string& content) {
        {
            std::ofstream f(tmpFile);
            f << content;
        } // destructor flushes and closes before m.load()
        Mapper m;
        m.load(tmpFile);
        return m;
    }
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_F(MapperTest, UnmappedGestureReturnsNone) {
    Mapper m = load("PINCH = MOUSE_CLICK LEFT\n");
    EXPECT_EQ(m.actionFor(GestureType::FIST).type, ActionType::NONE);
}

TEST_F(MapperTest, MouseClickLeft) {
    Mapper m = load("PINCH = MOUSE_CLICK LEFT\n");
    Action a = m.actionFor(GestureType::PINCH);
    EXPECT_EQ(a.type, ActionType::MOUSE_CLICK);
    ASSERT_EQ(a.params.size(), 1u);
    EXPECT_EQ(a.params[0], "LEFT");
}

TEST_F(MapperTest, MouseClickRight) {
    Mapper m = load("PINCH_MIDDLE = MOUSE_CLICK RIGHT\n");
    Action a = m.actionFor(GestureType::PINCH_MIDDLE);
    EXPECT_EQ(a.type, ActionType::MOUSE_CLICK);
    EXPECT_EQ(a.params[0], "RIGHT");
}

TEST_F(MapperTest, MouseDoubleClick) {
    Mapper m = load("FOUR_FINGERS = MOUSE_DOUBLE_CLICK LEFT\n");
    Action a = m.actionFor(GestureType::FOUR_FINGERS);
    EXPECT_EQ(a.type, ActionType::MOUSE_DOUBLE_CLICK);
    EXPECT_EQ(a.params[0], "LEFT");
}

TEST_F(MapperTest, KeyPress) {
    Mapper m = load("THREE_FINGERS = KEY_PRESS SPACE\n");
    Action a = m.actionFor(GestureType::THREE_FINGERS);
    EXPECT_EQ(a.type, ActionType::KEY_PRESS);
    ASSERT_EQ(a.params.size(), 1u);
    EXPECT_EQ(a.params[0], "SPACE");
}

TEST_F(MapperTest, KeyComboTwoParams) {
    Mapper m = load("SWIPE_LEFT = KEY_COMBO ALT LEFT\n");
    Action a = m.actionFor(GestureType::SWIPE_LEFT);
    EXPECT_EQ(a.type, ActionType::KEY_COMBO);
    ASSERT_EQ(a.params.size(), 2u);
    EXPECT_EQ(a.params[0], "ALT");
    EXPECT_EQ(a.params[1], "LEFT");
}

TEST_F(MapperTest, KeyComboThreeParams) {
    Mapper m = load("PINCH_DOUBLE = KEY_COMBO CTRL SHIFT S\n");
    Action a = m.actionFor(GestureType::PINCH_DOUBLE);
    EXPECT_EQ(a.type, ActionType::KEY_COMBO);
    ASSERT_EQ(a.params.size(), 3u);
    EXPECT_EQ(a.params[0], "CTRL");
    EXPECT_EQ(a.params[1], "SHIFT");
    EXPECT_EQ(a.params[2], "S");
}

TEST_F(MapperTest, ScrollWithAmount) {
    Mapper m = load("SWIPE_UP = SCROLL UP 5\n");
    Action a = m.actionFor(GestureType::SWIPE_UP);
    EXPECT_EQ(a.type, ActionType::SCROLL);
    ASSERT_EQ(a.params.size(), 2u);
    EXPECT_EQ(a.params[0], "UP");
    EXPECT_EQ(a.params[1], "5");
}

TEST_F(MapperTest, IgnoresCommentsAndBlankLines) {
    Mapper m = load(
        "# commentaire\n"
        "\n"
        "PINCH = MOUSE_CLICK LEFT\n"
        "  # autre commentaire\n"
        "ZTAP = MOUSE_CLICK LEFT\n"
    );
    EXPECT_EQ(m.actionFor(GestureType::PINCH).type, ActionType::MOUSE_CLICK);
    EXPECT_EQ(m.actionFor(GestureType::ZTAP).type, ActionType::MOUSE_CLICK);
    EXPECT_EQ(m.actionFor(GestureType::FIST).type, ActionType::NONE);
}

TEST_F(MapperTest, AllPinchVariantsRecognized) {
    Mapper m = load(
        "PINCH        = MOUSE_CLICK LEFT\n"
        "PINCH_MIDDLE = MOUSE_CLICK RIGHT\n"
        "PINCH_RING   = MOUSE_CLICK MIDDLE\n"
        "PINCH_PINKY  = MOUSE_DOUBLE_CLICK LEFT\n"
        "PINCH_DOUBLE = KEY_COMBO CTRL C\n"
        "PINCH_SIDE   = KEY_COMBO CTRL V\n"
    );
    EXPECT_EQ(m.actionFor(GestureType::PINCH).type,        ActionType::MOUSE_CLICK);
    EXPECT_EQ(m.actionFor(GestureType::PINCH_MIDDLE).type, ActionType::MOUSE_CLICK);
    EXPECT_EQ(m.actionFor(GestureType::PINCH_RING).type,   ActionType::MOUSE_CLICK);
    EXPECT_EQ(m.actionFor(GestureType::PINCH_PINKY).type,  ActionType::MOUSE_DOUBLE_CLICK);
    EXPECT_EQ(m.actionFor(GestureType::PINCH_DOUBLE).type, ActionType::KEY_COMBO);
    EXPECT_EQ(m.actionFor(GestureType::PINCH_SIDE).type,   ActionType::KEY_COMBO);
}

TEST_F(MapperTest, AllSwipesRecognized) {
    Mapper m = load(
        "SWIPE_UP    = SCROLL UP 3\n"
        "SWIPE_DOWN  = SCROLL DOWN 3\n"
        "SWIPE_LEFT  = KEY_COMBO ALT LEFT\n"
        "SWIPE_RIGHT = KEY_COMBO ALT RIGHT\n"
    );
    EXPECT_EQ(m.actionFor(GestureType::SWIPE_UP).type,    ActionType::SCROLL);
    EXPECT_EQ(m.actionFor(GestureType::SWIPE_DOWN).type,  ActionType::SCROLL);
    EXPECT_EQ(m.actionFor(GestureType::SWIPE_LEFT).type,  ActionType::KEY_COMBO);
    EXPECT_EQ(m.actionFor(GestureType::SWIPE_RIGHT).type, ActionType::KEY_COMBO);
}

TEST_F(MapperTest, LoadNonExistentFileReturnsFalse) {
    Mapper m;
    EXPECT_FALSE(m.load("/tmp/aura_nonexistent_file_xyz.txt"));
    EXPECT_FALSE(m.isLoaded());
}
