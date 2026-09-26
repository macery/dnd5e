#pragma once

#include "dnd/character.hpp"
#include "dnd/weapon.hpp"

namespace dnd {

enum class Ability { Strength, Dexterity, Constitution, Intelligence, Wisdom, Charisma };

enum class Skill {
    Acrobatics,
    AnimalHandling,
    Arcana,
    Athletics,
    Deception,
    History,
    Insight,
    Intimidation,
    Investigation,
    Medicine,
    Nature,
    Perception,
    Performance,
    Persuasion,
    Religion,
    SleightOfHand,
    Stealth,
    Survival,
};

constexpr int kAbilityCount = 6;
constexpr int kSkillCount = 18;

const char* ability_name(Ability ability);
const char* skill_name(Skill skill);

// A short, beginner-facing explanation of what the ability/skill covers and
// when you'd roll it -- used for in-app help (tooltips), not gameplay math.
const char* ability_description(Ability ability);
const char* skill_description(Skill skill);

// The fixed 5e skill -> governing ability mapping (e.g. Stealth -> Dexterity).
Ability ability_for_skill(Skill skill);

// floor((score - 10) / 2), matching the 5e ability modifier table exactly
// (including for odd scores and scores below 10).
int ability_modifier(int score);

// 2 + floor((level - 1) / 4): +2 at levels 1-4, +3 at 5-8, ..., +6 at 17-20.
int proficiency_bonus(int level);

int ability_score(const Character& character, Ability ability);
bool has_skill_proficiency(const Character& character, Skill skill);
bool has_save_proficiency(const Character& character, Ability ability);

// Ability modifier, plus the character's proficiency bonus if proficient.
int skill_bonus(const Character& character, Skill skill);
int saving_throw_bonus(const Character& character, Ability ability);

int passive_perception(const Character& character);
int initiative(const Character& character);

// DEX modifier if the weapon is ranged, or finesse and DEX >= STR;
// STR modifier otherwise. There's no proficiency-with-a-specific-weapon
// tracking yet, so this assumes the character is proficient with whatever
// they have equipped.
int weapon_attack_ability_modifier(const Character& character, const Weapon& weapon);

// proficiency_bonus(level) + weapon_attack_ability_modifier(...).
int weapon_attack_bonus(const Character& character, const Weapon& weapon);

}  // namespace dnd
