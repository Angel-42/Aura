#pragma once
#include "Aura/App/AuraRunner.hpp"
#include "Aura/Core/GestureEvent.hpp"
#include <SFML/Graphics.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace Aura::Demo {

// Action proposée dans le menu de rebind
struct ActionEntry {
    std::string label;    // Texte affiché : "Clic gauche"
    std::string binding;  // Valeur dans le profil : "MOUSE_CLICK LEFT"
};

struct ActionCategory {
    std::string              label;
    std::vector<ActionEntry> entries;
};

// Bouton cliquable dans le panel rebind
struct RebindButton {
    sf::FloatRect rect;
    ActionEntry   action;
};

enum class RebindStep { IDLE, WAITING_GESTURE, WAITING_ACTION };

// ─────────────────────────────────────────────────────────────────────────────

class DemoApp {
public:
    explicit DemoApp(App::AuraRunner& runner);
    void run();

private:
    App::AuraRunner& runner_;
    sf::RenderWindow window_;
    sf::Font         font_;
    bool             fontLoaded_ = false;

    // ── Cibles ────────────────────────────────────────────────────────────────
    struct Target {
        sf::Vector2f pos;
        float        radius    = 30.f;
        sf::Color    color;
        bool         alive     = true;
        float        hoverScale = 1.f;
        float        deathTimer = 0.f;
    };
    std::vector<Target> targets_;
    int score_ = 0;

    // ── Flash geste (HUD) ─────────────────────────────────────────────────────
    std::string     lastGestureName_;
    std::string     lastSide_;
    sf::Color       lastSideColor_      = sf::Color::White;
    sf::Clock       gestureFlash_;
    bool            gestureFlashActive_ = false;

    // ── Live rebind ───────────────────────────────────────────────────────────
    RebindStep               rebindStep_    = RebindStep::IDLE;
    Core::GestureType        capturedGesture_ = Core::GestureType::NONE;
    std::vector<RebindButton> rebindButtons_;   // généré à l'entrée de WAITING_ACTION
    sf::FloatRect            rebindCancelRect_;
    sf::FloatRect            rebindBtnHudRect_; // bouton HUD "Rebind"
    bool                     savedVisible_  = false;
    sf::Clock                savedClock_;

    // ── Helpers ───────────────────────────────────────────────────────────────
    bool loadFont();
    void spawnTargets(int count);

    // Boucle
    void processGestureQueue();
    void handleClick(sf::Vector2i pos);
    void update(float dt);
    void render();
    void drawHUD();

    // Rebind
    void startRebind();
    void cancelRebind();
    void buildRebindButtons();
    void applyBinding(const ActionEntry& entry);
    void drawRebindPanel();

    static void upsertBinding(const std::filesystem::path& profilePath,
                              const std::string& gesture,
                              const std::string& actionStr);
    static const std::vector<ActionCategory>& actionCategories();
};

} // namespace Aura::Demo
