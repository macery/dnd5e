#include "dnd/rules.hpp"

#include <array>
#include <stdexcept>

namespace dnd {

namespace {

constexpr std::array<const char*, kAbilityCount> kAbilityNames = {
    "Strength", "Dexterity", "Constitution", "Intelligence", "Wisdom", "Charisma",
};

constexpr std::array<const char*, kSkillCount> kSkillNames = {
    "Acrobatics",     "Animal Handling", "Arcana",       "Athletics",  "Deception",
    "History",        "Insight",         "Intimidation", "Investigation", "Medicine",
    "Nature",         "Perception",      "Performance",  "Persuasion", "Religion",
    "Sleight of Hand", "Stealth",        "Survival",
};

constexpr std::array<Ability, kSkillCount> kSkillAbility = {
    Ability::Dexterity,     // Acrobatics
    Ability::Wisdom,        // Animal Handling
    Ability::Intelligence,  // Arcana
    Ability::Strength,      // Athletics
    Ability::Charisma,      // Deception
    Ability::Intelligence,  // History
    Ability::Wisdom,        // Insight
    Ability::Charisma,      // Intimidation
    Ability::Intelligence,  // Investigation
    Ability::Wisdom,        // Medicine
    Ability::Intelligence,  // Nature
    Ability::Wisdom,        // Perception
    Ability::Charisma,      // Performance
    Ability::Charisma,      // Persuasion
    Ability::Intelligence,  // Religion
    Ability::Dexterity,     // Sleight of Hand
    Ability::Dexterity,     // Stealth
    Ability::Wisdom,        // Survival
};

constexpr std::array<const char*, kAbilityCount> kAbilityDescriptions = {
    "Physical power. Governs melee attacks and damage with heavy weapons, and tasks like breaking or lifting "
    "things.",
    "Agility and reflexes. Governs ranged and finesse-weapon attacks, your Armor Class, and avoiding danger.",
    "Health and stamina. Determines your hit points and resisting poison, disease, and exhaustion.",
    "Reasoning and memory. Governs Arcana, History, Investigation, Nature, and Religion checks.",
    "Awareness and intuition. Governs Perception, Insight, Animal Handling, Medicine, and Survival checks.",
    "Force of personality. Governs Deception, Intimidation, Performance, and Persuasion checks.",
};

constexpr std::array<const char*, kSkillCount> kSkillDescriptions = {
    "Staying on your feet in tricky situations -- balancing, tumbling, escaping a grapple.",
    "Calming, controlling, or reading the intentions of an animal.",
    "Recalling knowledge about spells, magic items, and magical traditions.",
    "Climbing, jumping, swimming, or physically grappling with something.",
    "Convincingly lying or misleading someone.",
    "Recalling knowledge about past events, people, and civilizations.",
    "Reading a person's true intentions, emotions, or honesty.",
    "Influencing someone through threats or a show of force.",
    "Finding clues and piecing together how something works from evidence.",
    "Diagnosing illness, stabilizing the dying, or treating injuries.",
    "Recalling knowledge about terrain, plants, animals, and weather.",
    "Noticing things -- spotting, hearing, or otherwise sensing your surroundings.",
    "Entertaining an audience with music, acting, storytelling, or the like.",
    "Convincing someone through genuine tact, social grace, or good arguments.",
    "Recalling knowledge about deities, rites, and religious practices.",
    "Manual trickery -- picking a pocket, planting an item, or a bit of stage magic.",
    "Moving quietly and staying out of sight.",
    "Tracking, foraging, navigating the wild, or predicting weather.",
};

int floor_div(int numerator, int denominator) {
    int quotient = numerator / denominator;
    int remainder = numerator % denominator;
    if (remainder != 0 && ((remainder < 0) != (denominator < 0))) {
        --quotient;
    }
    return quotient;
}

}  // namespace

const char* ability_name(Ability ability) {
    return kAbilityNames.at(static_cast<size_t>(ability));
}

const char* skill_name(Skill skill) {
    return kSkillNames.at(static_cast<size_t>(skill));
}

const char* ability_description(Ability ability) {
    return kAbilityDescriptions.at(static_cast<size_t>(ability));
}

const char* skill_description(Skill skill) {
    return kSkillDescriptions.at(static_cast<size_t>(skill));
}

Ability ability_for_skill(Skill skill) {
    return kSkillAbility.at(static_cast<size_t>(skill));
}

int ability_modifier(int score) {
    return floor_div(score - 10, 2);
}

int proficiency_bonus(int level) {
    if (level < 1) throw std::invalid_argument("level must be >= 1");
    return 2 + floor_div(level - 1, 4);
}

int ability_score(const Character& character, Ability ability) {
    switch (ability) {
        case Ability::Strength: return character.strength;
        case Ability::Dexterity: return character.dexterity;
        case Ability::Constitution: return character.constitution;
        case Ability::Intelligence: return character.intelligence;
        case Ability::Wisdom: return character.wisdom;
        case Ability::Charisma: return character.charisma;
    }
    throw std::invalid_argument("unknown ability");
}

bool has_skill_proficiency(const Character& character, Skill skill) {
    return (character.skill_proficiencies >> static_cast<int>(skill)) & 1u;
}

bool has_save_proficiency(const Character& character, Ability ability) {
    return (character.save_proficiencies >> static_cast<int>(ability)) & 1u;
}

int skill_bonus(const Character& character, Skill skill) {
    int modifier = ability_modifier(ability_score(character, ability_for_skill(skill)));
    if (has_skill_proficiency(character, skill)) {
        modifier += proficiency_bonus(character.level);
    }
    return modifier;
}

int saving_throw_bonus(const Character& character, Ability ability) {
    int modifier = ability_modifier(ability_score(character, ability));
    if (has_save_proficiency(character, ability)) {
        modifier += proficiency_bonus(character.level);
    }
    return modifier;
}

int passive_perception(const Character& character) {
    return 10 + skill_bonus(character, Skill::Perception);
}

int initiative(const Character& character) {
    return ability_modifier(character.dexterity);
}

int weapon_attack_ability_modifier(const Character& character, const Weapon& weapon) {
    int str_modifier = ability_modifier(character.strength);
    int dex_modifier = ability_modifier(character.dexterity);

    if (weapon.is_ranged) return dex_modifier;
    if (weapon.is_finesse) return dex_modifier >= str_modifier ? dex_modifier : str_modifier;
    return str_modifier;
}

int weapon_attack_bonus(const Character& character, const Weapon& weapon) {
    return proficiency_bonus(character.level) + weapon_attack_ability_modifier(character, weapon);
}

}  // namespace dnd
