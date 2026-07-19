#include <gtest/gtest.h>
#include "Aura/Core/GestureEvent.hpp"

using namespace Aura::Core;

// ---------------------------------------------------------------------------
// gestureName()
// ---------------------------------------------------------------------------

TEST(GestureNameTest, AllTypesHaveNonEmptyName) {
    const GestureType types[] = {
        GestureType::NONE,   GestureType::OPEN_PALM,  GestureType::FOUR_FINGERS,
        GestureType::FIST,   GestureType::POINT,      GestureType::TWO_FINGERS,
        GestureType::THREE_FINGERS, GestureType::PINCH, GestureType::PINCH_MIDDLE,
        GestureType::PINCH_RING,  GestureType::PINCH_PINKY, GestureType::PINCH_DOUBLE,
        GestureType::PINCH_SIDE,  GestureType::ZTAP,
        GestureType::SWIPE_LEFT,  GestureType::SWIPE_RIGHT,
        GestureType::SWIPE_UP,    GestureType::SWIPE_DOWN,
    };
    for (auto t : types)
        EXPECT_FALSE(gestureName(t).empty()) << "Type " << static_cast<int>(t);
}

TEST(GestureNameTest, NamesAreUnique) {
    const GestureType types[] = {
        GestureType::NONE,   GestureType::OPEN_PALM,  GestureType::FOUR_FINGERS,
        GestureType::FIST,   GestureType::POINT,      GestureType::TWO_FINGERS,
        GestureType::THREE_FINGERS, GestureType::PINCH, GestureType::PINCH_MIDDLE,
        GestureType::PINCH_RING,  GestureType::PINCH_PINKY, GestureType::PINCH_DOUBLE,
        GestureType::PINCH_SIDE,  GestureType::ZTAP,
        GestureType::SWIPE_LEFT,  GestureType::SWIPE_RIGHT,
        GestureType::SWIPE_UP,    GestureType::SWIPE_DOWN,
    };
    std::set<std::string> seen;
    for (auto t : types) {
        auto name = gestureName(t);
        EXPECT_TRUE(seen.insert(name).second)
            << "Duplicate name: " << name;
    }
}

TEST(GestureNameTest, KnownNamesCorrect) {
    EXPECT_EQ(gestureName(GestureType::NONE),          "NONE");
    EXPECT_EQ(gestureName(GestureType::OPEN_PALM),     "OPEN_PALM");
    EXPECT_EQ(gestureName(GestureType::FIST),          "FIST");
    EXPECT_EQ(gestureName(GestureType::POINT),         "POINT");
    EXPECT_EQ(gestureName(GestureType::TWO_FINGERS),   "TWO_FINGERS");
    EXPECT_EQ(gestureName(GestureType::THREE_FINGERS), "THREE_FINGERS");
    EXPECT_EQ(gestureName(GestureType::FOUR_FINGERS),  "FOUR_FINGERS");
    EXPECT_EQ(gestureName(GestureType::PINCH),         "PINCH");
    EXPECT_EQ(gestureName(GestureType::PINCH_MIDDLE),  "PINCH_MIDDLE");
    EXPECT_EQ(gestureName(GestureType::PINCH_RING),    "PINCH_RING");
    EXPECT_EQ(gestureName(GestureType::PINCH_PINKY),   "PINCH_PINKY");
    EXPECT_EQ(gestureName(GestureType::PINCH_DOUBLE),  "PINCH_DOUBLE");
    EXPECT_EQ(gestureName(GestureType::PINCH_SIDE),    "PINCH_SIDE");
    EXPECT_EQ(gestureName(GestureType::ZTAP),          "ZTAP");
    EXPECT_EQ(gestureName(GestureType::SWIPE_LEFT),    "SWIPE_LEFT");
    EXPECT_EQ(gestureName(GestureType::SWIPE_RIGHT),   "SWIPE_RIGHT");
    EXPECT_EQ(gestureName(GestureType::SWIPE_UP),      "SWIPE_UP");
    EXPECT_EQ(gestureName(GestureType::SWIPE_DOWN),    "SWIPE_DOWN");
}

// ---------------------------------------------------------------------------
// gestureFromName()
// ---------------------------------------------------------------------------

TEST(GestureFromNameTest, KnownNamesMapped) {
    EXPECT_EQ(gestureFromName("OPEN_PALM"),     GestureType::OPEN_PALM);
    EXPECT_EQ(gestureFromName("FOUR_FINGERS"),  GestureType::FOUR_FINGERS);
    EXPECT_EQ(gestureFromName("FIST"),          GestureType::FIST);
    EXPECT_EQ(gestureFromName("POINT"),         GestureType::POINT);
    EXPECT_EQ(gestureFromName("TWO_FINGERS"),   GestureType::TWO_FINGERS);
    EXPECT_EQ(gestureFromName("THREE_FINGERS"), GestureType::THREE_FINGERS);
    EXPECT_EQ(gestureFromName("PINCH"),         GestureType::PINCH);
    EXPECT_EQ(gestureFromName("PINCH_MIDDLE"),  GestureType::PINCH_MIDDLE);
    EXPECT_EQ(gestureFromName("PINCH_RING"),    GestureType::PINCH_RING);
    EXPECT_EQ(gestureFromName("PINCH_PINKY"),   GestureType::PINCH_PINKY);
    EXPECT_EQ(gestureFromName("PINCH_DOUBLE"),  GestureType::PINCH_DOUBLE);
    EXPECT_EQ(gestureFromName("PINCH_SIDE"),    GestureType::PINCH_SIDE);
    EXPECT_EQ(gestureFromName("ZTAP"),          GestureType::ZTAP);
    EXPECT_EQ(gestureFromName("SWIPE_LEFT"),    GestureType::SWIPE_LEFT);
    EXPECT_EQ(gestureFromName("SWIPE_RIGHT"),   GestureType::SWIPE_RIGHT);
    EXPECT_EQ(gestureFromName("SWIPE_UP"),      GestureType::SWIPE_UP);
    EXPECT_EQ(gestureFromName("SWIPE_DOWN"),    GestureType::SWIPE_DOWN);
}

TEST(GestureFromNameTest, UnknownNameReturnsNone) {
    EXPECT_EQ(gestureFromName(""),               GestureType::NONE);
    EXPECT_EQ(gestureFromName("UNKNOWN_XYZ"),    GestureType::NONE);
    EXPECT_EQ(gestureFromName("pinch"),          GestureType::NONE);  // case-sensitive
    EXPECT_EQ(gestureFromName("Fist"),           GestureType::NONE);
    EXPECT_EQ(gestureFromName("SWIPE"),          GestureType::NONE);  // préfixe seul
    EXPECT_EQ(gestureFromName("PINCH_"),         GestureType::NONE);  // suffix manquant
}

TEST(GestureFromNameTest, NoneExplicit) {
    EXPECT_EQ(gestureFromName("NONE"), GestureType::NONE);
}

// ---------------------------------------------------------------------------
// Round-trip : gestureFromName(gestureName(t)) == t pour tous les types
// ---------------------------------------------------------------------------

TEST(GestureRoundTripTest, AllTypes) {
    const GestureType types[] = {
        GestureType::NONE,   GestureType::OPEN_PALM,  GestureType::FOUR_FINGERS,
        GestureType::FIST,   GestureType::POINT,      GestureType::TWO_FINGERS,
        GestureType::THREE_FINGERS, GestureType::PINCH, GestureType::PINCH_MIDDLE,
        GestureType::PINCH_RING,  GestureType::PINCH_PINKY, GestureType::PINCH_DOUBLE,
        GestureType::PINCH_SIDE,  GestureType::ZTAP,
        GestureType::SWIPE_LEFT,  GestureType::SWIPE_RIGHT,
        GestureType::SWIPE_UP,    GestureType::SWIPE_DOWN,
    };
    for (auto t : types) {
        auto name = gestureName(t);
        EXPECT_EQ(gestureFromName(name), t)
            << "Round-trip échoué pour : " << name;
    }
}
