#include "Aura/Config/CalibConfig.hpp"
#include <fstream>
#include <iostream>
#include <cstdlib>

namespace Aura::Config {

CalibConfig::CalibConfig() {
    const char* home = std::getenv("HOME");
    dir_ = home ? std::filesystem::path(home) / ".aura"
                : std::filesystem::path(".aura");

    std::error_code ec;
    std::filesystem::create_directories(dir_, ec);
    if (ec) {
        std::cerr << "[CalibConfig] Warning: cannot create " << dir_
                  << " : " << ec.message() << "\n";
    }
}

std::filesystem::path CalibConfig::pathFor(const std::string& name) const {
    return dir_ / ("calib_" + name + ".txt");
}

bool CalibConfig::defaultExists() const {
    return exists("default");
}

bool CalibConfig::exists(const std::string& name) const {
    return std::filesystem::exists(pathFor(name));
}

bool CalibConfig::save(const std::string& name, const Vision::HSVRange& r) const {
    auto path = pathFor(name);
    std::ofstream ofs(path);
    if (!ofs) {
        std::cerr << "[CalibConfig] Cannot write " << path << "\n";
        return false;
    }
    ofs << r.H_min << " " << r.H_max << " "
        << r.S_min << " " << r.S_max << " "
        << r.V_min << " " << r.V_max << "\n";
    std::cout << "[CalibConfig] Saved calibration → " << path << "\n";
    return true;
}

bool CalibConfig::load(const std::string& name, Vision::HSVRange& r) const {
    auto path = pathFor(name);
    std::ifstream ifs(path);
    if (!ifs) {
        std::cerr << "[CalibConfig] Cannot read " << path << "\n";
        return false;
    }
    if (!(ifs >> r.H_min >> r.H_max >> r.S_min >> r.S_max >> r.V_min >> r.V_max)) {
        std::cerr << "[CalibConfig] Invalid format in " << path << "\n";
        return false;
    }
    std::cout << "[CalibConfig] Loaded calibration ← " << path << "\n";
    return true;
}

} // namespace Aura::Config
