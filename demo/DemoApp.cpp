#include "DemoApp.hpp"
#include <cmath>
#include <random>
#include <algorithm>

namespace Aura::Demo {

static constexpr int   kWinW    = 960;
static constexpr int   kWinH    = 620;
static constexpr float kTopBar  = 55.f;
static constexpr float kBotBar  = 30.f;
static constexpr int   kInitTargets = 7;
static constexpr float kFlashMs = 700.f;

// Palette de cibles
static const sf::Color kPalette[] = {
    {255,  90,  90}, // rouge
    { 80, 170, 255}, // bleu
    {100, 220,  90}, // vert
    {255, 200,  50}, // jaune
    {200,  90, 255}, // violet
    {255, 140,  50}, // orange
    { 50, 220, 200}, // cyan
};
static constexpr int kPaletteSize = sizeof(kPalette) / sizeof(kPalette[0]);

// Couleurs par side
static const sf::Color kColorLeft  = {255, 150,  50};  // orange
static const sf::Color kColorRight = { 80, 180, 255};  // bleu

// ── Construction ─────────────────────────────────────────────────────────────

DemoApp::DemoApp(App::AuraRunner& runner)
    : runner_(runner)
    , window_(sf::VideoMode({kWinW, kWinH}), "AURA Demo",
              sf::Style::Titlebar | sf::Style::Close)
{
    window_.setFramerateLimit(60);
    window_.setKeyRepeatEnabled(false);
    fontLoaded_ = loadFont();
    spawnTargets(kInitTargets);
}

bool DemoApp::loadFont() {
    for (const auto* path : {
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    }) {
        if (font_.openFromFile(path)) return true;
    }
    return false;
}

// ── Boucle principale ─────────────────────────────────────────────────────────

void DemoApp::run() {
    sf::Clock clock;
    while (window_.isOpen()) {
        float dt = clock.restart().asSeconds();

        while (const auto event = window_.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                window_.close();

            if (const auto* kp = event->getIf<sf::Event::KeyPressed>())
                if (kp->scancode == sf::Keyboard::Scancode::Escape)
                    window_.close();

            if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>())
                if (mb->button == sf::Mouse::Button::Left)
                    handleClick(mb->position);
        }

        processGestureQueue();
        update(dt);
        render();
    }
}

// ── EventQueue → overlay geste ───────────────────────────────────────────────

void DemoApp::processGestureQueue() {
    Core::GestureEvent ev;
    while (runner_.eventQueue().tryPop(ev)) {
        lastGestureName_ = Core::gestureName(ev.type);
        if (ev.side == Core::HandSide::LEFT) {
            lastSide_      = "L";
            lastSideColor_ = kColorLeft;
        } else if (ev.side == Core::HandSide::RIGHT) {
            lastSide_      = "R";
            lastSideColor_ = kColorRight;
        } else {
            lastSide_      = "?";
            lastSideColor_ = sf::Color(180, 180, 200);
        }
        gestureFlashActive_ = true;
        gestureFlash_.restart();
    }
    if (gestureFlashActive_ &&
        gestureFlash_.getElapsedTime().asMilliseconds() > kFlashMs)
        gestureFlashActive_ = false;
}

// ── Clic sur target ──────────────────────────────────────────────────────────

void DemoApp::handleClick(sf::Vector2i pos) {
    sf::Vector2f mp(static_cast<float>(pos.x), static_cast<float>(pos.y));
    for (auto& t : targets_) {
        if (!t.alive || t.deathTimer > 0.f) continue;
        float dx = mp.x - t.pos.x;
        float dy = mp.y - t.pos.y;
        float r  = t.radius * t.hoverScale;
        if (dx * dx + dy * dy <= r * r) {
            t.deathTimer = 0.001f;
            ++score_;
        }
    }
}

// ── Spawn ─────────────────────────────────────────────────────────────────────

void DemoApp::spawnTargets(int count) {
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> xDist(70.f, kWinW - 70.f);
    std::uniform_real_distribution<float> yDist(kTopBar + 40.f, kWinH - kBotBar - 50.f);
    std::uniform_int_distribution<int>    cDist(0, kPaletteSize - 1);

    for (int i = 0; i < count; ++i) {
        Target t;
        t.pos    = {xDist(rng), yDist(rng)};
        t.radius = 28.f;
        t.color  = kPalette[cDist(rng)];
        targets_.push_back(t);
    }
}

// ── Update ────────────────────────────────────────────────────────────────────

void DemoApp::update(float dt) {
    sf::Vector2i mpi = sf::Mouse::getPosition(window_);
    sf::Vector2f mp(static_cast<float>(mpi.x), static_cast<float>(mpi.y));

    int dead = 0;
    for (auto& t : targets_) {
        if (!t.alive) { ++dead; continue; }

        if (t.deathTimer > 0.f) {
            t.deathTimer += dt;
            if (t.deathTimer > 0.3f) { t.alive = false; ++dead; }
            continue;
        }

        // Hover
        float dx   = mp.x - t.pos.x;
        float dy   = mp.y - t.pos.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        float goal = (dist < t.radius * 1.5f) ? 1.25f : 1.f;
        t.hoverScale += (goal - t.hoverScale) * std::min(dt * 12.f, 1.f);
    }

    // Respawn autant que mort
    if (dead > 0) spawnTargets(dead);

    // Nettoyer les morts
    targets_.erase(std::remove_if(targets_.begin(), targets_.end(),
        [](const Target& t){ return !t.alive; }), targets_.end());
}

// ── Rendu ─────────────────────────────────────────────────────────────────────

void DemoApp::render() {
    window_.clear(sf::Color(14, 14, 22));

    for (const auto& t : targets_) {
        if (!t.alive) continue;
        float r = t.radius * t.hoverScale;

        if (t.deathTimer > 0.f) {
            float pct = t.deathTimer / 0.3f;
            r = t.radius * (1.f + pct * 1.2f);
            sf::CircleShape ring(r);
            ring.setOrigin({r, r});
            ring.setPosition(t.pos);
            ring.setFillColor(sf::Color::Transparent);
            ring.setOutlineThickness(3.f);
            sf::Color oc = t.color;
            oc.a = static_cast<std::uint8_t>(255 * (1.f - pct));
            ring.setOutlineColor(oc);
            window_.draw(ring);
            continue;
        }

        // Lueur (cercle plus grand, semi-transparent)
        float gr = r + 8.f;
        sf::CircleShape glow(gr);
        glow.setOrigin({gr, gr});
        glow.setPosition(t.pos);
        sf::Color gc = t.color;
        gc.a = 45;
        glow.setFillColor(gc);
        window_.draw(glow);

        // Corps
        sf::CircleShape circle(r);
        circle.setOrigin({r, r});
        circle.setPosition(t.pos);
        sf::Color fc = t.color;
        fc.a = 210;
        circle.setFillColor(fc);
        // Outline légèrement plus clair
        sf::Color oc = t.color;
        oc.r = static_cast<std::uint8_t>(std::min(255, (int)oc.r + 70));
        oc.g = static_cast<std::uint8_t>(std::min(255, (int)oc.g + 70));
        oc.b = static_cast<std::uint8_t>(std::min(255, (int)oc.b + 70));
        circle.setOutlineThickness(2.f);
        circle.setOutlineColor(oc);
        window_.draw(circle);
    }

    drawHUD();
    window_.display();
}

// ── HUD ───────────────────────────────────────────────────────────────────────

void DemoApp::drawHUD() {
    // Ligne de séparation haut
    sf::RectangleShape sep({static_cast<float>(kWinW), 1.f});
    sep.setPosition({0.f, kTopBar});
    sep.setFillColor(sf::Color(50, 50, 70));
    window_.draw(sep);

    // Ligne de séparation bas
    sep.setPosition({0.f, static_cast<float>(kWinH) - kBotBar});
    window_.draw(sep);

    if (!fontLoaded_) return;

    const sf::Color kDim(100, 100, 120);
    const sf::Color kBright(220, 220, 240);

    // ── Titre ─────────────────────────────────────────────────────────────
    sf::Text title(font_, "AURA Demo", 20);
    title.setFillColor(sf::Color(80, 160, 255));
    title.setPosition({18.f, 14.f});
    window_.draw(title);

    // ── Score ─────────────────────────────────────────────────────────────
    sf::Text scoreText(font_, "Score: " + std::to_string(score_), 20);
    scoreText.setFillColor(kBright);
    float sw = scoreText.getLocalBounds().size.x;
    scoreText.setPosition({static_cast<float>(kWinW) - sw - 20.f, 14.f});
    window_.draw(scoreText);

    // ── Flash geste ───────────────────────────────────────────────────────
    if (gestureFlashActive_) {
        float alpha = 1.f;
        float elapsedMs = gestureFlash_.getElapsedTime().asMilliseconds();
        if (elapsedMs > kFlashMs * 0.65f)
            alpha = 1.f - (elapsedMs - kFlashMs * 0.65f) / (kFlashMs * 0.35f);
        alpha = std::clamp(alpha, 0.f, 1.f);

        // Pastille side
        float cx = static_cast<float>(kWinW) * 0.5f - 80.f;
        float cy = 15.f;
        sf::CircleShape dot(8.f);
        dot.setOrigin({8.f, 8.f});
        dot.setPosition({cx, cy + 8.f});
        sf::Color dc = lastSideColor_;
        dc.a = static_cast<std::uint8_t>(alpha * 230);
        dot.setFillColor(dc);
        window_.draw(dot);

        // Nom du geste
        sf::Text gText(font_, lastGestureName_, 18);
        sf::Color tc = lastSideColor_;
        tc.a = static_cast<std::uint8_t>(alpha * 255);
        gText.setFillColor(tc);
        gText.setPosition({cx + 14.f, 14.f});
        window_.draw(gText);
    }

    // ── Hint bas ──────────────────────────────────────────────────────────
    sf::Text hint(font_, "PINCH = clic   OPEN_PALM/POINT = déplacer   ESC = quitter", 12);
    hint.setFillColor(kDim);
    float hw = hint.getLocalBounds().size.x;
    hint.setPosition({(static_cast<float>(kWinW) - hw) * 0.5f,
                      static_cast<float>(kWinH) - kBotBar + 7.f});
    window_.draw(hint);

    // ── Instruction score=0 ───────────────────────────────────────────────
    if (score_ == 0) {
        sf::Text instr(font_, "Cliquez sur les cercles avec PINCH !", 16);
        instr.setFillColor(sf::Color(160, 160, 190, 180));
        float iw = instr.getLocalBounds().size.x;
        instr.setPosition({(static_cast<float>(kWinW) - iw) * 0.5f,
                           static_cast<float>(kWinH) - kBotBar - 30.f});
        window_.draw(instr);
    }
}

} // namespace Aura::Demo
