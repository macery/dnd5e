#include "dnd/class_features.hpp"

#include <algorithm>
#include <stdexcept>

#include "dnd/character_creation.hpp"
#include "dnd/rules.hpp"
#include "dnd/spellcasting.hpp"

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
        {Feature::Ki, "Ki", "Ki", "Monk", 2,
         "Ki points equal to your monk level, back on a short rest. Flurry of Blows (after the Attack action, "
         "two unarmed strikes), Patient Defense (Dodge), or Step of the Wind (Dash or Disengage): 1 ki each, "
         "as a bonus action."},
        {Feature::LayOnHands, "LayOnHands", "Lay on Hands", "Paladin", 1,
         "Action: touch a creature and restore hit points from a pool of 5 x your paladin level, back on a "
         "long rest."},
        {Feature::ChannelDivinity, "ChannelDivinity", "Channel Divinity", "Cleric", 2,
         "Action: channel divine energy -- Turn Undead (undead that fail a WIS save are turned for a minute), or "
         "your domain's or oath's option. Back on a short rest."},
        {Feature::SorceryPoints, "SorceryPoints", "Sorcery Points", "Sorcerer", 2,
         "Font of Magic: points equal to your sorcerer level, back on a long rest. Spend them on Metamagic, or to "
         "create spell slots; turn unused slots back into points."},
        {Feature::NaturalRecovery, "NaturalRecovery", "Natural Recovery", "Druid", 2,
         "Circle of the Land: once per day, on a short rest, recover spell slots with combined levels up to half "
         "your druid level (rounded up), none of 6th level or higher."},
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
    if (feature == Feature::ChannelDivinity) {
        return (class_name == "Cleric" && level >= 2) || (class_name == "Paladin" && level >= 3);
    }
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
        case Feature::Ki: return level;
        case Feature::LayOnHands: return 5 * level;
        case Feature::ChannelDivinity:
            if (class_name == "Paladin") return 1;
            return level >= 18 ? 3 : level >= 6 ? 2 : 1;
        case Feature::NaturalRecovery: return character.subclass == "Circle of the Land" ? 1 : 0;
        case Feature::SorceryPoints: return level;
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
        case Feature::ActionSurge:
        case Feature::ChannelDivinity:
        case Feature::Ki: return Recovery::ShortRest;
        case Feature::BardicInspiration: return level >= 5 ? Recovery::ShortRest : Recovery::LongRest;
        case Feature::Rage:
        case Feature::ArcaneRecovery:
        case Feature::NaturalRecovery:
        case Feature::SorceryPoints:
        case Feature::LayOnHands: return Recovery::LongRest;
    }
    return Recovery::LongRest;
}

bool spend_feature(Character& character, Feature feature, const std::string& class_name, int amount) {
    if (amount <= 0 || feature_uses_left(feature, character, class_name) < amount) return false;
    character.feature_uses_spent[feature_info(feature).key] += amount;
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

// --- Levels 1-5 ----------------------------------------------------------------------

int jack_of_all_trades(const Character& character, const std::string& class_name) {
    return class_name == "Bard" && character.level >= 2 ? proficiency_bonus(character.level) / 2 : 0;
}

int song_of_rest_die(int bard_level) {
    if (bard_level >= 17) return 12;
    if (bard_level >= 13) return 10;
    if (bard_level >= 9) return 8;
    if (bard_level >= 2) return 6;
    return 0;
}

int slot_creation_cost(int slot_level) {
    static const int kCosts[] = {0, 2, 3, 5, 6, 7};
    return slot_level >= 1 && slot_level <= 5 ? kCosts[slot_level] : 0;
}

bool create_spell_slot(Character& character, const std::string& class_name, int slot_level) {
    int cost = slot_creation_cost(slot_level);
    if (cost == 0 || !spend_feature(character, Feature::SorceryPoints, class_name, cost)) return false;
    --character.spell_slots_used[static_cast<size_t>(slot_level - 1)];  // negative: an extra slot
    return true;
}

bool convert_slot_to_points(Character& character, const std::string& class_name, int slot_level) {
    if (slot_level < 1 || slot_level > 9 || feature_max_uses(Feature::SorceryPoints, character, class_name) == 0) {
        return false;
    }
    auto spent = character.feature_uses_spent.find(feature_info(Feature::SorceryPoints).key);
    if (spent == character.feature_uses_spent.end() || spent->second <= 0) return false;  // already full
    const ClassInfo* info = find_class_info(class_name);
    if (!info || slots_remaining(character, *info, slot_level) == 0) return false;
    ++character.spell_slots_used[static_cast<size_t>(slot_level - 1)];
    spent->second = std::max(0, spent->second - slot_level);
    return true;
}

namespace {

struct MetamagicInfo {
    Metamagic metamagic;
    const char* name;
    const char* description;
    bool automated;
};

const std::vector<MetamagicInfo>& metamagic_table() {
    static const std::vector<MetamagicInfo> table = {
        {Metamagic::Careful, "Careful Spell",
         "1 point: allies in the spell's area succeed on its save. (Area spells here only hit the targets you "
         "choose, so this rarely matters.)",
         false},
        {Metamagic::Distant, "Distant Spell", "1 point: double the spell's range. (Shown only: there's no map.)", false},
        {Metamagic::Empowered, "Empowered Spell",
         "1 point: reroll damage dice up to your CHA modifier (the low ones), keeping the new rolls.", true},
        {Metamagic::Extended, "Extended Spell", "1 point: double a duration of 1 minute or more. (Shown only.)", false},
        {Metamagic::Heightened, "Heightened Spell",
         "3 points: one target has disadvantage on its first save against the spell.", true},
        {Metamagic::Quickened, "Quickened Spell",
         "2 points: cast a spell that takes an action as a bonus action instead.", true},
        {Metamagic::Subtle, "Subtle Spell", "1 point: cast without words or gestures. (Shown only.)", false},
        {Metamagic::Twinned, "Twinned Spell",
         "Points equal to the spell's level (1 for a cantrip): a spell that targets one creature targets a second.",
         true},
    };
    return table;
}

const MetamagicInfo* metamagic_info(Metamagic metamagic) {
    for (const auto& info : metamagic_table()) {
        if (info.metamagic == metamagic) return &info;
    }
    return nullptr;
}

}  // namespace

const char* metamagic_name(Metamagic metamagic) {
    const MetamagicInfo* info = metamagic_info(metamagic);
    return info ? info->name : "";
}

Metamagic metamagic_from_name(const std::string& name) {
    for (const auto& info : metamagic_table()) {
        if (name == info.name) return info.metamagic;
    }
    return Metamagic::None;
}

std::vector<Metamagic> all_metamagic() {
    std::vector<Metamagic> all;
    for (const auto& info : metamagic_table()) all.push_back(info.metamagic);
    return all;
}

const char* metamagic_description(Metamagic metamagic) {
    const MetamagicInfo* info = metamagic_info(metamagic);
    return info ? info->description : "";
}

bool metamagic_automated(Metamagic metamagic) {
    const MetamagicInfo* info = metamagic_info(metamagic);
    return info && info->automated;
}

int metamagic_cost(Metamagic metamagic, int spell_level) {
    switch (metamagic) {
        case Metamagic::None: return 0;
        case Metamagic::Heightened: return 3;
        case Metamagic::Quickened: return 2;
        case Metamagic::Twinned: return std::max(1, spell_level);
        default: return 1;
    }
}

int metamagic_known(int sorcerer_level) {
    if (sorcerer_level >= 17) return 4;
    if (sorcerer_level >= 10) return 3;
    if (sorcerer_level >= 3) return 2;
    return 0;
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

int divine_smite_dice(int slot_level) {
    return std::min(5, slot_level + 1);
}

std::array<int, 9> use_arcane_recovery(Character& character, const std::string& class_name) {
    std::array<int, 9> recovered{};
    // A Land druid's Natural Recovery works the same way.
    Feature feature = class_name == "Druid" ? Feature::NaturalRecovery : Feature::ArcaneRecovery;
    if (feature_uses_left(feature, character, class_name) <= 0) return recovered;

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
    if (any) spend_feature(character, feature, class_name);
    return recovered;
}

// --- Fighting Style ------------------------------------------------------------------

const std::vector<FightingStyleInfo>& fighting_styles() {
    static const std::vector<FightingStyleInfo> styles = {
        {FightingStyle::Archery, "Archery", "Archery", "+2 to attack rolls with ranged weapons."},
        {FightingStyle::Defense, "Defense", "Defense", "+1 AC while wearing armor."},
        {FightingStyle::Dueling, "Dueling", "Dueling",
         "+2 damage with a melee weapon wielded in one hand and no other weapon."},
        {FightingStyle::GreatWeaponFighting, "GreatWeaponFighting", "Great Weapon Fighting",
         "Reroll 1s and 2s on the damage dice of a melee weapon wielded with two hands (keep the new roll)."},
    };
    return styles;
}

int fighting_style_level(const std::string& class_name) {
    if (class_name == "Fighter") return 1;
    if (class_name == "Paladin" || class_name == "Ranger") return 2;
    return 0;
}

std::vector<FightingStyle> fighting_styles_for(const std::string& class_name) {
    using S = FightingStyle;
    if (class_name == "Fighter") return {S::Archery, S::Defense, S::Dueling, S::GreatWeaponFighting};
    if (class_name == "Paladin") return {S::Defense, S::Dueling, S::GreatWeaponFighting};
    if (class_name == "Ranger") return {S::Archery, S::Defense, S::Dueling};
    return {};
}

FightingStyle fighting_style(const Character& character, const std::string& class_name) {
    int level = fighting_style_level(class_name);
    if (level == 0 || character.level < level) return FightingStyle::None;
    auto allowed = fighting_styles_for(class_name);
    for (const auto& info : fighting_styles()) {
        if (character.fighting_style == info.key &&
            std::find(allowed.begin(), allowed.end(), info.style) != allowed.end()) {
            return info.style;
        }
    }
    return FightingStyle::None;
}

const FightingStyleInfo* fighting_style_info(FightingStyle style) {
    for (const auto& info : fighting_styles()) {
        if (info.style == style) return &info;
    }
    return nullptr;
}

}  // namespace dnd
