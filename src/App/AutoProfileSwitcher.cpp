#include "Aura/App/AutoProfileSwitcher.hpp"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <chrono>
#include <cstdio>

namespace Aura::App {

// --------------------------------------------------------------------------
// Construction / destruction
// --------------------------------------------------------------------------

AutoProfileSwitcher::AutoProfileSwitcher(const std::filesystem::path& configFile)
    : configFile_(configFile)
{
    reload();
    thread_ = std::thread([this] { loop(); });
}

AutoProfileSwitcher::~AutoProfileSwitcher() {
    running_.store(false);
    if (thread_.joinable()) thread_.join();
}

// --------------------------------------------------------------------------
// Template de config
// --------------------------------------------------------------------------

void AutoProfileSwitcher::createTemplateIfMissing(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) return;
    std::ofstream f(path);
    if (!f) return;
    f << "# AURA — Overrides de profil automatique\n"
      << "# Format : mot-clé (insensible à la casse) = nom_du_profil\n"
      << "# La première règle qui correspond gagne.\n"
      << "#\n"
      << "# Les apps courantes sont déjà reconnues automatiquement :\n"
      << "#   Firefox, Chrome, Safari, Arc, Brave, Opera  → browser\n"
      << "#   Steam, Minecraft, Valorant, CS2, Fortnite… → gaming\n"
      << "#   Tout le reste                               → default\n"
      << "#\n"
      << "# Ce fichier sert uniquement pour les cas personnalisés :\n"
      << "# mon_app       = gaming\n"
      << "# mon_editeur   = default\n";
    std::cout << "[AutoProfile] Config créée → " << path << "\n";
}

// --------------------------------------------------------------------------
// Chargement des règles
// --------------------------------------------------------------------------

std::string AutoProfileSwitcher::trim(std::string s) {
    auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

void AutoProfileSwitcher::reload() {
    std::vector<std::pair<std::string, std::string>> newRules;
    std::ifstream f(configFile_);
    if (!f) { rules_ = newRules; return; }

    std::string line;
    while (std::getline(f, line)) {
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        line = trim(line);
        if (line.empty()) continue;

        auto sep = line.find('=');
        if (sep == std::string::npos) continue;

        std::string pattern = trim(line.substr(0, sep));
        std::string profile = trim(line.substr(sep + 1));
        if (pattern.empty() || profile.empty()) continue;

        // Stocker en minuscules pour la comparaison
        std::transform(pattern.begin(), pattern.end(), pattern.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        newRules.emplace_back(std::move(pattern), std::move(profile));
    }

    rules_ = std::move(newRules);
    std::cout << "[AutoProfile] " << rules_.size() << " règle(s) ← " << configFile_ << "\n";
}

// --------------------------------------------------------------------------
// Détection de l'app active (appel bloquant, ~50-150ms)
// --------------------------------------------------------------------------

// Retourne la catégorie macOS de l'app active ("games", "action-games", etc.)
// ou "" si non disponible.
#ifdef __APPLE__
static std::string getActiveAppCategory() {
    // Récupère le chemin de l'app au premier plan
    FILE* pipe = popen(
        "osascript -e 'POSIX path of (path to frontmost application)' 2>/dev/null", "r");
    if (!pipe) return "";
    char buf[1024] = {};
    fgets(buf, sizeof(buf), pipe);
    pclose(pipe);
    std::string appPath = buf;
    while (!appPath.empty() && (appPath.back() == '\n' || appPath.back() == '\r' || appPath.back() == '/'))
        appPath.pop_back();
    if (appPath.empty()) return "";

    // mdls lit la catégorie depuis les métadonnées Spotlight
    std::string cmd = "mdls -name kMDItemAppStoreCategory \"" + appPath + "\" 2>/dev/null";
    FILE* pipe2 = popen(cmd.c_str(), "r");
    if (!pipe2) return "";
    char buf2[256] = {};
    fgets(buf2, sizeof(buf2), pipe2);
    pclose(pipe2);
    std::string result(buf2);
    // Format: "kMDItemAppStoreCategory = \"Games\""
    auto q1 = result.find('"');
    if (q1 == std::string::npos) return "";
    auto q2 = result.find('"', q1 + 1);
    if (q2 == std::string::npos) return "";
    std::string cat = result.substr(q1 + 1, q2 - q1 - 1);
    std::transform(cat.begin(), cat.end(), cat.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return cat;  // ex: "games", "action games", "role playing games", "(null)"
}
#endif

std::string AutoProfileSwitcher::getActiveAppName() const {
#ifdef __APPLE__
    FILE* pipe = popen(
        "osascript -e 'tell application \"System Events\" to get name of "
        "first application process whose frontmost is true' 2>/dev/null", "r");
    if (!pipe) return "";
    char buf[256] = {};
    fgets(buf, sizeof(buf), pipe);
    pclose(pipe);
    return trim(std::string(buf));
#elif defined(__linux__)
    FILE* pipe = popen("xdotool getactivewindow getwindowname 2>/dev/null", "r");
    if (!pipe) return "";
    char buf[512] = {};
    std::string result;
    while (fgets(buf, sizeof(buf), pipe)) result += buf;
    pclose(pipe);
    return trim(result);
#else
    return "";
#endif
}

std::string AutoProfileSwitcher::matchProfile(const std::string& appName) const {
    std::string lower = appName;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // 1. Règles utilisateur (auto_profile.txt) — priorité maximale
    for (const auto& [pattern, profile] : rules_)
        if (lower.find(pattern) != std::string::npos) return profile;

    // 2. Catégorie macOS — détecte n'importe quel jeu App Store / Steam
#ifdef __APPLE__
    {
        std::string cat = getActiveAppCategory();
        if (!cat.empty() && cat != "(null)" && cat.find("game") != std::string::npos)
            return "gaming";
    }
#endif

    // 3. Base built-in — noms connus (jeux hors App Store, browsers)
    static const std::pair<const char*, const char*> kBuiltin[] = {
        // Navigateurs → browser
        {"firefox",        "browser"},
        {"chrome",         "browser"},
        {"chromium",       "browser"},
        {"safari",         "browser"},
        {"arc",            "browser"},
        {"brave",          "browser"},
        {"opera",          "browser"},
        {"vivaldi",        "browser"},
        {"edge",           "browser"},
        {"tor browser",    "browser"},
        // Jeux / launchers → gaming
        {"steam",          "gaming"},
        {"minecraft",      "gaming"},
        {"valorant",       "gaming"},
        {"fortnite",       "gaming"},
        {"overwatch",      "gaming"},
        {"apex",           "gaming"},
        {"league of legends", "gaming"},
        {"dota",           "gaming"},
        {"counter-strike", "gaming"},
        {"csgo",           "gaming"},
        {"cs2",            "gaming"},
        {"cyberpunk",      "gaming"},
        {"gta",            "gaming"},
        {"roblox",         "gaming"},
        {"hollow knight",  "gaming"},
        {"celeste",        "gaming"},
        {"unity",          "gaming"},   // éditeur Unity = souvent test de jeu
    };
    for (const auto& [pattern, profile] : kBuiltin)
        if (lower.find(pattern) != std::string::npos) return profile;

    return "default";
}

// --------------------------------------------------------------------------
// Thread de surveillance
// --------------------------------------------------------------------------

void AutoProfileSwitcher::loop() {
    std::string lastApp;
    while (running_.load(std::memory_order_relaxed)) {
        std::string app = getActiveAppName();
        if (app != lastApp) {
            lastApp = app;
            std::string profile = matchProfile(app);
            {
                std::lock_guard lock(mutex_);
                detectedProfile_ = profile;
            }
        }
        // Attendre kPollIntervalMs en petits slices pour réagir à running_=false
        for (int i = 0; i < 10 && running_.load(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(kPollIntervalMs / 10));
    }
}

// --------------------------------------------------------------------------
// Interface principale (thread principal)
// --------------------------------------------------------------------------

bool AutoProfileSwitcher::pollChanged(std::string& newProfile) {
    std::string current;
    {
        std::lock_guard lock(mutex_);
        current = detectedProfile_;
    }
    if (current == lastPolled_) return false;
    lastPolled_ = current;
    newProfile  = current;
    return true;
}

} // namespace Aura::App
