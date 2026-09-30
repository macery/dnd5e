#include "dnd/class_features.hpp"

#include <algorithm>
#include <stdexcept>

#include "dnd/rules.hpp"

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

// --- Features with limited uses ------------------------------------------------

const std::vector<FeatureInfo>& limited_features() {
    static const std::vector<FeatureInfo> features = {
        {Feature::SecondWind, "SecondWind", "Second Wind", "Fighter", 1,
         "Bonus action: regain 1d10 + fighter level hit points. Once per short rest."},
        {Feature::ActionSurge, "ActionSurge", "Action Surge", "Fighter", 2,
         "On your turn, take one additional action. Once per short rest."},
        {Feature::Rage, "Rage", "Rage", "Barbarian", 1,
         "Bonus action, 1 minute: advantage on STR checks and saves, extra damage on STR melee weapon attacks, "
         "and resistance to bludgeoning, piercing, and slashing damage. No spellcasting or concentration. Ends "
         "early if you're knocked out, or if a turn ends without you attacking or taking damage."},
        {Feature::BardicInspiration, "BardicInspiration", "Bardic Inspiration", "Bard", 1,
         "Bonus action: give an ally an inspiration die to add to their next attack roll or saving throw "
         "within 10 minutes. Uses equal your CHA modifier (minimum 1)."},
        {Feature::ArcaneRecovery, "ArcaneRecovery", "Arcane Recovery", "Wizard", 1,
         "Once per day, on a short rest: recover spell slots with combined levels up to half your wizard level "
         "(rounded up), none of 6th level or higher."},
    };
    return features;
}

const FeatureInfo& feature_info(Feature feature) {
    for (const auto& info : limited_features()) {
        if (info.feature == feature) return info;
    }
    throw std::invalid_argument("unknown feature");
}

bool has_feature(Feature feature, const std::string& class_name, int level) {
    const FeatureInfo& info = feature_info(feature);
    return class_name == info.class_name && level >= info.min_level;
}

int feature_max_uses(Feature feature, const Character& character, const std::string& class_name) {
    if (!has_feature(feature, class_name, character.level)) return 0;
    int level = character.level;
    switch (feature) {
        case Feature::SecondWind: return 1;
        case Feature::ActionSurge: return level >= 17 ? 2 : 1;
        case Feature::Rage:
            if (level >= 20) return 99;
            if (level >= 17) return 6;
            if (level >= 12) return 5;
            if (level >= 6) return 4;
            if (level >= 3) return 3;
            return 2;
        case Feature::BardicInspiration: return std::max(1, ability_modifier(character.charisma));
        case Feature::ArcaneRecovery: return 1;
    }
    return 0;
}

int feature_uses_left(Feature feature, const Character& character, const std::string& class_name) {
    auto spent = character.feature_uses_spent.find(feature_info(feature).key);
    int used = spent == character.feature_uses_spent.end() ? 0 : spent->second;
    return std::max(0, feature_max_uses(feature, character, class_name) - used);
}

Recovery feature_recovery(Feature feature, int level) {
    switch (feature) {
        case Feature::SecondWind:
        case Feature::ActionSurge: return Recovery::ShortRest;
        case Feature::BardicInspiration: return level >= 5 ? Recovery::ShortRest : Recovery::LongRest;
        case Feature::Rage:
        case Feature::ArcaneRecovery: return Recovery::LongRest;
    }
    return Recovery::LongRest;
}

bool spend_feature(Character& character, Feature feature, const std::string& class_name) {
    if (feature_uses_left(feature, character, class_name) <= 0) return false;
    ++character.feature_uses_spent[feature_info(feature).key];
    return true;
}

void restore_short_rest_features(Character& character, const std::string& class_name) {
    for (const auto& info : limited_features()) {
        if (!has_feature(info.feature, class_name, character.level)) continue;
        if (feature_recovery(info.feature, character.level) == Recovery::ShortRest) {
            character.feature_uses_spent.erase(info.key);
        }
    }
}

void restore_all_features(Character& character) {
    character.feature_uses_spent.clear();
}

// --- Formulas --------------------------------------------------------------------

int rage_damage_bonus(int level) {
    if (level >= 16) return 4;
    if (level >= 9) return 3;
    return 2;
}

int sneak_attack_dice(int level) {
    return (level + 1) / 2;
}

int martial_arts_die(int level) {
    if (level >= 17) return 10;
    if (level >= 11) return 8;
    if (level >= 5) return 6;
    return 4;
}

int bardic_inspiration_die(int level) {
    if (level >= 15) return 12;
    if (level >= 10) return 10;
    if (level >= 5) return 8;
    return 6;
}

int second_wind_bonus(int level) {
    return level;
}

int arcane_recovery_budget(int level) {
    return (level + 1) / 2;
}

std::array<int, 9> use_arcane_recovery(Character& character, const std::string& class_name) {
    std::array<int, 9> recovered{};
    if (feature_uses_left(Feature::ArcaneRecovery, character, class_name) <= 0) return recovered;

    int budget = arcane_recovery_budget(character.level);
    bool any = false;
    for (int level = 5; level >= 1 && budget > 0; --level) {  // none of 6th level or higher
        auto index = static_cast<size_t>(level - 1);
        while (character.spell_slots_used[index] > 0 && level <= budget) {
            --character.spell_slots_used[index];
            ++recovered[index];
            budget -= level;
            any = true;
        }
    }
    if (any) spend_feature(character, Feature::ArcaneRecovery, class_name);
    return recovered;
}

}  // namespace dnd
