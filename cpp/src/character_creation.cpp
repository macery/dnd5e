#include "dnd/character_creation.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace dnd {

namespace {

using A = Ability;
using S = Skill;

std::vector<Skill> all_skills() {
    std::vector<Skill> skills;
    for (int i = 0; i < kSkillCount; ++i) skills.push_back(static_cast<Skill>(i));
    return skills;
}

bool contains(const std::vector<Skill>& skills, Skill skill) {
    return std::find(skills.begin(), skills.end(), skill) != skills.end();
}

size_t index_of(Ability ability) {
    return static_cast<size_t>(ability);
}

}  // namespace

const std::vector<ClassInfo>& class_infos() {
    static const std::vector<ClassInfo> classes = {
        {"Barbarian", 12, {A::Strength, A::Constitution}, 2,
         {S::AnimalHandling, S::Athletics, S::Intimidation, S::Nature, S::Perception, S::Survival}, "Greataxe",
         "A fierce warrior who fights with primal rage. Unarmored Defense: AC 10 + DEX + CON."},
        {"Bard", 8, {A::Dexterity, A::Charisma}, 3, all_skills(), "Rapier",
         "An inspiring performer whose words and music carry magic. Picks any three skills.", {CasterType::Full, A::Charisma, SpellPreparation::Known, 2}},
        {"Cleric", 8, {A::Wisdom, A::Charisma}, 2, {S::History, S::Insight, S::Medicine, S::Persuasion, S::Religion},
         "Mace", "A priestly champion who wields divine magic in service of a god.", {CasterType::Full, A::Wisdom, SpellPreparation::Prepared, 3}},
        {"Druid", 8, {A::Intelligence, A::Wisdom}, 2,
         {S::Arcana, S::AnimalHandling, S::Insight, S::Medicine, S::Nature, S::Perception, S::Religion, S::Survival},
         "Quarterstaff", "A priest of the old faith, wielding the powers of nature.", {CasterType::Full, A::Wisdom, SpellPreparation::Prepared, 2}},
        {"Fighter", 10, {A::Strength, A::Constitution}, 2,
         {S::Acrobatics, S::AnimalHandling, S::Athletics, S::History, S::Insight, S::Intimidation, S::Perception,
          S::Survival},
         "Longsword", "A master of martial combat, skilled with every weapon and armor."},
        {"Monk", 8, {A::Strength, A::Dexterity}, 2,
         {S::Acrobatics, S::Athletics, S::History, S::Insight, S::Religion, S::Stealth}, "Shortsword",
         "A disciplined martial artist. Unarmored Defense: AC 10 + DEX + WIS."},
        {"Paladin", 10, {A::Wisdom, A::Charisma}, 2,
         {S::Athletics, S::Insight, S::Intimidation, S::Medicine, S::Persuasion, S::Religion}, "Longsword",
         "A holy warrior bound to a sacred oath.", {CasterType::Half, A::Charisma, SpellPreparation::Prepared, 0}},
        {"Ranger", 10, {A::Strength, A::Dexterity}, 3,
         {S::AnimalHandling, S::Athletics, S::Insight, S::Investigation, S::Nature, S::Perception, S::Stealth,
          S::Survival},
         "Longbow", "A hunter and tracker at home in the wilds.", {CasterType::Half, A::Wisdom, SpellPreparation::Known, 0}},
        {"Rogue", 8, {A::Dexterity, A::Intelligence}, 4,
         {S::Acrobatics, S::Athletics, S::Deception, S::Insight, S::Intimidation, S::Investigation, S::Perception,
          S::Performance, S::Persuasion, S::SleightOfHand, S::Stealth},
         "Rapier", "A scoundrel who relies on stealth, skill, and precision."},
        {"Sorcerer", 6, {A::Constitution, A::Charisma}, 2,
         {S::Arcana, S::Deception, S::Insight, S::Intimidation, S::Persuasion, S::Religion}, "Light Crossbow",
         "A spellcaster whose magic comes from an inborn gift or bloodline.", {CasterType::Full, A::Charisma, SpellPreparation::Known, 4}},
        {"Warlock", 8, {A::Wisdom, A::Charisma}, 2,
         {S::Arcana, S::Deception, S::History, S::Intimidation, S::Investigation, S::Nature, S::Religion},
         "Light Crossbow", "A wielder of magic granted by a pact with an otherworldly patron.", {CasterType::Pact, A::Charisma, SpellPreparation::Known, 2}},
        {"Wizard", 6, {A::Intelligence, A::Wisdom}, 2,
         {S::Arcana, S::History, S::Insight, S::Investigation, S::Medicine, S::Religion}, "Quarterstaff",
         "A scholarly magic-user who masters spells through study.", {CasterType::Full, A::Intelligence, SpellPreparation::Spellbook, 3}},
    };
    return classes;
}

const std::vector<RaceInfo>& race_infos() {
    // Ability bonus order: STR, DEX, CON, INT, WIS, CHA. Base races only --
    // subraces (Hill Dwarf, High Elf, ...) aren't modeled yet.
    static const std::vector<RaceInfo> races = {
        {"Dragonborn", {2, 0, 0, 0, 0, 1}, 0, 30, {}, 0,
         {"Draconic Ancestry", "Breath Weapon", "Damage Resistance"}},
        {"Dwarf", {0, 0, 2, 0, 0, 0}, 0, 25, {}, 0, {"Darkvision", "Dwarven Resilience", "Stonecunning"}},
        {"Elf", {0, 2, 0, 0, 0, 0}, 0, 30, {S::Perception}, 0,
         {"Darkvision", "Keen Senses (Perception)", "Fey Ancestry", "Trance"}},
        {"Gnome", {0, 0, 0, 2, 0, 0}, 0, 25, {}, 0, {"Darkvision", "Gnome Cunning"}},
        {"Half-Elf", {0, 0, 0, 0, 0, 2}, 2, 30, {}, 2,
         {"Darkvision", "Fey Ancestry", "Skill Versatility (two skills of your choice)",
          "+1 to two other abilities of your choice"}},
        {"Half-Orc", {2, 0, 1, 0, 0, 0}, 0, 30, {S::Intimidation}, 0,
         {"Darkvision", "Menacing (Intimidation)", "Relentless Endurance", "Savage Attacks"}},
        {"Halfling", {0, 2, 0, 0, 0, 0}, 0, 25, {}, 0, {"Lucky", "Brave", "Halfling Nimbleness"}},
        {"Human", {1, 1, 1, 1, 1, 1}, 0, 30, {}, 0, {"+1 to every ability score"}},
        {"Tiefling", {0, 0, 0, 1, 0, 2}, 0, 30, {}, 0, {"Darkvision", "Hellish Resistance", "Infernal Legacy"}},
    };
    return races;
}

const std::vector<BackgroundInfo>& background_infos() {
    static const std::vector<BackgroundInfo> backgrounds = {
        {"Acolyte", {S::Insight, S::Religion}, "You served in a temple, performing sacred rites."},
        {"Charlatan", {S::Deception, S::SleightOfHand}, "You've always had a way with people -- and their purses."},
        {"Criminal", {S::Deception, S::Stealth}, "You have a history of breaking the law."},
        {"Entertainer", {S::Acrobatics, S::Performance}, "You thrive in front of an audience."},
        {"Folk Hero", {S::AnimalHandling, S::Survival}, "You come from humble roots, but you're destined for more."},
        {"Guild Artisan", {S::Insight, S::Persuasion}, "You're a member of an artisan's guild, skilled in a trade."},
        {"Hermit", {S::Medicine, S::Religion}, "You lived in seclusion for a formative part of your life."},
        {"Noble", {S::History, S::Persuasion}, "You were born into wealth, power, and privilege."},
        {"Outlander", {S::Athletics, S::Survival}, "You grew up in the wilds, far from civilization."},
        {"Sage", {S::Arcana, S::History}, "You spent years learning the lore of the multiverse."},
        {"Sailor", {S::Athletics, S::Perception}, "You sailed on a seagoing vessel for years."},
        {"Soldier", {S::Athletics, S::Intimidation}, "War has been your life for as long as you care to remember."},
        {"Urchin", {S::SleightOfHand, S::Stealth}, "You grew up on the streets, alone and poor."},
    };
    return backgrounds;
}

const ClassInfo* find_class_info(const std::string& name) {
    for (const auto& info : class_infos()) {
        if (info.name == name) return &info;
    }
    return nullptr;
}

const RaceInfo* find_race_info(const std::string& name) {
    for (const auto& info : race_infos()) {
        if (info.name == name) return &info;
    }
    return nullptr;
}

const BackgroundInfo* find_background_info(const std::string& name) {
    for (const auto& info : background_infos()) {
        if (info.name == name) return &info;
    }
    return nullptr;
}

int roll_ability_score(const Dice& dice) {
    std::vector<int> rolls = dice.roll(6, 4);
    std::sort(rolls.begin(), rolls.end());
    return rolls[1] + rolls[2] + rolls[3];
}

std::array<int, kAbilityCount> roll_ability_scores(const Dice& dice) {
    std::array<int, kAbilityCount> scores{};
    for (auto& score : scores) score = roll_ability_score(dice);
    return scores;
}

int point_buy_cost(int score) {
    if (score < kPointBuyMin || score > kPointBuyMax) {
        throw std::invalid_argument("point-buy scores must be between 8 and 15");
    }
    if (score <= 13) return score - 8;
    return score == 14 ? 7 : 9;
}

int point_buy_total(const std::array<int, kAbilityCount>& scores) {
    int total = 0;
    for (int score : scores) total += point_buy_cost(score);
    return total;
}

SkillPlan skill_plan(const ClassInfo& class_info, const RaceInfo& race, const BackgroundInfo& background) {
    SkillPlan plan;
    plan.granted = race.skill_proficiencies;
    plan.free_choice_count = race.free_skill_choices;
    for (Skill skill : background.skill_proficiencies) {
        if (contains(plan.granted, skill)) {
            ++plan.free_choice_count;  // overlap: pick any other skill instead
        } else {
            plan.granted.push_back(skill);
        }
    }

    for (Skill skill : class_info.skill_options) {
        if (!contains(plan.granted, skill)) plan.class_options.push_back(skill);
    }
    plan.class_choice_count = std::min(class_info.skill_choice_count, static_cast<int>(plan.class_options.size()));
    return plan;
}

std::optional<std::string> validate_ability_scores(const CreationChoices& choices) {
    if (choices.method == AbilityScoreMethod::PointBuy) {
        for (int score : choices.base_scores) {
            if (score < kPointBuyMin || score > kPointBuyMax) {
                return "With point buy, every score must be between 8 and 15.";
            }
        }
        int spent = point_buy_total(choices.base_scores);
        if (spent > kPointBuyBudget) {
            return "That costs " + std::to_string(spent) + " points; the budget is " +
                   std::to_string(kPointBuyBudget) + ".";
        }
        return std::nullopt;
    }

    auto assigned = choices.base_scores;
    auto pool = choices.score_pool;
    std::sort(assigned.begin(), assigned.end());
    std::sort(pool.begin(), pool.end());
    if (assigned != pool) return "Assign each value exactly once -- one per ability.";
    for (int score : assigned) {
        if (score < 3 || score > 18) return "Ability scores must be between 3 and 18 before racial bonuses.";
    }
    return std::nullopt;
}

std::optional<std::string> validate_flexible_bonuses(const CreationChoices& choices) {
    const RaceInfo* race = find_race_info(choices.race_name);
    if (!race) return "Choose a race.";

    const auto& picks = choices.flexible_bonus_abilities;
    if (static_cast<int>(picks.size()) != race->flexible_bonus_count) {
        return "Choose " + std::to_string(race->flexible_bonus_count) + " abilities for your +1 bonuses.";
    }
    for (size_t i = 0; i < picks.size(); ++i) {
        if (race->ability_bonuses[index_of(picks[i])] != 0) {
            return std::string("Your +1 bonuses can't go to ") + ability_name(picks[i]) +
                   ", which already has a racial bonus.";
        }
        for (size_t j = i + 1; j < picks.size(); ++j) {
            if (picks[i] == picks[j]) return "Your +1 bonuses must go to different abilities.";
        }
    }
    return std::nullopt;
}

std::optional<std::string> validate_skills(const CreationChoices& choices) {
    const ClassInfo* class_info = find_class_info(choices.class_name);
    const RaceInfo* race = find_race_info(choices.race_name);
    const BackgroundInfo* background = find_background_info(choices.background_name);
    if (!class_info) return "Choose a class.";
    if (!race) return "Choose a race.";
    if (!background) return "Choose a background.";

    SkillPlan plan = skill_plan(*class_info, *race, *background);
    int expected = plan.class_choice_count + plan.free_choice_count;
    const auto& chosen = choices.chosen_skills;

    int outside_class_list = 0;
    for (size_t i = 0; i < chosen.size(); ++i) {
        if (contains(plan.granted, chosen[i])) {
            return std::string("You already have ") + skill_name(chosen[i]) + " from your race or background.";
        }
        for (size_t j = i + 1; j < chosen.size(); ++j) {
            if (chosen[i] == chosen[j]) return std::string(skill_name(chosen[i])) + " is chosen twice.";
        }
        if (!contains(plan.class_options, chosen[i])) ++outside_class_list;
    }

    if (static_cast<int>(chosen.size()) != expected) {
        return "Choose exactly " + std::to_string(expected) + " skills (you have " + std::to_string(chosen.size()) +
               ").";
    }
    if (outside_class_list > plan.free_choice_count) {
        return "Choose at least " + std::to_string(plan.class_choice_count) + " skills from your " +
               class_info->name + " list.";
    }
    return std::nullopt;
}

std::optional<std::string> validate_details(const CreationChoices& choices) {
    bool blank = std::all_of(choices.name.begin(), choices.name.end(), [](unsigned char c) { return std::isspace(c); });
    if (blank) return "Give your character a name.";
    return std::nullopt;
}

std::optional<std::string> validate_choices(const CreationChoices& choices) {
    if (!find_class_info(choices.class_name)) return "Choose a class.";
    if (!find_race_info(choices.race_name)) return "Choose a race.";
    if (!find_background_info(choices.background_name)) return "Choose a background.";
    if (auto error = validate_ability_scores(choices)) return error;
    if (auto error = validate_flexible_bonuses(choices)) return error;
    if (auto error = validate_skills(choices)) return error;
    return validate_details(choices);
}

std::array<int, kAbilityCount> final_ability_scores(const CreationChoices& choices) {
    auto scores = choices.base_scores;
    if (const RaceInfo* race = find_race_info(choices.race_name)) {
        for (size_t i = 0; i < scores.size(); ++i) scores[i] += race->ability_bonuses[i];
    }
    for (Ability ability : choices.flexible_bonus_abilities) scores[index_of(ability)] += 1;
    return scores;
}

int starting_armor_class(const ClassInfo& class_info, const std::array<int, kAbilityCount>& scores) {
    auto modifier = [&scores](Ability ability) { return ability_modifier(scores[index_of(ability)]); };
    int armor_class = 10 + modifier(A::Dexterity);
    if (class_info.name == "Barbarian") armor_class += modifier(A::Constitution);
    if (class_info.name == "Monk") armor_class += modifier(A::Wisdom);
    return std::max(10 + modifier(A::Dexterity), armor_class);
}

Character build_character(const CreationChoices& choices) {
    if (auto error = validate_choices(choices)) throw std::invalid_argument(*error);

    const ClassInfo& class_info = *find_class_info(choices.class_name);
    const RaceInfo& race = *find_race_info(choices.race_name);
    const BackgroundInfo& background = *find_background_info(choices.background_name);
    auto scores = final_ability_scores(choices);

    Character character;
    character.character_name = choices.name;
    character.player_id = choices.player_id;
    character.class_id = choices.class_id;
    character.race = choices.race_id;
    character.alignment = choices.alignment_id;
    character.background = background.name;
    character.weapon_id = choices.weapon_id;
    character.level = 1;

    character.strength = scores[index_of(A::Strength)];
    character.dexterity = scores[index_of(A::Dexterity)];
    character.constitution = scores[index_of(A::Constitution)];
    character.intelligence = scores[index_of(A::Intelligence)];
    character.wisdom = scores[index_of(A::Wisdom)];
    character.charisma = scores[index_of(A::Charisma)];

    for (Ability ability : class_info.saving_throws) character.save_proficiencies |= 1u << index_of(ability);
    for (Skill skill : skill_plan(class_info, race, background).granted) {
        character.skill_proficiencies |= 1u << static_cast<int>(skill);
    }
    for (Skill skill : choices.chosen_skills) character.skill_proficiencies |= 1u << static_cast<int>(skill);

    character.max_hp = std::max(1, class_info.hit_die + ability_modifier(character.constitution));
    character.current_hp = character.max_hp;
    character.armor_class = starting_armor_class(class_info, scores);
    return character;
}

const std::vector<StartingItem>& explorers_pack() {
    static const std::vector<StartingItem> pack = {
        {"Bedroll", 1}, {"Rations (1 day)", 10}, {"Waterskin", 1}, {"Torch", 10}, {"Hempen Rope (50 feet)", 1},
    };
    return pack;
}

}  // namespace dnd
