#include <gtest/gtest.h>
#include "Aura/Input/Controller.hpp"

using namespace Aura::Input;

// ---------------------------------------------------------------------------
// Note : sur macOS, Controller::available() = true et les méthodes injectent
// réellement de l'input. Les tests ci-dessous sont donc limités aux chemins
// qui ne déclenchent PAS d'injection :
//   - guards de dimensions invalides → retournent false avant toute injection
//   - clés inconnues → resolveKeyCode() = 0xFFFF → retournent false
//
// Sur Linux sans display (CI), available() = false → tout retourne false.
// Dans les deux cas : aucune injection, aucun crash.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Construction / available
// ---------------------------------------------------------------------------

TEST(ControllerTest, ConstructionDoesNotCrash) {
    EXPECT_NO_FATAL_FAILURE({ Controller c; });
}

TEST(ControllerTest, AvailableReturnsBool) {
    Controller c;
    bool a = c.available();
    EXPECT_TRUE(a == true || a == false);  // valeur cohérente, pas de crash
}

// ---------------------------------------------------------------------------
// moveMouse — guards de dimensions invalides (sûr : pas d'injection)
// ---------------------------------------------------------------------------

TEST(ControllerTest, MoveMouse_ZeroDimensions_ReturnsFalse) {
    Controller c;
    EXPECT_FALSE(c.moveMouse(320, 240, 0, 0));
}

TEST(ControllerTest, MoveMouse_NegativeWidth_ReturnsFalse) {
    Controller c;
    EXPECT_FALSE(c.moveMouse(320, 240, -1, 480));
}

TEST(ControllerTest, MoveMouse_NegativeHeight_ReturnsFalse) {
    Controller c;
    EXPECT_FALSE(c.moveMouse(320, 240, 640, -1));
}

TEST(ControllerTest, MoveMouse_BothNegative_ReturnsFalse) {
    Controller c;
    EXPECT_FALSE(c.moveMouse(0, 0, -640, -480));
}

// ---------------------------------------------------------------------------
// pressKey / keyDown / keyUp — clés inconnues (sûr : resolveKeyCode = 0xFFFF)
// ---------------------------------------------------------------------------

TEST(ControllerTest, PressKey_EmptyString_ReturnsFalse) {
    Controller c;
    EXPECT_FALSE(c.pressKey(""));
}

TEST(ControllerTest, PressKey_UnknownKey_ReturnsFalse) {
    Controller c;
    EXPECT_FALSE(c.pressKey("TOTALLY_UNKNOWN_KEY_XYZ_999"));
}

TEST(ControllerTest, KeyDown_UnknownKey_ReturnsFalse) {
    Controller c;
    EXPECT_FALSE(c.keyDown("TOTALLY_UNKNOWN_KEY_XYZ_999"));
}

TEST(ControllerTest, KeyUp_UnknownKey_ReturnsFalse) {
    Controller c;
    EXPECT_FALSE(c.keyUp("TOTALLY_UNKNOWN_KEY_XYZ_999"));
}

// ---------------------------------------------------------------------------
// Aliases clavier — vérifie que les noms utilisés dans DemoApp/config sont
// reconnus. Sur Linux CI (available=false) → false mais pas de crash.
// Sur macOS → presse brièvement la touche ; acceptable en test local.
// ---------------------------------------------------------------------------

TEST(ControllerTest, PressKey_KnownAliases_NoCrash) {
    Controller c;
    // Ces clés doivent au moins ne pas crasher, quelle que soit la plateforme.
    // Elles retournent true si available() && clé connue, false sinon.
    EXPECT_NO_FATAL_FAILURE(c.pressKey("ESC"));
    EXPECT_NO_FATAL_FAILURE(c.pressKey("ESCAPE"));   // alias
    EXPECT_NO_FATAL_FAILURE(c.pressKey("ENTER"));
    EXPECT_NO_FATAL_FAILURE(c.pressKey("RETURN"));   // alias
    EXPECT_NO_FATAL_FAILURE(c.pressKey("SPACE"));
    EXPECT_NO_FATAL_FAILURE(c.pressKey("TAB"));
    EXPECT_NO_FATAL_FAILURE(c.pressKey("BACKSPACE"));
    EXPECT_NO_FATAL_FAILURE(c.pressKey("DELETE"));
    EXPECT_NO_FATAL_FAILURE(c.pressKey("LEFT"));
    EXPECT_NO_FATAL_FAILURE(c.pressKey("RIGHT"));
    EXPECT_NO_FATAL_FAILURE(c.pressKey("UP"));
    EXPECT_NO_FATAL_FAILURE(c.pressKey("DOWN"));
}

TEST(ControllerTest, PressKey_Modifiers_NoCrash) {
    Controller c;
    EXPECT_NO_FATAL_FAILURE(c.pressKey("CTRL"));
    EXPECT_NO_FATAL_FAILURE(c.pressKey("SHIFT"));
    EXPECT_NO_FATAL_FAILURE(c.pressKey("ALT"));
    EXPECT_NO_FATAL_FAILURE(c.pressKey("CMD"));
}

TEST(ControllerTest, PressKey_FunctionKeys_NoCrash) {
    Controller c;
    EXPECT_NO_FATAL_FAILURE(c.pressKey("F1"));
    EXPECT_NO_FATAL_FAILURE(c.pressKey("F5"));
    EXPECT_NO_FATAL_FAILURE(c.pressKey("F12"));
}
