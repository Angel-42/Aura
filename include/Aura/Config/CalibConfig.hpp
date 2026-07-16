#pragma once
#include "Aura/Vision/Types.hpp"
#include <filesystem>
#include <string>

namespace Aura::Config {

class CalibConfig {
public:
    CalibConfig();

    [[nodiscard]] bool defaultExists() const;
    [[nodiscard]] bool exists(const std::string& name) const;
    bool save(const std::string& name, const Vision::HSVRange& range) const;
    bool load(const std::string& name, Vision::HSVRange& range) const;

    [[nodiscard]] std::filesystem::path configDir() const { return dir_; }

private:
    std::filesystem::path dir_;
    [[nodiscard]] std::filesystem::path pathFor(const std::string& name) const;
};

} // namespace Aura::Config
