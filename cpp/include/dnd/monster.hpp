#pragma once

#include <string>

#include "dnd/effects.hpp"
#include "dnd/rules.hpp"

namespace dnd {

// A sample-data stat block, seeded read-only content -- foundation for the
// combat engine. Deliberately excludes skills/saves/proficiency (5e
// monsters' bonuses are hand-picked exceptions, not a PC-style formula) and
// flavor fields nothing in the app uses yet (speed, size, type). Has a
// single attack, matching how these low-CR stat blocks actually present.
struct Monster {
    int monster_id = 0;
    std::string name;
    int armor_class = 10;
    int hit_points = 1;
    int strength = 10;
    int dexterity = 10;
    int constitution = 10;
    int intelligence = 10;
    int wisdom = 10;
    int charisma = 10;
    double challenge_rating = 0.0;

    std::string attack_name;
    int attack_bonus = 0;
    int damage_dice_count = 1;
    int damage_dice_sides = 4;
    int damage_bonus = 0;

    // Some attacks add an effect on a hit: the target makes a save
    // (on_hit_save_dc > 0) or gains on_hit_condition. Wolf: DC 11 STR or prone.
    Ability on_hit_save_ability = Ability::Strength;
    int on_hit_save_dc = 0;  // 0 = no on-hit effect
    Condition on_hit_condition = Condition::Prone;
};

}  // namespace dnd
