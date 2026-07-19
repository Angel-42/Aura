#include "DemoApp.hpp"
#include <cmath>
#include <random>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>

namespace Aura::Demo {

// ── Constantes layout ─────────────────────────────────────────────────────────

static constexpr int   kWinW   = 960;
static constexpr int   kWinH   = 620;
static constexpr float kTopBar = 55.f;
static constexpr float kBotBar = 30.f;
static constexpr int   kInitTargets = 7;
static constexpr float kFlashMs    = 700.f;

// Panel rebind
static constexpr float kPanX = 80.f;
static constexpr float kPanY = 60.f;
static constexpr float kPanW = 800.f;
static constexpr float kPanH = 500.f;

static const sf::Color kPalette[] = {
    {255,  90,  90}, {80, 170, 255}, {100, 220, 90},
    {255, 200,  50}, {200, 90, 255}, {255, 140, 50}, {50, 220, 200},
};
static constexpr int kPaletteSize = static_cast<int>(sizeof(kPalette) / sizeof(kPalette[0]));

static const sf::Color kColorLeft  = {255, 150,  50};
static const sf::Color kColorRight = { 80, 180, 255};

// ── Catalogue d'actions disponibles ──────────────────────────────────────────

const std::vector<ActionCategory>& DemoApp::actionCategories() {
    static const std::vector<ActionCategory> cats = {
        {"Souris", {
            {"Clic gauche",      "MOUSE_CLICK LEFT"},
            {"Clic droit",       "MOUSE_CLICK RIGHT"},
            {"Double-clic",      "MOUSE_DOUBLE_CLICK LEFT"},
            {"Scroll haut",      "SCROLL UP 3"},
            {"Scroll bas",       "SCROLL DOWN 3"},
            {"Scroll haut ×5",   "SCROLL UP 5"},
            {"Scroll bas ×5",    "SCROLL DOWN 5"},
            {"Drag (appuyer)",   "MOUSE_DOWN LEFT"},
            {"Drag (relâcher)",  "MOUSE_UP LEFT"},
        }},
        {"Touches", {
            {"Espace",          "KEY_PRESS SPACE"},
            {"Entrée",          "KEY_PRESS RETURN"},
            {"Échap",           "KEY_PRESS ESCAPE"},
            {"Tab",             "KEY_PRESS TAB"},
            {"← Gauche",        "KEY_PRESS LEFT"},
            {"→ Droite",        "KEY_PRESS RIGHT"},
            {"↑ Haut",          "KEY_PRESS UP"},
            {"↓ Bas",           "KEY_PRESS DOWN"},
            {"Suppr",           "KEY_PRESS DELETE"},
            {"Retour arrière",  "KEY_PRESS BACKSPACE"},
            {"F5 (refresh)",    "KEY_PRESS F5"},
        }},
        {"Raccourcis", {
            {"Copier",          "KEY_COMBO CMD C"},
            {"Coller",          "KEY_COMBO CMD V"},
            {"Couper",          "KEY_COMBO CMD X"},
            {"Annuler",         "KEY_COMBO CMD Z"},
            {"Rétablir",        "KEY_COMBO CMD SHIFT Z"},
            {"Enregistrer",     "KEY_COMBO CMD S"},
            {"Tout sélect.",    "KEY_COMBO CMD A"},
            {"Chercher",        "KEY_COMBO CMD F"},
            {"Nouvel onglet",   "KEY_COMBO CMD T"},
            {"Fermer onglet",   "KEY_COMBO CMD W"},
            {"Onglet préc.",    "KEY_COMBO CMD SHIFT TAB"},
            {"Onglet suiv.",    "KEY_COMBO CMD TAB"},
        }},
        {"Autre", {
            {"Aucune action",   "NONE"},
        }},
    };
    return cats;
}

// ── Construction ──────────────────────────────────────────────────────────────

DemoApp::DemoApp(App::AuraRunner& runner)
    : runner_(runner)
    , window_(sf::VideoMode({kWinW, kWinH}), "AURA Demo",
              sf::Style::Titlebar | sf::Style::Close)
{
    window_.setFramerateLimit(60);
    window_.setKeyRepeatEnabled(false);
    fontLoaded_ = loadFont();
    spawnTargets(kInitTargets);

    // Bouton HUD "Rebind" (coin haut gauche après le titre)
    rebindBtnHudRect_ = sf::FloatRect({kWinW - 115.f, 10.f}, {100.f, 32.f});
}

bool DemoApp::loadFont() {
    for (const auto* p : {
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    }) {
        if (font_.openFromFile(p)) return true;
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

            if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
                if (kp->scancode == sf::Keyboard::Scancode::Escape) {
                    if (rebindStep_ != RebindStep::IDLE) cancelRebind();
                    else window_.close();
                }
                if (kp->scancode == sf::Keyboard::Scancode::R &&
                    rebindStep_ == RebindStep::IDLE)
                    startRebind();
            }

            if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>())
                if (mb->button == sf::Mouse::Button::Left)
                    handleClick(mb->position);
        }

        processGestureQueue();
        update(dt);
        render();
    }
}

// ── EventQueue ────────────────────────────────────────────────────────────────

void DemoApp::processGestureQueue() {
    Core::GestureEvent ev;
    while (runner_.eventQueue().tryPop(ev)) {
        if (rebindStep_ == RebindStep::WAITING_GESTURE) {
            // Capturer ce geste — swipes exclus (trop furtifs pour rebind)
            if (ev.type < Core::GestureType::SWIPE_LEFT) {
                capturedGesture_ = ev.type;
                buildRebindButtons();
                rebindStep_ = RebindStep::WAITING_ACTION;
            }
            continue; // ne pas mettre à jour le flash en mode rebind
        }
        // Flash geste normal
        lastGestureName_ = Core::gestureName(ev.type);
        lastSide_        = (ev.side == Core::HandSide::LEFT)  ? "L"
                         : (ev.side == Core::HandSide::RIGHT) ? "R" : "?";
        lastSideColor_   = (ev.side == Core::HandSide::LEFT)  ? kColorLeft
                         : (ev.side == Core::HandSide::RIGHT) ? kColorRight
                         : sf::Color(180, 180, 200);
        gestureFlashActive_ = true;
        gestureFlash_.restart();
    }

    if (gestureFlashActive_ &&
        gestureFlash_.getElapsedTime().asMilliseconds() > kFlashMs)
        gestureFlashActive_ = false;

    if (savedVisible_ && savedClock_.getElapsedTime().asMilliseconds() > 1800)
        savedVisible_ = false;
}

// ── Rebind ────────────────────────────────────────────────────────────────────

void DemoApp::startRebind() {
    rebindStep_      = RebindStep::WAITING_GESTURE;
    capturedGesture_ = Core::GestureType::NONE;
    rebindButtons_.clear();
}

void DemoApp::cancelRebind() {
    rebindStep_ = RebindStep::IDLE;
    rebindButtons_.clear();
}

void DemoApp::buildRebindButtons() {
    rebindButtons_.clear();

    const auto& cats = actionCategories();
    constexpr int   kCols   = 4;
    constexpr float kBtnW   = 172.f;
    constexpr float kBtnH   = 30.f;
    constexpr float kGapX   = 8.f;
    constexpr float kGapY   = 6.f;
    constexpr float kStartX = kPanX + 20.f;
    constexpr float kStartY = kPanY + 120.f;  // sous l'en-tête du panel

    float y = kStartY;
    for (const auto& cat : cats) {
        y += 22.f;  // hauteur du header de catégorie
        int col = 0;
        float rowX = kStartX;
        for (const auto& entry : cat.entries) {
            RebindButton btn;
            btn.rect   = sf::FloatRect({rowX, y}, {kBtnW, kBtnH});
            btn.action = entry;
            rebindButtons_.push_back(btn);
            ++col;
            if (col >= kCols) {
                col = 0;
                rowX = kStartX;
                y += kBtnH + kGapY;
            } else {
                rowX += kBtnW + kGapX;
            }
        }
        if (col > 0) y += kBtnH + kGapY;  // terminer la dernière ligne partielle
        y += 4.f;  // espace inter-catégorie
    }

    // Bouton Annuler
    rebindCancelRect_ = sf::FloatRect({kPanX + kPanW - 120.f, kPanY + kPanH - 48.f}, {105.f, 34.f});
}

void DemoApp::applyBinding(const ActionEntry& entry) {
    auto path = runner_.activeProfilePath();
    if (path.empty() || !std::filesystem::exists(path)) {
        std::cerr << "[Rebind] Profil introuvable : " << path << "\n";
        cancelRebind();
        return;
    }

    upsertBinding(path, Core::gestureName(capturedGesture_), entry.binding);
    runner_.reloadProfile();

    std::cout << "[Rebind] " << Core::gestureName(capturedGesture_)
              << " → " << entry.binding << "  (profil: " << path.filename().string() << ")\n";

    savedVisible_ = true;
    savedClock_.restart();
    cancelRebind();
}

void DemoApp::upsertBinding(const std::filesystem::path& profilePath,
                             const std::string& gesture,
                             const std::string& actionStr) {
    std::vector<std::string> lines;
    {
        std::ifstream ifs(profilePath);
        std::string line;
        while (std::getline(ifs, line)) lines.push_back(line);
    }

    bool found = false;
    for (auto& line : lines) {
        // Strip comment to find the key
        auto commentPos = line.find('#');
        std::string code = (commentPos != std::string::npos)
            ? line.substr(0, commentPos) : line;
        auto eq = code.find('=');
        if (eq == std::string::npos) continue;

        std::string key = code.substr(0, eq);
        // Trim key
        auto a = key.find_first_not_of(" \t");
        auto b = key.find_last_not_of(" \t");
        if (a == std::string::npos) continue;
        key = key.substr(a, b - a + 1);
        std::transform(key.begin(), key.end(), key.begin(), ::toupper);

        if (key == gesture) {
            line  = gesture + " = " + actionStr;
            found = true;
        }
    }

    if (!found)
        lines.push_back(gesture + " = " + actionStr);

    std::ofstream ofs(profilePath);
    for (const auto& l : lines) ofs << l << "\n";
}

// ── Clic ──────────────────────────────────────────────────────────────────────

void DemoApp::handleClick(sf::Vector2i pos) {
    sf::Vector2f mp(static_cast<float>(pos.x), static_cast<float>(pos.y));

    // Bouton Rebind dans le HUD
    if (rebindStep_ == RebindStep::IDLE && rebindBtnHudRect_.contains(mp)) {
        startRebind();
        return;
    }

    // Panel rebind ouvert
    if (rebindStep_ == RebindStep::WAITING_ACTION) {
        if (rebindCancelRect_.contains(mp)) { cancelRebind(); return; }
        for (const auto& btn : rebindButtons_) {
            if (btn.rect.contains(mp)) { applyBinding(btn.action); return; }
        }
        return; // clic en dehors des boutons → ignorer
    }

    if (rebindStep_ == RebindStep::WAITING_GESTURE) {
        // Clic sur cancel (ESC / clic hors panel)
        if (mp.x < kPanX || mp.x > kPanX + kPanW ||
            mp.y < kPanY || mp.y > kPanY + kPanH)
            cancelRebind();
        return;
    }

    // Jeu : clic sur une cible
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
    if (rebindStep_ != RebindStep::IDLE) return;  // jeu en pause pendant rebind

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
        float dx   = mp.x - t.pos.x;
        float dy   = mp.y - t.pos.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        float goal = (dist < t.radius * 1.5f) ? 1.25f : 1.f;
        t.hoverScale += (goal - t.hoverScale) * std::min(dt * 12.f, 1.f);
    }

    if (dead > 0) spawnTargets(dead);
    targets_.erase(std::remove_if(targets_.begin(), targets_.end(),
        [](const Target& t){ return !t.alive; }), targets_.end());
}

// ── Rendu ─────────────────────────────────────────────────────────────────────

void DemoApp::render() {
    window_.clear(sf::Color(14, 14, 22));

    // Cibles (atténuées si panel ouvert)
    for (const auto& t : targets_) {
        if (!t.alive) continue;
        float r = t.radius * t.hoverScale;

        if (t.deathTimer > 0.f) {
            float pct = t.deathTimer / 0.3f;
            float dr  = t.radius * (1.f + pct * 1.2f);
            sf::CircleShape ring(dr);
            ring.setOrigin({dr, dr});
            ring.setPosition(t.pos);
            ring.setFillColor(sf::Color::Transparent);
            ring.setOutlineThickness(3.f);
            sf::Color oc = t.color;
            oc.a = static_cast<std::uint8_t>(255 * (1.f - pct));
            ring.setOutlineColor(oc);
            window_.draw(ring);
            continue;
        }

        sf::Color fc = t.color;
        if (rebindStep_ != RebindStep::IDLE) fc.a = 60;  // atténuer pendant rebind

        float gr = r + 8.f;
        sf::CircleShape glow(gr);
        glow.setOrigin({gr, gr});
        glow.setPosition(t.pos);
        sf::Color gc = fc;
        gc.a = (rebindStep_ == RebindStep::IDLE) ? 45 : 15;
        glow.setFillColor(gc);
        window_.draw(glow);

        sf::CircleShape circle(r);
        circle.setOrigin({r, r});
        circle.setPosition(t.pos);
        circle.setFillColor(fc);
        if (rebindStep_ == RebindStep::IDLE) {
            sf::Color oc = t.color;
            oc.r = static_cast<std::uint8_t>(std::min(255, (int)oc.r + 70));
            oc.g = static_cast<std::uint8_t>(std::min(255, (int)oc.g + 70));
            oc.b = static_cast<std::uint8_t>(std::min(255, (int)oc.b + 70));
            circle.setOutlineThickness(2.f);
            circle.setOutlineColor(oc);
        }
        window_.draw(circle);
    }

    drawHUD();

    if (rebindStep_ != RebindStep::IDLE)
        drawRebindPanel();

    window_.display();
}

// ── HUD ───────────────────────────────────────────────────────────────────────

void DemoApp::drawHUD() {
    sf::RectangleShape sep({static_cast<float>(kWinW), 1.f});
    sep.setPosition({0.f, kTopBar});
    sep.setFillColor(sf::Color(50, 50, 70));
    window_.draw(sep);

    sep.setPosition({0.f, static_cast<float>(kWinH) - kBotBar});
    window_.draw(sep);

    if (!fontLoaded_) return;

    const sf::Color kDim   (100, 100, 120);
    const sf::Color kBright(220, 220, 240);

    // Titre
    sf::Text title(font_, "AURA Demo", 20);
    title.setFillColor(sf::Color(80, 160, 255));
    title.setPosition({18.f, 14.f});
    window_.draw(title);

    // Score
    sf::Text scoreText(font_, "Score: " + std::to_string(score_), 20);
    scoreText.setFillColor(kBright);
    float sw = scoreText.getLocalBounds().size.x;
    scoreText.setPosition({rebindBtnHudRect_.position.x - sw - 20.f, 14.f});
    window_.draw(scoreText);

    // Bouton Rebind
    {
        sf::Vector2i mpi = sf::Mouse::getPosition(window_);
        bool hovered = rebindBtnHudRect_.contains(sf::Vector2f(mpi));
        sf::RectangleShape btn(sf::Vector2f(rebindBtnHudRect_.size));
        btn.setPosition(rebindBtnHudRect_.position);
        btn.setFillColor(hovered ? sf::Color(60, 100, 180) : sf::Color(35, 50, 90));
        btn.setOutlineThickness(1.f);
        btn.setOutlineColor(sf::Color(80, 120, 200));
        window_.draw(btn);

        sf::Text rbText(font_, "Rebind [R]", 13);
        rbText.setFillColor(hovered ? sf::Color::White : sf::Color(160, 190, 240));
        float tw = rbText.getLocalBounds().size.x;
        rbText.setPosition({
            rebindBtnHudRect_.position.x + (rebindBtnHudRect_.size.x - tw) * 0.5f,
            rebindBtnHudRect_.position.y + 8.f
        });
        window_.draw(rbText);
    }

    // Flash geste
    if (gestureFlashActive_) {
        float elapsedMs = gestureFlash_.getElapsedTime().asMilliseconds();
        float alpha = std::clamp(1.f - (elapsedMs - kFlashMs * 0.65f) / (kFlashMs * 0.35f),
                                 0.f, 1.f);

        float cx = static_cast<float>(kWinW) * 0.5f - 90.f;
        float cy = 15.f;
        sf::CircleShape dot(8.f);
        dot.setOrigin({8.f, 8.f});
        dot.setPosition({cx, cy + 8.f});
        sf::Color dc = lastSideColor_;
        dc.a = static_cast<std::uint8_t>(alpha * 230);
        dot.setFillColor(dc);
        window_.draw(dot);

        sf::Text gText(font_, lastGestureName_, 18);
        sf::Color tc = lastSideColor_;
        tc.a = static_cast<std::uint8_t>(alpha * 255);
        gText.setFillColor(tc);
        gText.setPosition({cx + 14.f, 14.f});
        window_.draw(gText);
    }

    // "Saved!" confirmation
    if (savedVisible_) {
        float t = savedClock_.getElapsedTime().asMilliseconds();
        float alpha = std::clamp(1.f - (t - 1200.f) / 600.f, 0.f, 1.f);
        sf::Text saved(font_, "✓ Binding sauvegardé !", 15);
        sf::Color sc(100, 230, 130, static_cast<std::uint8_t>(alpha * 255));
        saved.setFillColor(sc);
        float tw = saved.getLocalBounds().size.x;
        saved.setPosition({(kWinW - tw) * 0.5f, 15.f});
        window_.draw(saved);
    }

    // Hint bas
    sf::Text hint(font_, "PINCH = clic   OPEN_PALM/POINT = déplacer   R = rebind   ESC = quitter", 12);
    hint.setFillColor(kDim);
    float hw = hint.getLocalBounds().size.x;
    hint.setPosition({(static_cast<float>(kWinW) - hw) * 0.5f,
                      static_cast<float>(kWinH) - kBotBar + 7.f});
    window_.draw(hint);
}

// ── Panel rebind ──────────────────────────────────────────────────────────────

void DemoApp::drawRebindPanel() {
    // Fond semi-transparent
    sf::RectangleShape overlay({static_cast<float>(kWinW), static_cast<float>(kWinH)});
    overlay.setFillColor(sf::Color(0, 0, 0, 160));
    window_.draw(overlay);

    // Panel
    sf::RectangleShape panel({kPanW, kPanH});
    panel.setPosition({kPanX, kPanY});
    panel.setFillColor(sf::Color(16, 16, 28, 248));
    panel.setOutlineThickness(1.f);
    panel.setOutlineColor(sf::Color(70, 80, 120));
    window_.draw(panel);

    if (!fontLoaded_) return;

    const sf::Color kTitle  (200, 210, 255);
    const sf::Color kSubtitle(130, 140, 180);
    const sf::Color kCatHdr (100, 110, 160);
    const sf::Color kBtnNorm(35, 40, 65);
    const sf::Color kBtnHover(55, 80, 140);
    const sf::Color kBtnBorder(60, 70, 110);
    const sf::Color kBtnText (200, 205, 230);

    sf::Vector2i mpi = sf::Mouse::getPosition(window_);
    sf::Vector2f mp(mpi);

    // ── WAITING_GESTURE ───────────────────────────────────────────────────────
    if (rebindStep_ == RebindStep::WAITING_GESTURE) {
        sf::Text t1(font_, "Rebind un geste", 22);
        t1.setFillColor(kTitle);
        t1.setPosition({kPanX + 24.f, kPanY + 20.f});
        window_.draw(t1);

        sf::Text t2(font_, "Fais le geste que tu veux rebinder...", 16);
        t2.setFillColor(kSubtitle);
        t2.setPosition({kPanX + 24.f, kPanY + 55.f});
        window_.draw(t2);

        // Animation pointillée
        float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(sf::Clock().getElapsedTime().asMilliseconds()) * 0.003f);
        sf::CircleShape dot(8.f + pulse * 4.f);
        float dr = 8.f + pulse * 4.f;
        dot.setOrigin({dr, dr});
        dot.setPosition({kPanX + kPanW * 0.5f, kPanY + kPanH * 0.42f});
        dot.setFillColor(sf::Color(80, 140, 255, 180));
        window_.draw(dot);

        sf::Text t3(font_, "(PINCH, FIST, TWO_FINGERS, etc.)", 14);
        t3.setFillColor(sf::Color(90, 100, 140));
        float tw3 = t3.getLocalBounds().size.x;
        t3.setPosition({kPanX + (kPanW - tw3) * 0.5f, kPanY + kPanH * 0.55f});
        window_.draw(t3);

        sf::Text hint(font_, "ESC ou clic hors du panel = annuler", 13);
        hint.setFillColor(sf::Color(70, 75, 110));
        float hw = hint.getLocalBounds().size.x;
        hint.setPosition({kPanX + (kPanW - hw) * 0.5f, kPanY + kPanH - 38.f});
        window_.draw(hint);
        return;
    }

    // ── WAITING_ACTION ────────────────────────────────────────────────────────
    // Titre + geste capturé
    sf::Text t1(font_, "Geste capturé : " + Core::gestureName(capturedGesture_), 20);
    t1.setFillColor(kTitle);
    t1.setPosition({kPanX + 24.f, kPanY + 18.f});
    window_.draw(t1);

    sf::Text t2(font_, "Choisis l'action à lui assigner :", 14);
    t2.setFillColor(kSubtitle);
    t2.setPosition({kPanX + 24.f, kPanY + 50.f});
    window_.draw(t2);

    // Ligne séparatrice
    sf::RectangleShape sep({kPanW - 40.f, 1.f});
    sep.setPosition({kPanX + 20.f, kPanY + 76.f});
    sep.setFillColor(sf::Color(50, 55, 85));
    window_.draw(sep);

    // Boutons d'action (positions calculées dans buildRebindButtons)
    // Recalculer les catégories pour les headers
    const auto& cats = actionCategories();
    size_t btnIdx = 0;
    constexpr int   kCols   = 4;
    constexpr float kBtnW   = 172.f;
    constexpr float kBtnH   = 30.f;
    constexpr float kGapX   = 8.f;
    constexpr float kGapY   = 6.f;
    constexpr float kStartX = kPanX + 20.f;
    constexpr float kStartY = kPanY + 120.f;

    float y = kStartY;
    for (const auto& cat : cats) {
        // Header catégorie
        sf::Text catHdr(font_, cat.label, 12);
        catHdr.setFillColor(kCatHdr);
        catHdr.setPosition({kStartX, y + 4.f});
        window_.draw(catHdr);
        y += 22.f;

        int col = 0;
        float rowX = kStartX;
        for (const auto& entry : cat.entries) {
            if (btnIdx < rebindButtons_.size()) {
                const auto& btn = rebindButtons_[btnIdx];
                bool hovered = btn.rect.contains(mp);

                sf::RectangleShape shape(sf::Vector2f(btn.rect.size));
                shape.setPosition(btn.rect.position);
                shape.setFillColor(hovered ? kBtnHover : kBtnNorm);
                shape.setOutlineThickness(1.f);
                shape.setOutlineColor(hovered ? sf::Color(100, 140, 220) : kBtnBorder);
                window_.draw(shape);

                sf::Text label(font_, entry.label, 12);
                label.setFillColor(hovered ? sf::Color::White : kBtnText);
                float lw = label.getLocalBounds().size.x;
                float lh = label.getLocalBounds().size.y;
                label.setPosition({
                    btn.rect.position.x + (btn.rect.size.x - lw) * 0.5f,
                    btn.rect.position.y + (btn.rect.size.y - lh) * 0.5f - 2.f
                });
                window_.draw(label);
                ++btnIdx;
            }

            ++col;
            if (col >= kCols) {
                col = 0;
                rowX = kStartX;
                y += kBtnH + kGapY;
            } else {
                rowX += kBtnW + kGapX;
            }
        }
        if (col > 0) y += kBtnH + kGapY;
        y += 4.f;
    }

    // Bouton Annuler
    {
        bool hovered = rebindCancelRect_.contains(mp);
        sf::RectangleShape shape(sf::Vector2f(rebindCancelRect_.size));
        shape.setPosition(rebindCancelRect_.position);
        shape.setFillColor(hovered ? sf::Color(120, 40, 40) : sf::Color(60, 25, 25));
        shape.setOutlineThickness(1.f);
        shape.setOutlineColor(sf::Color(130, 50, 50));
        window_.draw(shape);

        sf::Text ct(font_, "Annuler (ESC)", 13);
        ct.setFillColor(sf::Color(230, 160, 160));
        float tw = ct.getLocalBounds().size.x;
        ct.setPosition({
            rebindCancelRect_.position.x + (rebindCancelRect_.size.x - tw) * 0.5f,
            rebindCancelRect_.position.y + 9.f
        });
        window_.draw(ct);
    }
}

} // namespace Aura::Demo
