#pragma once

#include <cstdint>
#include <string>

namespace dnd {

struct Character {
    int character_id = 0;
    std::string character_name;
    int class_id = 0;
    int race = 0;
    int alignment = 0;
    int player_id = 0;
    int level = 1;

    int strength = 10;
    int dexterity = 10;
    int constitution = 10;
    int intelligence = 10;
    int wisdom = 10;
    int charisma = 10;

    int current_hp = 0;
    int max_hp = 0;
    int armor_class = 10;
    int weapon_id = 0;

    // Bit index = static_cast<int>(dnd::Skill) / dnd::Ability. Kept as raw
    // bitmasks here (rather than a join table) because the set of skills and
    // abilities is fixed 5e rules data, not per-character data.
    std::uint32_t skill_proficiencies = 0;
    std::uint32_t save_proficiencies = 0;
};

}  // namespace dnd
