#include "Aura/Input/Controller.hpp"
#include <iostream>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <thread>
#include <chrono>

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#endif

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace Aura::Input {

// --------------------------------------------------------------------------
// Normalisation des noms de touches vers X11 KeySym strings
// --------------------------------------------------------------------------
#ifdef __linux__
static std::string toX11KeyName(const std::string& name) {
    static const std::unordered_map<std::string, std::string> table = {
        {"SPACE","space"},{"ENTER","Return"},{"ESC","Escape"},{"TAB","Tab"},
        {"LEFT","Left"},{"RIGHT","Right"},{"UP","Up"},{"DOWN","Down"},
        {"BACKSPACE","BackSpace"},{"DELETE","Delete"},
        {"F1","F1"},{"F2","F2"},{"F3","F3"},{"F4","F4"},{"F5","F5"},
        {"F6","F6"},{"F7","F7"},{"F8","F8"},{"F9","F9"},{"F10","F10"},
        {"F11","F11"},{"F12","F12"},
        {"CTRL","Control_L"},{"SHIFT","Shift_L"},{"ALT","Alt_L"},
        {"META","Super_L"},{"WIN","Super_L"},
    };
    auto it = table.find(name);
    if (it != table.end()) return it->second;
    // Pour les lettres et chiffres, X11 veut le caractère en minuscule
    if (name.size() == 1) {
        std::string lower = name;
        lower[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(lower[0])));
        return lower;
    }
    return name;
}
#endif

// --------------------------------------------------------------------------
// Table CGKeyCode pour macOS (layout US QWERTY)
// --------------------------------------------------------------------------
#ifdef __APPLE__
unsigned short Controller::resolveKeyCode(const std::string& name) {
    static const std::unordered_map<std::string, unsigned short> table = {
        {"A",0},{"S",1},{"D",2},{"F",3},{"H",4},{"G",5},{"Z",6},{"X",7},
        {"C",8},{"V",9},{"B",11},{"Q",12},{"W",13},{"E",14},{"R",15},
        {"Y",16},{"T",17},{"1",18},{"2",19},{"3",20},{"4",21},{"6",22},
        {"5",23},{"9",25},{"7",26},{"8",28},{"0",29},{"O",31},{"U",32},
        {"I",34},{"P",35},{"L",37},{"J",38},{"K",40},{"N",45},{"M",46},
        {"ENTER",36},{"TAB",48},{"SPACE",49},{"BACKSPACE",51},{"ESC",53},
        {"LEFT",123},{"RIGHT",124},{"DOWN",125},{"UP",126},{"DELETE",117},
        {"F1",122},{"F2",120},{"F3",99},{"F4",118},{"F5",96},{"F6",97},
        {"F7",98},{"F8",100},{"F9",101},{"F10",109},{"F11",103},{"F12",111},
        {"CTRL",59},{"SHIFT",56},{"ALT",58},{"CMD",55},{"META",55},{"WIN",55},
    };
    auto it = table.find(name);
    return (it != table.end()) ? it->second : 0xFFFF;
}

// Retourne le flag CGEventFlags correspondant à une touche modificatrice
uint64_t Controller::modifierFlagFor(const std::string& name) {
    if (name == "CTRL")                               return kCGEventFlagMaskControl;
    if (name == "SHIFT")                              return kCGEventFlagMaskShift;
    if (name == "ALT")                                return kCGEventFlagMaskAlternate;
    if (name == "CMD" || name == "META" || name == "WIN") return kCGEventFlagMaskCommand;
    return 0;
}
#endif

// --------------------------------------------------------------------------
// Constructor / Destructor
// --------------------------------------------------------------------------
Controller::Controller() : available_(false)
#ifdef __linux__
    , display_(nullptr)
#endif
{
#ifdef __linux__
    display_ = XOpenDisplay(nullptr);
    available_ = (display_ != nullptr);
    if (!available_)
        std::cerr << "[Controller] Cannot open X display — input disabled.\n";
#elif defined(__APPLE__)
    available_ = true;
#else
    std::cerr << "[Controller] Unsupported platform — input disabled.\n";
#endif
}

Controller::~Controller() {
#ifdef __linux__
    if (display_) { XCloseDisplay(display_); display_ = nullptr; }
#endif
}

bool Controller::available() const noexcept { return available_; }

// --------------------------------------------------------------------------
// Souris — déplacement
// --------------------------------------------------------------------------
bool Controller::moveMouse(int frameX, int frameY, int frameW, int frameH) {
    if (!available_ || frameW <= 0 || frameH <= 0) return false;

#ifdef __linux__
    int sw = DisplayWidth(display_, DefaultScreen(display_));
    int sh = DisplayHeight(display_, DefaultScreen(display_));
    int x = (frameW - 1 - frameX) * sw / frameW;
    int y = frameY * sh / frameH;
    XTestFakeMotionEvent(display_, DefaultScreen(display_), x, y, CurrentTime);
    XFlush(display_);
    return true;
#elif defined(__APPLE__)
    int sw = static_cast<int>(CGDisplayPixelsWide(CGMainDisplayID()));
    int sh = static_cast<int>(CGDisplayPixelsHigh(CGMainDisplayID()));
    int x = (frameW - 1 - frameX) * sw / frameW;
    int y = frameY * sh / frameH;
    CGWarpMouseCursorPosition(CGPointMake(x, y));
    CGAssociateMouseAndMouseCursorPosition(true);
    return true;
#else
    (void)frameX; (void)frameY; return false;
#endif
}

// --------------------------------------------------------------------------
// Souris — boutons
// --------------------------------------------------------------------------
#ifdef __linux__
static int linuxButton(MouseButton b) {
    switch (b) {
        case MouseButton::Right:  return 3;
        case MouseButton::Middle: return 2;
        default:                  return 1;
    }
}
#endif

#ifdef __APPLE__
static std::pair<CGEventType,CGMouseButton> appleButton(MouseButton b, bool down) {
    switch (b) {
        case MouseButton::Right:
            return {down ? kCGEventRightMouseDown : kCGEventRightMouseUp, kCGMouseButtonRight};
        case MouseButton::Middle:
            return {down ? kCGEventOtherMouseDown : kCGEventOtherMouseUp, kCGMouseButtonCenter};
        default:
            return {down ? kCGEventLeftMouseDown : kCGEventLeftMouseUp, kCGMouseButtonLeft};
    }
}
#endif

bool Controller::mouseDown(MouseButton button) {
    if (!available_) return false;
#ifdef __linux__
    XTestFakeButtonEvent(display_, linuxButton(button), True, CurrentTime);
    XFlush(display_);
    return true;
#elif defined(__APPLE__)
    auto [evType, btn] = appleButton(button, true);
    CGEventRef ev = CGEventCreate(nullptr);
    CGPoint loc = ev ? CGEventGetLocation(ev) : CGPointMake(0, 0);
    if (ev) CFRelease(ev);
    CGEventRef down = CGEventCreateMouseEvent(nullptr, evType, loc, btn);
    if (!down) return false;
    CGEventPost(kCGHIDEventTap, down);
    CFRelease(down);
    return true;
#else
    return false;
#endif
}

bool Controller::mouseUp(MouseButton button) {
    if (!available_) return false;
#ifdef __linux__
    XTestFakeButtonEvent(display_, linuxButton(button), False, CurrentTime);
    XFlush(display_);
    return true;
#elif defined(__APPLE__)
    auto [evType, btn] = appleButton(button, false);
    CGEventRef ev = CGEventCreate(nullptr);
    CGPoint loc = ev ? CGEventGetLocation(ev) : CGPointMake(0, 0);
    if (ev) CFRelease(ev);
    CGEventRef up = CGEventCreateMouseEvent(nullptr, evType, loc, btn);
    if (!up) return false;
    CGEventPost(kCGHIDEventTap, up);
    CFRelease(up);
    return true;
#else
    return false;
#endif
}

bool Controller::click(MouseButton button) {
    return mouseDown(button) && mouseUp(button);
}

bool Controller::doubleClick(MouseButton button) {
    if (!click(button)) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    return click(button);
}

// --------------------------------------------------------------------------
// Scroll
// --------------------------------------------------------------------------
bool Controller::scroll(int deltaY, int deltaX) {
    if (!available_) return false;
#ifdef __linux__
    // Button 4/5 = scroll vertical, 6/7 = scroll horizontal
    auto fire = [&](int btn) {
        XTestFakeButtonEvent(display_, btn, True,  CurrentTime);
        XTestFakeButtonEvent(display_, btn, False, CurrentTime);
    };
    for (int i = 0; i < std::abs(deltaY); ++i) fire(deltaY > 0 ? 4 : 5);
    for (int i = 0; i < std::abs(deltaX); ++i) fire(deltaX > 0 ? 6 : 7);
    XFlush(display_);
    return true;
#elif defined(__APPLE__)
    // CGScrollWheelEvent: (source, unit, wheelCount, wheel1=vertical, wheel2=horizontal)
    CGEventRef ev = CGEventCreateScrollWheelEvent(
        nullptr, kCGScrollEventUnitLine, 2,
        static_cast<int32_t>(deltaY),
        static_cast<int32_t>(deltaX));
    if (!ev) return false;
    CGEventPost(kCGHIDEventTap, ev);
    CFRelease(ev);
    return true;
#else
    (void)deltaY; (void)deltaX; return false;
#endif
}

// --------------------------------------------------------------------------
// Clavier
// --------------------------------------------------------------------------
#ifdef __linux__
bool Controller::sendKey(const std::string& keyName, bool down) {
    std::string x11name = toX11KeyName(keyName);
    KeySym sym = XStringToKeysym(x11name.c_str());
    if (sym == NoSymbol) {
        std::cerr << "[Controller] Unknown key: " << keyName << "\n";
        return false;
    }
    KeyCode code = XKeysymToKeycode(display_, sym);
    if (code == 0) return false;
    XTestFakeKeyEvent(display_, code, down ? True : False, CurrentTime);
    XFlush(display_);
    return true;
}
#elif defined(__APPLE__)
bool Controller::sendKey(const std::string& keyName, bool down) {
    unsigned short code = resolveKeyCode(keyName);
    if (code == 0xFFFF) {
        std::cerr << "[Controller] Unknown key: " << keyName << "\n";
        return false;
    }

    // Mettre à jour le suivi des modificateurs actifs
    uint64_t flag = modifierFlagFor(keyName);
    if (flag) {
        if (down) modifierFlags_ |=  flag;
        else      modifierFlags_ &= ~flag;
    }

    CGEventRef ev = CGEventCreateKeyboardEvent(nullptr, static_cast<CGKeyCode>(code), down);
    if (!ev) return false;
    // Attacher les flags de modificateurs à l'événement — indispensable pour KEY_COMBO
    CGEventSetFlags(ev, static_cast<CGEventFlags>(modifierFlags_));
    CGEventPost(kCGHIDEventTap, ev);
    CFRelease(ev);
    return true;
}
#endif

bool Controller::pressKey(const std::string& keyName) {
    if (!available_) return false;
#if defined(__linux__) || defined(__APPLE__)
    return sendKey(keyName, true) && sendKey(keyName, false);
#else
    return false;
#endif
}

bool Controller::keyDown(const std::string& keyName) {
    if (!available_) return false;
#if defined(__linux__) || defined(__APPLE__)
    return sendKey(keyName, true);
#else
    return false;
#endif
}

bool Controller::keyUp(const std::string& keyName) {
    if (!available_) return false;
#if defined(__linux__) || defined(__APPLE__)
    return sendKey(keyName, false);
#else
    return false;
#endif
}

} // namespace Aura::Input
