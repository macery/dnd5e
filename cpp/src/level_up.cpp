#include "dnd/level_up.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>

#include "dnd/progression.hpp"
#include "dnd/subclass.hpp"

namespace dnd {

int pending_levels(const Character& character) {
    return std::max(0, level_for_xp(character.experience) - character.level);
}

// --- Class tables ---------------------------------------------------------------

namespace {

constexpr bool kAuto = true;
constexpr bool kShown = false;

// PHB ch. 3, the Features column of each class table (Ability Score
// Improvements left out). "automated" marks what the app applies.
const std::map<std::string, std::vector<ClassFeatureName>>& feature_tables() {
    static const std::map<std::string, std::vector<ClassFeatureName>> tables = {
        {"Barbarian",
         {{1, "Rage", kAuto}, {1, "Unarmored Defense", kAuto}, {2, "Reckless Attack", kAuto},
          {2, "Danger Sense", kShown}, {3, "Primal Path", kAuto}, {5, "Extra Attack", kAuto},
          {5, "Fast Movement", kShown}, {6, "Primal Path feature", kShown}, {7, "Feral Instinct", kShown},
          {9, "Brutal Critical (1 die)", kShown}, {10, "Primal Path feature", kShown},
          {11, "Relentless Rage", kShown}, {13, "Brutal Critical (2 dice)", kShown},
          {14, "Primal Path feature", kShown}, {15, "Persistent Rage", kShown},
          {17, "Brutal Critical (3 dice)", kShown}, {18, "Indomitable Might", kShown},
          {20, "Primal Champion", kShown}}},
        {"Bard",
         {{1, "Spellcasting", kAuto}, {1, "Bardic Inspiration (d6)", kAuto}, {2, "Jack of All Trades", kShown},
          {2, "Song of Rest (d6)", kShown}, {3, "Bard College", kAuto}, {3, "Expertise", kShown},
          {5, "Bardic Inspiration (d8)", kAuto}, {5, "Font of Inspiration", kAuto}, {6, "Countercharm", kShown},
          {6, "Bard College feature", kShown}, {9, "Song of Rest (d8)", kShown},
          {10, "Bardic Inspiration (d10)", kAuto}, {10, "Expertise", kShown}, {10, "Magical Secrets", kShown},
          {13, "Song of Rest (d10)", kShown}, {14, "Magical Secrets", kShown},
          {14, "Bard College feature", kShown}, {15, "Bardic Inspiration (d12)", kAuto},
          {17, "Song of Rest (d12)", kShown}, {18, "Magical Secrets", kShown},
          {20, "Superior Inspiration", kShown}}},
        {"Cleric",
         {{1, "Spellcasting", kAuto}, {1, "Divine Domain", kAuto}, {2, "Channel Divinity (1/rest)", kAuto},
          {2, "Divine Domain feature", kShown}, {5, "Destroy Undead (CR 1/2)", kAuto},
          {6, "Channel Divinity (2/rest)", kAuto}, {6, "Divine Domain feature", kShown},
          {8, "Destroy Undead (CR 1)", kShown}, {8, "Divine Domain feature", kShown},
          {10, "Divine Intervention", kShown}, {11, "Destroy Undead (CR 2)", kShown},
          {14, "Destroy Undead (CR 3)", kShown}, {17, "Destroy Undead (CR 4)", kShown},
          {17, "Divine Domain feature", kShown}, {18, "Channel Divinity (3/rest)", kAuto},
          {20, "Divine Intervention improvement", kShown}}},
        {"Druid",
         {{1, "Druidic", kShown}, {1, "Spellcasting", kAuto}, {2, "Wild Shape", kShown},
          {2, "Druid Circle", kAuto}, {4, "Wild Shape improvement", kShown}, {6, "Druid Circle feature", kShown},
          {8, "Wild Shape improvement", kShown}, {10, "Druid Circle feature", kShown},
          {14, "Druid Circle feature", kShown}, {18, "Timeless Body", kShown}, {18, "Beast Spells", kShown},
          {20, "Archdruid", kShown}}},
        {"Fighter",
         {{1, "Fighting Style", kAuto}, {1, "Second Wind", kAuto}, {2, "Action Surge (one use)", kAuto},
          {3, "Martial Archetype", kAuto}, {5, "Extra Attack", kAuto}, {7, "Martial Archetype feature", kShown},
          {9, "Indomitable (one use)", kShown}, {10, "Martial Archetype feature", kShown},
          {11, "Extra Attack (2)", kAuto}, {13, "Indomitable (two uses)", kShown},
          {15, "Martial Archetype feature", kShown}, {17, "Action Surge (two uses)", kAuto},
          {17, "Indomitable (three uses)", kShown}, {18, "Martial Archetype feature", kShown},
          {20, "Extra Attack (3)", kAuto}}},
        {"Monk",
         {{1, "Unarmored Defense", kAuto}, {1, "Martial Arts", kAuto}, {2, "Ki", kAuto},
          {2, "Unarmored Movement", kShown}, {3, "Monastic Tradition", kAuto}, {3, "Deflect Missiles", kShown},
          {4, "Slow Fall", kShown}, {5, "Extra Attack", kAuto}, {5, "Stunning Strike", kShown},
          {6, "Ki-Empowered Strikes", kShown}, {6, "Monastic Tradition feature", kShown}, {7, "Evasion", kShown},
          {7, "Stillness of Mind", kShown}, {9, "Unarmored Movement improvement", kShown},
          {10, "Purity of Body", kShown}, {11, "Monastic Tradition feature", kShown},
          {13, "Tongue of the Sun and Moon", kShown}, {14, "Diamond Soul", kShown}, {15, "Timeless Body", kShown},
          {17, "Monastic Tradition feature", kShown}, {18, "Empty Body", kShown}, {20, "Perfect Self", kShown}}},
        {"Paladin",
         {{1, "Divine Sense", kShown}, {1, "Lay on Hands", kAuto}, {2, "Fighting Style", kAuto},
          {2, "Spellcasting", kAuto}, {2, "Divine Smite", kAuto}, {3, "Divine Health", kShown},
          {3, "Sacred Oath", kAuto}, {5, "Extra Attack", kAuto}, {6, "Aura of Protection", kShown},
          {7, "Sacred Oath feature", kShown}, {10, "Aura of Courage", kShown},
          {11, "Improved Divine Smite", kShown}, {14, "Cleansing Touch", kShown},
          {15, "Sacred Oath feature", kShown}, {18, "Aura improvements", kShown},
          {20, "Sacred Oath feature", kShown}}},
        {"Ranger",
         {{1, "Favored Enemy", kShown}, {1, "Natural Explorer", kShown}, {2, "Fighting Style", kAuto},
          {2, "Spellcasting", kAuto}, {3, "Ranger Archetype", kAuto}, {3, "Primeval Awareness", kShown},
          {5, "Extra Attack", kAuto}, {6, "Favored Enemy and Natural Explorer improvements", kShown},
          {7, "Ranger Archetype feature", kShown}, {8, "Land's Stride", kShown},
          {10, "Natural Explorer improvement", kShown}, {10, "Hide in Plain Sight", kShown},
          {11, "Ranger Archetype feature", kShown}, {14, "Favored Enemy improvement", kShown},
          {14, "Vanish", kShown}, {15, "Ranger Archetype feature", kShown}, {18, "Feral Senses", kShown},
          {20, "Foe Slayer", kShown}}},
        {"Rogue",
         {{1, "Expertise", kShown}, {1, "Sneak Attack", kAuto}, {1, "Thieves' Cant", kShown},
          {2, "Cunning Action", kAuto}, {3, "Roguish Archetype", kAuto}, {5, "Uncanny Dodge", kAuto},
          {6, "Expertise", kShown}, {7, "Evasion", kShown}, {9, "Roguish Archetype feature", kShown},
          {11, "Reliable Talent", kShown}, {13, "Roguish Archetype feature", kShown}, {14, "Blindsense", kShown},
          {15, "Slippery Mind", kShown}, {17, "Roguish Archetype feature", kShown}, {18, "Elusive", kShown},
          {20, "Stroke of Luck", kShown}}},
        {"Sorcerer",
         {{1, "Spellcasting", kAuto}, {1, "Sorcerous Origin", kAuto}, {2, "Font of Magic", kShown},
          {3, "Metamagic", kShown}, {6, "Sorcerous Origin feature", kShown}, {10, "Metamagic", kShown},
          {14, "Sorcerous Origin feature", kShown}, {17, "Metamagic", kShown},
          {18, "Sorcerous Origin feature", kShown}, {20, "Sorcerous Restoration", kShown}}},
        {"Warlock",
         {{1, "Otherworldly Patron", kAuto}, {1, "Pact Magic", kAuto}, {2, "Eldritch Invocations", kShown},
          {3, "Pact Boon", kShown}, {6, "Otherworldly Patron feature", kShown},
          {10, "Otherworldly Patron feature", kShown}, {11, "Mystic Arcanum (6th level)", kShown},
          {13, "Mystic Arcanum (7th level)", kShown}, {14, "Otherworldly Patron feature", kShown},
          {15, "Mystic Arcanum (8th level)", kShown}, {17, "Mystic Arcanum (9th level)", kShown},
          {20, "Eldritch Master", kShown}}},
        {"Wizard",
         {{1, "Spellcasting", kAuto}, {1, "Arcane Recovery", kAuto}, {2, "Arcane Tradition", kAuto},
          {6, "Arcane Tradition feature", kShown}, {10, "Arcane Tradition feature", kShown},
          {14, "Arcane Tradition feature", kShown}, {18, "Spell Mastery", kShown},
          {20, "Signature Spells", kShown}}},
    };
    return tables;
}

int& score_of(Character& character, Ability ability) {
    switch (ability) {
        case Ability::Strength: return character.strength;
        case Ability::Dexterity: return character.dexterity;
        case Ability::Constitution: return character.constitution;
        case Ability::Intelligence: return character.intelligence;
        case Ability::Wisdom: return character.wisdom;
        case Ability::Charisma: return character.charisma;
    }
    return character.strength;
}

std::string signed_text(int value) {
    return (value >= 0 ? "+" : "") + std::to_string(value);
}

// Every scaling value for this character, labelled, in display order.
std::vector<std::pair<std::string, std::string>> scaling_values(const Character& character,
                                                                const ClassInfo& class_info) {
    std::vector<std::pair<std::string, std::string>> values;
    const std::string& name = class_info.name;
    int level = character.level;
    values.push_back({"Proficiency bonus", signed_text(proficiency_bonus(level))});
    values.push_back({"Max HP", std::to_string(character.max_hp)});
    values.push_back({"Hit dice", std::to_string(level) + "d" + std::to_string(class_info.hit_die)});
    int attacks = attacks_per_attack_action(name, level);
    if (attacks > 1) values.push_back({"Attacks per Attack action", std::to_string(attacks)});

    for (const auto& info : limited_features()) {
        if (!has_feature(info.feature, name, level)) continue;
        int max = feature_max_uses(info.feature, character, name);
        std::string label = info.feature == Feature::Ki           ? "Ki points"
                            : info.feature == Feature::LayOnHands ? "Lay on Hands pool (HP)"
                                                                  : std::string(info.name) + " uses";
        values.push_back({label, max >= 99 ? "unlimited" : std::to_string(max)});
    }
    if (name == "Barbarian") values.push_back({"Rage damage", "+" + std::to_string(rage_damage_bonus(level))});
    if (name == "Rogue") values.push_back({"Sneak Attack", std::to_string(sneak_attack_dice(level)) + "d6"});
    if (name == "Monk") values.push_back({"Martial Arts die", "d" + std::to_string(martial_arts_die(level))});
    if (name == "Bard") {
        values.push_back({"Bardic Inspiration die", "d" + std::to_string(bardic_inspiration_die(level))});
    }

    if (is_spellcaster(class_info)) {
        SpellAllowance allowance = spell_allowance(character, class_info);
        if (allowance.cantrips > 0) values.push_back({"Cantrips known", std::to_string(allowance.cantrips)});
        bool prepared = class_info.spellcasting.preparation != SpellPreparation::Known;
        values.push_back({prepared ? "Spells prepared" : "Spells known", std::to_string(allowance.spells)});
        auto slots = spell_slots(class_info, level);
        std::string text;
        for (int slot = 1; slot <= kMaxSpellLevel; ++slot) {
            int count = slots[static_cast<size_t>(slot - 1)];
            if (count == 0) continue;
            if (!text.empty()) text += ", ";
            text += std::to_string(count) + " × " + ordinal(slot);
        }
        values.push_back({"Spell slots", text.empty() ? "none" : text});
    }
    return values;
}

}  // namespace

const std::vector<ClassFeatureName>& class_feature_names(const std::string& class_name) {
    static const std::vector<ClassFeatureName> none;
    auto found = feature_tables().find(class_name);
    return found == feature_tables().end() ? none : found->second;
}

std::vector<ClassFeatureName> class_features_at(const std::string& class_name, int level) {
    std::vector<ClassFeatureName> features;
    for (const auto& feature : class_feature_names(class_name)) {
        if (feature.level == level) features.push_back(feature);
    }
    return features;
}

bool has_ability_score_improvement(const std::string& class_name, int level) {
    if (level == 4 || level == 8 || level == 12 || level == 16 || level == 19) return true;
    if (class_name == "Fighter" && (level == 6 || level == 14)) return true;
    return class_name == "Rogue" && level == 10;
}

// --- Planning a level ------------------------------------------------------------

std::vector<ScalingChange> scaling_changes(const Character& before, const Character& after,
                                           const ClassInfo& class_info) {
    auto old_values = scaling_values(before, class_info);
    std::vector<ScalingChange> changes;
    for (const auto& [label, value] : scaling_values(after, class_info)) {
        auto old = std::find_if(old_values.begin(), old_values.end(),
                                [&](const auto& entry) { return entry.first == label; });
        std::string previous = old == old_values.end() ? "" : old->second;
        if (previous != value) changes.push_back({label, previous, value});
    }
    return changes;
}

LevelUpPlan level_up_plan(const Character& character, const ClassInfo& class_info) {
    if (character.level >= kMaxLevel) throw std::invalid_argument("already at the highest level");
    LevelUpPlan plan;
    plan.new_level = character.level + 1;
    plan.hit_die = class_info.hit_die;
    plan.average_hp = class_info.hit_die / 2 + 1;
    plan.ability_score_improvement = has_ability_score_improvement(class_info.name, plan.new_level);
    plan.fighting_style = fighting_style_level(class_info.name) == plan.new_level;
    if (plan.fighting_style) plan.fighting_style_options = fighting_styles_for(class_info.name);
    plan.features = class_features_at(class_info.name, plan.new_level);
    if (!character.subclass.empty()) {
        // The subclass's own features replace the table's "Divine Domain feature" placeholders.
        std::string placeholder = subclass_kind(class_info.name) + " feature";
        plan.features.erase(std::remove_if(plan.features.begin(), plan.features.end(),
                                           [&](const ClassFeatureName& f) { return placeholder == f.name; }),
                            plan.features.end());
        for (const auto& feature : subclass_features_at(character.subclass, plan.new_level)) {
            plan.features.push_back(feature);
        }
    }

    Character after = character;
    after.level = plan.new_level;
    after.max_hp += std::max(1, plan.average_hp + ability_modifier(character.constitution));
    plan.spellcaster = is_spellcaster(class_info);
    if (plan.spellcaster) {
        plan.spells_before = spell_allowance(character, class_info);
        plan.spells_after = spell_allowance(after, class_info);
    }
    plan.changes = scaling_changes(character, after, class_info);
    return plan;
}

std::optional<std::string> validate_level_up(const Character& character, const LevelUpPlan& plan,
                                             const LevelUpChoices& choices) {
    if (character.level >= kMaxLevel) return character.character_name + " is already level 20.";
    if (plan.new_level != character.level + 1) return "This level-up plan is out of date.";
    if (pending_levels(character) <= 0) {
        return character.character_name + " needs " + std::to_string(xp_threshold_for_level(plan.new_level)) +
               " XP for level " + std::to_string(plan.new_level) + ".";
    }
    if (choices.hp_method == HitPointMethod::Roll && (choices.hp_roll < 1 || choices.hp_roll > plan.hit_die)) {
        return "Roll your hit die (1 to " + std::to_string(plan.hit_die) + ").";
    }

    if (plan.ability_score_improvement) {
        if (choices.ability_increases.size() != 2) {
            return std::string("Choose your Ability Score Improvement: +2 to one ability, or +1 to two.");
        }
        Character raised = character;
        for (Ability ability : choices.ability_increases) ++score_of(raised, ability);
        for (Ability ability : choices.ability_increases) {
            if (score_of(raised, ability) > 20) {
                return std::string(ability_name(ability)) + " can't go above 20.";
            }
        }
    } else if (!choices.ability_increases.empty()) {
        return "There's no Ability Score Improvement at level " + std::to_string(plan.new_level) + ".";
    }

    if (plan.fighting_style) {
        for (FightingStyle style : plan.fighting_style_options) {
            const FightingStyleInfo* info = fighting_style_info(style);
            if (info && choices.fighting_style == info->key) return std::nullopt;
        }
        return std::string("Choose a Fighting Style.");
    }
    return std::nullopt;
}

int level_up_hp_increase(const Character& character, const LevelUpPlan& plan, const LevelUpChoices& choices) {
    int old_modifier = ability_modifier(character.constitution);
    int constitution = character.constitution;
    for (Ability ability : choices.ability_increases) {
        if (ability == Ability::Constitution) ++constitution;
    }
    int new_modifier = ability_modifier(constitution);
    int die = choices.hp_method == HitPointMethod::Roll ? choices.hp_roll : plan.average_hp;
    return std::max(1, die + new_modifier) + (new_modifier - old_modifier) * character.level;
}

void apply_level_up(Character& character, const LevelUpPlan& plan, const LevelUpChoices& choices) {
    if (auto error = validate_level_up(character, plan, choices)) throw std::invalid_argument(*error);
    int hp = level_up_hp_increase(character, plan, choices);
    for (Ability ability : choices.ability_increases) ++score_of(character, ability);
    if (plan.fighting_style) character.fighting_style = choices.fighting_style;
    character.level = plan.new_level;
    character.max_hp += hp;
    if (character.subclass == "Draconic Bloodline") {  // Draconic Resilience: +1 HP per sorcerer level
        character.max_hp += 1;
        character.current_hp += 1;
    }
    character.current_hp += hp;
}

}  // namespace dnd
