#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace Aura::Config {

// Gère les profils de mapping dans ~/.aura/profiles/<name>.txt
// Un profil = un fichier de mapping complet (même format que default_mapping.txt).
class ProfileManager {
public:
    ProfileManager();

    [[nodiscard]] std::filesystem::path path(const std::string& name) const;
    [[nodiscard]] bool exists(const std::string& name) const;

    // Retourne les noms de tous les profils disponibles (sans extension).
    [[nodiscard]] std::vector<std::string> list() const;

    // Copie sourceFile vers profiles/default.txt si le profil "default" n'existe pas.
    bool initDefault(const std::filesystem::path& sourceFile) const;

    [[nodiscard]] std::filesystem::path profilesDir() const { return dir_; }

private:
    std::filesystem::path dir_;
};

} // namespace Aura::Config
