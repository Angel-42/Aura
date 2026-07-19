#include "Aura/Input/DualMapper.hpp"
#include "Aura/Core/GestureEvent.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace Aura::Input {

std::string DualMapper::trim(const std::string& s) {
    auto a = s.find_first_not_of(" \t\r\n");
    auto b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

ActionType DualMapper::parseActionType(const std::string& s) {
    std::string u = s;
    std::transform(u.begin(), u.end(), u.begin(),
                   [](unsigned char c){ return std::toupper(c); });
    if (u == "MOUSE_MOVE")         return ActionType::MOUSE_MOVE;
    if (u == "MOUSE_CLICK")        return ActionType::MOUSE_CLICK;
    if (u == "MOUSE_DOUBLE_CLICK") return ActionType::MOUSE_DOUBLE_CLICK;
    if (u == "MOUSE_DOWN")         return ActionType::MOUSE_DOWN;
    if (u == "MOUSE_UP")           return ActionType::MOUSE_UP;
    if (u == "SCROLL")             return ActionType::SCROLL;
    if (u == "KEY_PRESS")          return ActionType::KEY_PRESS;
    if (u == "KEY_COMBO")          return ActionType::KEY_COMBO;
    if (u == "KEY_DOWN")           return ActionType::KEY_DOWN;
    if (u == "KEY_UP")             return ActionType::KEY_UP;
    return ActionType::NONE;
}

Action DualMapper::parseAction(const std::string& value) {
    Action action;
    std::istringstream ss(value);
    std::string token;
    bool first = true;
    while (ss >> token) {
        if (first) { action.type = parseActionType(token); first = false; }
        else        action.params.push_back(token);
    }
    return action;
}

bool DualMapper::load(const std::filesystem::path& path) {
    std::ifstream f(path);
    if (!f) return false;

    bindings_.clear();
    std::string line;
    int loaded = 0, lineNum = 0;

    while (std::getline(f, line)) {
        ++lineNum;
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        line = trim(line);
        if (line.empty()) continue;

        // Format : LEFT_GESTURE + RIGHT_GESTURE = ACTION
        auto plus = line.find('+');
        auto eq   = line.find('=');
        if (plus == std::string::npos || eq == std::string::npos || plus >= eq) {
            std::cerr << "[DualMapper] Ligne " << lineNum << " ignorée : " << line << "\n";
            continue;
        }

        std::string leftStr  = trim(line.substr(0, plus));
        std::string rightStr = trim(line.substr(plus + 1, eq - plus - 1));
        std::string actStr   = trim(line.substr(eq + 1));

        // Uppercase pour la comparaison
        std::transform(leftStr.begin(),  leftStr.end(),  leftStr.begin(),
                       [](unsigned char c){ return std::toupper(c); });
        std::transform(rightStr.begin(), rightStr.end(), rightStr.begin(),
                       [](unsigned char c){ return std::toupper(c); });

        Core::GestureType left  = Core::gestureFromName(leftStr);
        Core::GestureType right = Core::gestureFromName(rightStr);

        if (left == Core::GestureType::NONE && leftStr != "NONE") {
            std::cerr << "[DualMapper] Geste gauche inconnu '" << leftStr
                      << "' ligne " << lineNum << "\n";
            continue;
        }
        if (right == Core::GestureType::NONE && rightStr != "NONE") {
            std::cerr << "[DualMapper] Geste droit inconnu '" << rightStr
                      << "' ligne " << lineNum << "\n";
            continue;
        }

        Action action = parseAction(actStr);
        if (action.type == ActionType::NONE) {
            std::cerr << "[DualMapper] Action inconnue '" << actStr
                      << "' ligne " << lineNum << "\n";
            continue;
        }

        bindings_[{left, right}] = action;
        ++loaded;
    }

    std::cout << "[DualMapper] " << loaded << " combinaisons ← " << path << "\n";
    return loaded > 0;
}

std::optional<Action> DualMapper::lookup(Core::GestureType left,
                                          Core::GestureType right) const {
    auto it = bindings_.find({left, right});
    if (it != bindings_.end()) return it->second;
    return std::nullopt;
}

} // namespace Aura::Input
