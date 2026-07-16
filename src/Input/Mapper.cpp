#include "Aura/Input/Mapper.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace Aura::Input {

// --------------------------------------------------------------------------
// Helpers de parsing
// --------------------------------------------------------------------------

std::string Mapper::trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    auto end   = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

Core::GestureType Mapper::parseGesture(const std::string& key) {
    std::string k = key;
    std::transform(k.begin(), k.end(), k.begin(),
                   [](unsigned char c){ return std::toupper(c); });

    if (k == "OPEN_PALM")     return Core::GestureType::OPEN_PALM;
    if (k == "FOUR_FINGERS")  return Core::GestureType::FOUR_FINGERS;
    if (k == "FIST")          return Core::GestureType::FIST;
    if (k == "POINT")         return Core::GestureType::POINT;
    if (k == "TWO_FINGERS")   return Core::GestureType::TWO_FINGERS;
    if (k == "THREE_FINGERS") return Core::GestureType::THREE_FINGERS;
    if (k == "PINCH")         return Core::GestureType::PINCH;
    if (k == "PINCH_MIDDLE")  return Core::GestureType::PINCH_MIDDLE;
    if (k == "PINCH_RING")    return Core::GestureType::PINCH_RING;
    if (k == "PINCH_PINKY")   return Core::GestureType::PINCH_PINKY;
    if (k == "PINCH_DOUBLE")  return Core::GestureType::PINCH_DOUBLE;
    if (k == "PINCH_SIDE")    return Core::GestureType::PINCH_SIDE;
    if (k == "ZTAP")          return Core::GestureType::ZTAP;
    if (k == "SWIPE_LEFT")    return Core::GestureType::SWIPE_LEFT;
    if (k == "SWIPE_RIGHT")   return Core::GestureType::SWIPE_RIGHT;
    if (k == "SWIPE_UP")      return Core::GestureType::SWIPE_UP;
    if (k == "SWIPE_DOWN")    return Core::GestureType::SWIPE_DOWN;
    return Core::GestureType::NONE;
}

ActionType Mapper::parseActionType(const std::string& s) {
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

Action Mapper::parseAction(const std::string& value) {
    Action action;
    std::istringstream ss(value);
    std::string token;
    bool first = true;

    while (ss >> token) {
        if (first) {
            action.type = parseActionType(token);
            first = false;
        } else {
            action.params.push_back(token);
        }
    }
    return action;
}

// --------------------------------------------------------------------------
// Parsing de la clé avec préfixe de main optionnel
// --------------------------------------------------------------------------

static std::pair<Core::HandSide, std::string> splitSidePrefix(const std::string& key) {
    if (key.rfind("LEFT_", 0) == 0)
        return {Core::HandSide::LEFT,  key.substr(5)};
    if (key.rfind("RIGHT_", 0) == 0)
        return {Core::HandSide::RIGHT, key.substr(6)};
    return {Core::HandSide::UNKNOWN,   key};
}

// --------------------------------------------------------------------------
// Chargement
// --------------------------------------------------------------------------

bool Mapper::load(const std::filesystem::path& path) {
    std::ifstream ifs(path);
    if (!ifs) {
        std::cerr << "[Mapper] Cannot open: " << path << "\n";
        return false;
    }

    mapping_.clear();
    leftMapping_.clear();
    rightMapping_.clear();
    std::string line;
    int lineNum = 0;

    while (std::getline(ifs, line)) {
        ++lineNum;
        auto trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        auto sep = trimmed.find('=');
        if (sep == std::string::npos) {
            std::cerr << "[Mapper] Line " << lineNum << " ignored (no '='): " << trimmed << "\n";
            continue;
        }

        std::string rawKey = trim(trimmed.substr(0, sep));
        std::string value  = trim(trimmed.substr(sep + 1));

        // Supprimer les commentaires inline (# jusqu'à fin de ligne)
        auto commentPos = value.find('#');
        if (commentPos != std::string::npos)
            value = trim(value.substr(0, commentPos));
        if (value.empty()) continue;

        auto [side, gestureKey] = splitSidePrefix(rawKey);

        Core::GestureType gesture = parseGesture(gestureKey);
        if (gesture == Core::GestureType::NONE) {
            std::cerr << "[Mapper] Unknown gesture '" << rawKey << "' at line " << lineNum << "\n";
            continue;
        }

        Action action = parseAction(value);

        // NONE explicite = pas d'action. Valide pour overrider côté spécifique.
        // Pour les bindings génériques, NONE est la valeur par défaut — on ne le stocke pas.
        if (action.type == ActionType::NONE) {
            std::string firstToken;
            std::istringstream chk(value);
            chk >> firstToken;
            std::transform(firstToken.begin(), firstToken.end(), firstToken.begin(),
                           [](unsigned char c){ return std::toupper(c); });
            if (firstToken != "NONE") {
                std::cerr << "[Mapper] Unknown action '" << value << "' at line " << lineNum << "\n";
                continue;
            }
            // NONE générique → skip (c'est le défaut)
            if (side == Core::HandSide::UNKNOWN) continue;
        }

        if (side == Core::HandSide::LEFT)        leftMapping_[gesture]  = action;
        else if (side == Core::HandSide::RIGHT)  rightMapping_[gesture] = action;
        else                                     mapping_[gesture]      = action;
    }

    loaded_ = true;
    std::cout << "[Mapper] Loaded "
              << mapping_.size() << " generic + "
              << leftMapping_.size() << " LEFT + "
              << rightMapping_.size() << " RIGHT bindings from " << path << "\n";
    return true;
}

bool Mapper::loadDefault() {
    // Cherche config/default_mapping.txt dans le répertoire courant
    std::filesystem::path p = "config/default_mapping.txt";
    if (!std::filesystem::exists(p)) {
        // Fallback : même répertoire que le binaire (si lancé depuis build/)
        p = "../config/default_mapping.txt";
    }
    if (!std::filesystem::exists(p)) {
        std::cerr << "[Mapper] Default mapping not found. Running without gesture mapping.\n";
        return false;
    }
    return load(p);
}

Action Mapper::actionFor(Core::GestureType gesture, Core::HandSide side) const {
    // Cherche d'abord le binding côté-spécifique
    if (side == Core::HandSide::LEFT) {
        auto it = leftMapping_.find(gesture);
        if (it != leftMapping_.end()) return it->second;
    } else if (side == Core::HandSide::RIGHT) {
        auto it = rightMapping_.find(gesture);
        if (it != rightMapping_.end()) return it->second;
    }
    // Fallback : binding générique
    auto it = mapping_.find(gesture);
    return (it != mapping_.end()) ? it->second : Action{ActionType::NONE, {}};
}

} // namespace Aura::Input
