#pragma once
#include "Aura/App/AuraRunner.hpp"
#include "Aura/Core/GestureEvent.hpp"
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

namespace Aura::Demo {

class DemoApp {
public:
    explicit DemoApp(App::AuraRunner& runner);
    void run();

private:
    App::AuraRunner& runner_;
    sf::RenderWindow window_;
    sf::Font         font_;
    bool             fontLoaded_ = false;

    struct Target {
        sf::Vector2f pos;
        float        radius    = 30.f;
        sf::Color    color;
        bool         alive     = true;
        float        hoverScale = 1.f;
        float        deathTimer = 0.f;  // >0 → animation de mort
    };
    std::vector<Target> targets_;
    int score_ = 0;

    // Dernière info geste (depuis EventQueue)
    std::string     lastGestureName_ = "";
    std::string     lastSide_        = "";
    sf::Color       lastSideColor_   = sf::Color::White;
    sf::Clock       gestureFlash_;
    bool            gestureFlashActive_ = false;

    bool loadFont();
    void spawnTargets(int count);

    void processGestureQueue();
    void handleClick(sf::Vector2i pos);
    void update(float dt);
    void render();
    void drawHUD();
};

} // namespace Aura::Demo
