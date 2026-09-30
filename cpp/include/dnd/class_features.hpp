#pragma once

#include <array>
#include <string>
#include <vector>

#include "dnd/character.hpp"

namespace dnd {

// Class features (2014 PHB ch. 3); see docs/class-features.md. Pure rules,
// no state -- the encounter and rests apply them.

// Attacks per Attack action: 2 from level 5 for Fighters, Barbarians,
// Paladins, Rangers, and Monks (Extra Attack); Fighters get 3 at level 11
// and 4 at level 20. Everyone else: 1.
int attacks_per_attack_action(const std::string& class_name, int level);

// --- Features with limited uses ------------------------------------------------

enum class Feature { SecondWind, ActionSurge, Rage, BardicInspiration, ArcaneRecovery };

enum class Recovery { ShortRest, LongRest };

struct FeatureInfo {
    Feature feature;
    const char* key;         // stored in Character::feature_uses_spent
    const char* name;        // "Second Wind"
    const char* class_name;  // "Fighter"
    int min_level;
    const char* description; // for tooltips
};

const std::vector<FeatureInfo>& limited_features();
const FeatureInfo& feature_info(Feature feature);

// True if a character of this class and level has the feature.
bool has_feature(Feature feature, const std::string& class_name, int level);

// Uses per rest (0 without the feature). Rage is effectively unlimited at
// level 20 (99). Bardic Inspiration: CHA modifier, minimum 1.
int feature_max_uses(Feature feature, const Character& character, const std::string& class_name);
int feature_uses_left(Feature feature, const Character& character, const std::string& class_name);
// When the feature comes back: Bardic Inspiration moves to short rests at
// level 5 (Font of Inspiration); the rest are fixed.
Recovery feature_recovery(Feature feature, int level);

// Spends one use; false (changing nothing) if none are left.
bool spend_feature(Character& character, Feature feature, const std::string& class_name);

// Short rest: features that recover on a short rest come back.
void restore_short_rest_features(Character& character, const std::string& class_name);
// Long rest: every feature comes back.
void restore_all_features(Character& character);

// --- Formulas --------------------------------------------------------------------

int rage_damage_bonus(int level);         // +2, +3 at 9, +4 at 16
int sneak_attack_dice(int level);         // d6s: 1 per 2 levels, rounded up
int martial_arts_die(int level);          // d4, d6 at 5, d8 at 11, d10 at 17
int bardic_inspiration_die(int level);    // d6, d8 at 5, d10 at 10, d12 at 15
int second_wind_bonus(int level);         // Second Wind heals 1d10 + this (fighter level)
int arcane_recovery_budget(int level);    // combined slot levels: half the level, rounded up

// Arcane Recovery (Wizard): recovers spent slots, highest first, with combined
// levels up to the budget and none of 6th level or higher. Spends the use.
// Returns the slots recovered by level (index = slot level - 1); all zeros
// if the feature isn't available or nothing was spent.
std::array<int, 9> use_arcane_recovery(Character& character, const std::string& class_name);

}  // namespace dnd
