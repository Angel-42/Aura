#include "Aura/Config/ProfileManager.hpp"
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <algorithm>

namespace Aura::Config {

ProfileManager::ProfileManager() {
    const char* home = std::getenv("HOME");
    dir_ = (home ? std::filesystem::path(home) : std::filesystem::path("."))
           / ".aura" / "profiles";

    std::error_code ec;
    std::filesystem::create_directories(dir_, ec);
    if (ec) {
        std::cerr << "[ProfileManager] Warning: cannot create " << dir_
                  << " : " << ec.message() << "\n";
    }
}

std::filesystem::path ProfileManager::path(const std::string& name) const {
    return dir_ / (name + ".txt");
}

bool ProfileManager::exists(const std::string& name) const {
    return std::filesystem::exists(path(name));
}

std::vector<std::string> ProfileManager::list() const {
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir_, ec)) {
        if (entry.path().extension() == ".txt")
            names.push_back(entry.path().stem().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

bool ProfileManager::initDefault(const std::filesystem::path& sourceFile) const {
    if (exists("default")) return true;  // déjà présent

    // Chercher le fichier source (chemin relatif depuis le binaire)
    std::vector<std::filesystem::path> candidates = {
        sourceFile,
        std::filesystem::path("../") / sourceFile,
    };
    std::filesystem::path src;
    for (const auto& c : candidates) {
        if (std::filesystem::exists(c)) { src = c; break; }
    }

    if (src.empty()) {
        std::cerr << "[ProfileManager] Source introuvable pour le profil default : "
                  << sourceFile << "\n";
        return false;
    }

    std::error_code ec;
    std::filesystem::copy_file(src, path("default"), ec);
    if (ec) {
        std::cerr << "[ProfileManager] Impossible de copier vers " << path("default")
                  << " : " << ec.message() << "\n";
        return false;
    }
    std::cout << "[ProfileManager] Profil 'default' créé → " << path("default") << "\n";
    return true;
}

void ProfileManager::seedFromDir(const std::filesystem::path& configDir) const {
    // Chercher le répertoire config/ (relatif au CWD ou au parent)
    std::filesystem::path src;
    for (const auto& candidate : {configDir, std::filesystem::path("../") / configDir}) {
        if (std::filesystem::is_directory(candidate)) { src = candidate; break; }
    }
    if (src.empty()) return;

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(src, ec)) {
        if (entry.path().extension() != ".txt") continue;
        std::string name = entry.path().stem().string();
        if (exists(name)) continue;  // ne pas écraser les modifs utilisateur
        std::filesystem::copy_file(entry.path(), path(name), ec);
        if (!ec)
            std::cout << "[ProfileManager] Profil '" << name << "' créé → " << path(name) << "\n";
    }
}

} // namespace Aura::Config
