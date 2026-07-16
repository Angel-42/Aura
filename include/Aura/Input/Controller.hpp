#pragma once
#include <string>

namespace Aura::Input {

enum class MouseButton { Left, Right, Middle };

#ifdef __linux__
struct _XDisplay;
typedef _XDisplay Display;
#endif

class Controller {
public:
    Controller();
    ~Controller();

    Controller(const Controller&) = delete;
    Controller& operator=(const Controller&) = delete;

    [[nodiscard]] bool available() const noexcept;

    // Souris
    bool moveMouse(int frameX, int frameY, int frameW, int frameH);
    bool click(MouseButton button);
    bool doubleClick(MouseButton button);
    bool mouseDown(MouseButton button);
    bool mouseUp(MouseButton button);

    // Scroll : positif = haut/droite, négatif = bas/gauche
    bool scroll(int deltaY, int deltaX = 0);

    // Clavier
    bool pressKey(const std::string& keyName);   // down + up
    bool keyDown(const std::string& keyName);
    bool keyUp(const std::string& keyName);

private:
    bool available_;

#ifdef __linux__
    Display* display_;
    bool sendKey(const std::string& keyName, bool down);
#elif defined(__APPLE__)
    bool sendKey(const std::string& keyName, bool down);
    static unsigned short resolveKeyCode(const std::string& keyName);
#endif
};

} // namespace Aura::Input
