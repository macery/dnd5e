#pragma once

#include <string>

namespace dnd {

struct Weapon {
    int weapon_id = 0;
    std::string name;
    int damage_dice_count = 1;
    int damage_dice_sides = 4;
    bool is_finesse = false;
    bool is_ranged = false;
};

}  // namespace dnd
