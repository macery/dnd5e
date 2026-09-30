#pragma once

#include <array>
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
    std::string background;  // a dnd::BackgroundInfo name; empty if none
    // Failed three death saves (or took massive damage). Stays dead until
    // revived by a spell -- see docs/effects-system.md, Decisions.
    bool dead = false;

    int experience = 0;
    // Spent on short rests, partly recovered on a long rest; the total
    // available is one per level (see dnd/progression.hpp).
    int hit_dice_used = 0;
    // Spell slots spent since they were last restored, indexed by slot
    // level - 1. Totals come from class + level (see dnd/spellcasting.hpp).
    std::array<int, 9> spell_slots_used{};

    // Bit index = static_cast<int>(dnd::Skill) / dnd::Ability. Kept as raw
    // bitmasks here (rather than a join table) because the set of skills and
    // abilities is fixed 5e rules data, not per-character data.
    std::uint32_t skill_proficiencies = 0;
    std::uint32_t save_proficiencies = 0;
};

}  // namespace dnd
