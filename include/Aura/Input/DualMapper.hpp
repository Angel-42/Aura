#pragma once
#include "Aura/Core/GestureEvent.hpp"
#include "Aura/Input/Mapper.hpp"
#include <filesystem>
#include <map>
#include <optional>
#include <utility>

namespace Aura::Input {

// Mapping de paires de gestes (main gauche + main droite) → Action.
// Format du fichier de config (config/dual.txt) :
//   LEFT_GESTURE + RIGHT_GESTURE = ACTION [PARAMS]
//   ex : FIST + FIST = KEY_COMBO CMD Z
class DualMapper {
public:
    bool load(const std::filesystem::path& path);

    // Retourne l'action liée à la combinaison (left, right), ou nullopt.
    [[nodiscard]] std::optional<Action> lookup(Core::GestureType left,
                                               Core::GestureType right) const;
    [[nodiscard]] bool empty() const { return bindings_.empty(); }

private:
    std::map<std::pair<Core::GestureType, Core::GestureType>, Action> bindings_;

    static Action             parseAction(const std::string& value);
    static ActionType         parseActionType(const std::string& s);
    static std::string        trim(const std::string& s);
};

} // namespace Aura::Input
