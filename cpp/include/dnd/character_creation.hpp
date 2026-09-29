#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/dice.hpp"
#include "dnd/rules.hpp"

namespace dnd {

// Fixed 5e (2014 PHB) rules data for building a level-1 character. Names
// match the seeded Classes/Races tables, which only store id + name; the
// rules themselves live here, like the progression tables.

// Which spell slot table a class uses (see dnd/spellcasting.hpp).
enum class CasterType { None, Full, Half, Pact };

// How a caster decides which spells it can cast.
enum class SpellPreparation {
    None,
    Known,      // learns a fixed number (Bard, Ranger, Sorcerer, Warlock)
    Prepared,   // prepares from the whole class list (Cleric, Druid, Paladin)
    Spellbook,  // prepares from spells copied into a spellbook (Wizard)
};

struct SpellcastingInfo {
    CasterType caster_type = CasterType::None;
    Ability ability = Ability::Intelligence;
    SpellPreparation preparation = SpellPreparation::None;
    int base_cantrips = 0;  // cantrips known at level 1 (+1 at 4, +1 at 10)
};

struct ClassInfo {
    std::string name;
    int hit_die = 8;
    std::array<Ability, 2> saving_throws{};
    int skill_choice_count = 2;
    std::vector<Skill> skill_options;  // pick skill_choice_count of these
    std::string suggested_weapon;      // a seeded Weapon.name
    std::string summary;               // one line for the wizard
    SpellcastingInfo spellcasting;     // caster_type None for non-casters
};

struct RaceInfo {
    std::string name;
    std::array<int, kAbilityCount> ability_bonuses{};  // indexed by Ability
    // Extra +1s the player assigns to distinct abilities that have no fixed
    // bonus (Half-Elf: two).
    int flexible_bonus_count = 0;
    int speed = 30;
    std::vector<Skill> skill_proficiencies;  // granted outright
    int free_skill_choices = 0;              // any skill (Half-Elf: two)
    std::vector<std::string> traits;         // shown, not yet simulated
};

struct BackgroundInfo {
    std::string name;
    std::array<Skill, 2> skill_proficiencies{};
    std::string summary;
};

const std::vector<ClassInfo>& class_infos();
const std::vector<RaceInfo>& race_infos();
const std::vector<BackgroundInfo>& background_infos();

// nullptr when the name isn't known.
const ClassInfo* find_class_info(const std::string& name);
const RaceInfo* find_race_info(const std::string& name);
const BackgroundInfo* find_background_info(const std::string& name);

// --- Ability scores -------------------------------------------------------

enum class AbilityScoreMethod { StandardArray, Rolled, PointBuy };

constexpr std::array<int, kAbilityCount> kStandardArray = {15, 14, 13, 12, 10, 8};
constexpr int kPointBuyBudget = 27;
constexpr int kPointBuyMin = 8;
constexpr int kPointBuyMax = 15;

// 4d6, drop the lowest die.
int roll_ability_score(const Dice& dice);
std::array<int, kAbilityCount> roll_ability_scores(const Dice& dice);

// Point-buy cost of one score (8 -> 0 ... 13 -> 5, 14 -> 7, 15 -> 9).
// Throws std::invalid_argument outside [kPointBuyMin, kPointBuyMax].
int point_buy_cost(int score);
int point_buy_total(const std::array<int, kAbilityCount>& scores);

// --- Skills ---------------------------------------------------------------

// What the player still gets to pick once class, race, and background are
// known. `granted` (race + background) are fixed. A background skill that
// duplicates a race skill becomes an extra free choice, per the PHB rule for
// overlapping proficiencies.
struct SkillPlan {
    std::vector<Skill> granted;
    int class_choice_count = 0;
    std::vector<Skill> class_options;  // the class list minus `granted`
    int free_choice_count = 0;         // any skill not already granted
};

SkillPlan skill_plan(const ClassInfo& class_info, const RaceInfo& race, const BackgroundInfo& background);

// --- Assembling the character --------------------------------------------

struct CreationChoices {
    std::string name;
    int player_id = 0;

    std::string class_name;
    int class_id = 0;  // Classes row for class_name
    std::string race_name;
    int race_id = 0;
    int alignment_id = 0;
    std::string background_name;

    AbilityScoreMethod method = AbilityScoreMethod::StandardArray;
    // For StandardArray/Rolled: the values available to assign
    // (kStandardArray, or the six rolls). Ignored for PointBuy.
    std::array<int, kAbilityCount> score_pool = kStandardArray;
    // Scores before racial bonuses, indexed by Ability.
    std::array<int, kAbilityCount> base_scores = kStandardArray;
    std::vector<Ability> flexible_bonus_abilities;

    std::vector<Skill> chosen_skills;  // class picks + free picks together
    int weapon_id = 0;
};

// Each returns an error message for the player, or nullopt if that part of
// the choices is valid. Unknown class/race/background names are errors.
std::optional<std::string> validate_ability_scores(const CreationChoices& choices);
std::optional<std::string> validate_flexible_bonuses(const CreationChoices& choices);
std::optional<std::string> validate_skills(const CreationChoices& choices);
std::optional<std::string> validate_details(const CreationChoices& choices);
// First error from all of the above.
std::optional<std::string> validate_choices(const CreationChoices& choices);

// Base score + racial and flexible bonuses, indexed by Ability.
std::array<int, kAbilityCount> final_ability_scores(const CreationChoices& choices);

// 10 + DEX, or the class's Unarmored Defense (Barbarian + CON, Monk + WIS),
// whichever is higher. There's no armor system yet.
int starting_armor_class(const ClassInfo& class_info, const std::array<int, kAbilityCount>& scores);

// A full level-1 character: final scores, class saving throws, all skill
// proficiencies, max HP = hit die + CON modifier (minimum 1) at full health,
// starting AC, and the background. Throws std::invalid_argument with the
// validate_choices message if the choices aren't valid.
Character build_character(const CreationChoices& choices);

// The Explorer's Pack, as seeded Item names + quantities.
struct StartingItem {
    std::string item_name;
    int quantity = 1;
};
const std::vector<StartingItem>& explorers_pack();

}  // namespace dnd
