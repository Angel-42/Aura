#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <utility>
#include <thread>
#include <mutex>
#include <atomic>

namespace Aura::App {

// Surveille la fenêtre active et retourne le profil correspondant.
// La détection tourne dans un thread dédié (osascript/xdotool prend ~100ms).
// Le thread principal lit le résultat sans bloquer via pollChanged().
class AutoProfileSwitcher {
public:
    explicit AutoProfileSwitcher(const std::filesystem::path& configFile);
    ~AutoProfileSwitcher();

    // Non-bloquant. Retourne true si le profil actif a changé depuis le dernier appel.
    // Si true, newProfile contient le nouveau nom.
    bool pollChanged(std::string& newProfile);

    // Recharge les règles depuis le fichier de config.
    void reload();

    [[nodiscard]] bool hasRules() const { return !rules_.empty(); }

    // Crée un fichier de config template si absent.
    static void createTemplateIfMissing(const std::filesystem::path& path);

private:
    std::filesystem::path configFile_;
    std::vector<std::pair<std::string, std::string>> rules_;  // {pattern_lc, profile}

    std::thread         thread_;
    std::atomic<bool>   running_{true};
    std::mutex          mutex_;
    std::string         detectedProfile_ = "default";
    std::string         lastPolled_      = "";

    static constexpr int kPollIntervalMs = 1000;

    void loop();
    [[nodiscard]] std::string getActiveAppName() const;
    [[nodiscard]] std::string matchProfile(const std::string& appName) const;
    [[nodiscard]] static std::string trim(std::string s);
};

} // namespace Aura::App
