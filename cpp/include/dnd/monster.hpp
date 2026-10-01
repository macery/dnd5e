#pragma once

#include <string>
#include <vector>

#include "dnd/damage.hpp"
#include "dnd/effects.hpp"
#include "dnd/rules.hpp"

namespace dnd {

// One attack in a stat block (MonsterAttacks). An attack can add extra
// dice of another damage type (Hell Hound: + 2d6 fire) and a save against a
// condition on a hit (Wolf: DC 11 STR or prone).
struct MonsterAttack {
    std::string name;
    int attack_bonus = 0;
    int damage_dice_count = 1;
    int damage_dice_sides = 4;
    int damage_bonus = 0;
    std::string damage_type;  // "piercing", ...
    bool ranged = false;
    int extra_dice_count = 0;  // 0 = no extra damage
    int extra_dice_sides = 0;
    std::string extra_damage_type;
    Ability on_hit_save_ability = Ability::Strength;
    int on_hit_save_dc = 0;  // 0 = no on-hit effect
    Condition on_hit_condition = Condition::Prone;
    int count = 1;  // made this many times per turn (Multiattack)
};

// A seeded SRD 5.1 stat block (docs/monsters-and-encounters.md).
// Deliberately excludes skills/saves/proficiency (5e monsters' bonuses are
// hand-picked exceptions, not a PC-style formula) and fields nothing uses
// yet (speed, size).
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

    std::string creature_type;  // "Humanoid", "Undead", "Beast", "Fiend", ...
    DamageTraits damage;
    std::vector<Condition> condition_immunities;
    std::string multiattack;  // summary for the UI ("Two claws and a bite"); empty if none
    std::vector<MonsterAttack> attacks;  // in order; at least one for every seeded monster
};

}  // namespace dnd
