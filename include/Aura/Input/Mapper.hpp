#pragma once
#include "Aura/Core/GestureEvent.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <filesystem>

namespace Aura::Input {

enum class ActionType {
    NONE,
    MOUSE_MOVE,
    MOUSE_CLICK,
    MOUSE_DOUBLE_CLICK,
    MOUSE_DOWN,
    MOUSE_UP,
    SCROLL,
    KEY_PRESS,
    KEY_COMBO,   // KEY_COMBO MOD1 [MOD2] KEY → tient les modifs, presse la dernière touche
    KEY_DOWN,
    KEY_UP,
};

struct Action {
    ActionType           type = ActionType::NONE;
    std::vector<std::string> params;  // ex: {"LEFT"} pour MOUSE_CLICK
};

class Mapper {
public:
    Mapper() = default;
    bool load(const std::filesystem::path& path);
    bool loadDefault();  // cherche config/default_mapping.txt à côté du binaire

    [[nodiscard]] Action actionFor(Core::GestureType gesture) const;
    [[nodiscard]] bool   isLoaded() const { return loaded_; }

private:
    std::unordered_map<Core::GestureType, Action> mapping_;
    bool loaded_ = false;

    static Action          parseAction(const std::string& value);
    static Core::GestureType parseGesture(const std::string& key);
    static ActionType      parseActionType(const std::string& s);
    static std::string     trim(const std::string& s);
};

} // namespace Aura::Input
