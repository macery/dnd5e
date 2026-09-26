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

}  // namespace dnd
