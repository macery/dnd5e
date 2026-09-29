#pragma once

#include <string>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/character_creation.hpp"
#include "dnd/dice.hpp"
#include "dnd/lookup_entry.hpp"

namespace dnd {

constexpr int kMaxLevel = 20;

// XP a single monster is worth, from the 5e XP-by-Challenge-Rating table
// (CR 0 = 10, 1/8 = 25, 1/4 = 50, 1/2 = 100, 1 = 200, ... 30 = 155,000).
// Derived from challenge_rating rather than stored, since it's fixed rules
// data. Throws std::invalid_argument for a CR not on the table.
int xp_for_challenge_rating(double challenge_rating);

// Total XP needed to *be* `level` (0 for level 1, 300 for 2, ..., 355,000
// for 20), per the 5e character advancement table. Throws
// std::invalid_argument outside [1, kMaxLevel].
int xp_threshold_for_level(int level);

// The highest level whose threshold `xp` meets, capped at kMaxLevel.
int level_for_xp(int xp);

// The class's hit die sides (d12 Barbarian; d10 Fighter/Paladin/Ranger;
// d6 Sorcerer/Wizard; d8 everyone else), looked up by the seeded class
// name. Throws std::invalid_argument for an unknown class.
int hit_die_for_class(const std::string& class_name);

// hit_die_for_class for the entry in `classes` with this id -- falling back
// to a d8 (the most common hit die) if the id or name isn't recognized, so
// a UI never crashes on a character with stale class data.
int hit_die_for_class_id(const std::vector<LookupEntry>& classes, int class_id);

// Max HP gained per level after 1st, using the fixed-average option:
// (hit_die / 2 + 1) + CON modifier, never less than 1.
int level_up_hp_gain(int hit_die, int constitution);

// Adds `xp` to character.experience and applies every level-up it earns
// (up to kMaxLevel): level increases, and max HP and current HP both grow
// by level_up_hp_gain per level. Returns the number of levels gained.
int award_experience(Character& character, int xp, int hit_die);

// Hit dice available to spend on a short rest (one per level, minus any
// already spent since the last long rest).
int hit_dice_remaining(const Character& character);

// Short rest: spends one hit die, regaining hit_die roll + CON modifier HP
// (never negative, never above max). Returns the HP actually regained, or
// -1 without changing anything if no hit dice remain.
int spend_hit_die(Character& character, const Dice& dice, int hit_die);

// Long rest: restores HP to max, recovers spent hit dice (up to half the
// character's total, minimum one), and restores every spell slot.
void take_long_rest(Character& character);

// The automatic part of a short rest: a Warlock regains its pact slots.
// (Spending hit dice is the player's choice -- see spend_hit_die.)
void take_short_rest(Character& character, const ClassInfo& class_info);

}  // namespace dnd
