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
    f << "# AURA — Changement de profil automatique selon l'app active\n"
      << "# Format : mot-clé (insensible à la casse) = nom_du_profil\n"
      << "# La première règle qui correspond gagne.\n"
      << "#\n"
      << "firefox   = browser\n"
      << "chrome    = browser\n"
      << "safari    = browser\n"
      << "arc       = browser\n"
      << "#\n"
      << "# minecraft = gaming\n"
      << "# valorant  = gaming\n"
      << "# steam     = gaming\n";
    std::cout << "[AutoProfile] Template créé → " << path << "\n";
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
    if (rules_.empty()) return "default";
    std::string lower = appName;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    for (const auto& [pattern, profile] : rules_)
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
