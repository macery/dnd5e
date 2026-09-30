#include "dnd/class_features.hpp"

namespace dnd {

int attacks_per_attack_action(const std::string& class_name, int level) {
    if (class_name == "Fighter") {
        if (level >= 20) return 4;
        if (level >= 11) return 3;
        if (level >= 5) return 2;
        return 1;
    }
    bool extra_attack = class_name == "Barbarian" || class_name == "Paladin" || class_name == "Ranger" ||
                        class_name == "Monk";
    return extra_attack && level >= 5 ? 2 : 1;
}

}  // namespace dnd
