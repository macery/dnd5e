#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/character_creation.hpp"
#include "dnd/class_features.hpp"
#include "dnd/rules.hpp"
#include "dnd/spellcasting.hpp"

namespace dnd {

// Levelling up (2014 PHB ch. 1 "Beyond 1st Level", ch. 3 class tables); see
// docs/level-up.md. XP makes a level available; the player applies it, one
// level at a time, with apply_level_up.

// Levels the character's XP has earned but they haven't taken yet.
int pending_levels(const Character& character);

// --- Class tables ---------------------------------------------------------------

struct ClassFeatureName {
    int level;
    const char* name;  // "Extra Attack", "Brutal Critical (1 die)", "Primal Path feature"
    bool automated;    // the app applies it (otherwise it's shown only)
};

// Every feature on the class's PHB table for levels 1-20, in level order,
// except Ability Score Improvements (see has_ability_score_improvement).
// Empty for an unknown class.
const std::vector<ClassFeatureName>& class_feature_names(const std::string& class_name);
std::vector<ClassFeatureName> class_features_at(const std::string& class_name, int level);

// Levels 4, 8, 12, 16, and 19; Fighters also at 6 and 14, Rogues also at 10.
bool has_ability_score_improvement(const std::string& class_name, int level);

// --- Planning a level ------------------------------------------------------------

// A value that scales with level, before and after ("Rage uses", "2", "3").
struct ScalingChange {
    std::string label;
    std::string before;  // "" if the character didn't have it
    std::string after;
};

// What reaching the next level brings.
struct LevelUpPlan {
    int new_level = 2;
    int hit_die = 8;
    int average_hp = 5;  // hit_die / 2 + 1, before CON
    bool ability_score_improvement = false;
    bool fighting_style = false;  // a Fighting Style is chosen at this level
    std::vector<FightingStyle> fighting_style_options;
    std::vector<ClassFeatureName> features;  // gained at new_level
    bool spellcaster = false;
    SpellAllowance spells_before;
    SpellAllowance spells_after;  // pick the difference in new cantrips/spells
    std::vector<ScalingChange> changes;  // with no ASI applied (see scaling_changes)
};

// Throws std::invalid_argument at level 20.
LevelUpPlan level_up_plan(const Character& character, const ClassInfo& class_info);

enum class HitPointMethod { Average, Roll };

struct LevelUpChoices {
    HitPointMethod hp_method = HitPointMethod::Average;
    int hp_roll = 0;  // the hit die's result, for HitPointMethod::Roll
    // Ability Score Improvement: two picks -- the same ability twice for +2,
    // or two different abilities for +1 each. Empty when there's no ASI.
    std::vector<Ability> ability_increases;
    std::string fighting_style;  // a FightingStyleInfo key, when one is chosen
};

// An error for the player, or nullopt. Checks that the XP has earned the
// level, the HP roll fits the die, an ASI is given exactly when one is due
// and keeps every score at 20 or below, and a due Fighting Style is one the
// class can take.
std::optional<std::string> validate_level_up(const Character& character, const LevelUpPlan& plan,
                                             const LevelUpChoices& choices);

// Max HP gained: the roll or the average, plus the new CON modifier (minimum
// 1), plus 1 per earlier level for each point the CON modifier went up
// (PHB ch. 1).
int level_up_hp_increase(const Character& character, const LevelUpPlan& plan, const LevelUpChoices& choices);

// Applies a validated level: level + 1, the ASI, the Fighting Style, and the
// HP increase to both max and current HP. Throws std::invalid_argument if
// validate_level_up fails. (New spells are saved by the caller, like the
// Spellbook does.)
void apply_level_up(Character& character, const LevelUpPlan& plan, const LevelUpChoices& choices);

// Every scaling value that differs between two versions of a character
// (proficiency bonus, max HP, hit dice, attacks, feature uses, Rage damage,
// Sneak Attack, Martial Arts, Bardic Inspiration, spell slots, spells known).
std::vector<ScalingChange> scaling_changes(const Character& before, const Character& after,
                                           const ClassInfo& class_info);

}  // namespace dnd
