#include "dnd/checks.hpp"

#include "dnd/class_features.hpp"
#include "dnd/equipment.hpp"
#include "dnd/magic_items.hpp"
#include "dnd/rules.hpp"

namespace dnd {

namespace {

// The skill named `what`, if it is one.
bool find_skill(const std::string& what, Skill& skill) {
    for (int i = 0; i < kSkillCount; ++i) {
        if (what == skill_name(static_cast<Skill>(i))) {
            skill = static_cast<Skill>(i);
            return true;
        }
    }
    return false;
}

std::string format_bonus(int value) {
    return (value >= 0 ? "+" : "") + std::to_string(value);
}

}  // namespace

bool check_ability(const std::string& what, Ability& ability) {
    Skill skill;
    if (find_skill(what, skill)) {
        ability = ability_for_skill(skill);
        return true;
    }
    for (int i = 0; i < kAbilityCount; ++i) {
        if (what == ability_name(static_cast<Ability>(i))) {
            ability = static_cast<Ability>(i);
            return true;
        }
    }
    return false;
}

int check_bonus(const CheckRoller& roller, const std::string& what) {
    const Character scored = with_item_scores(roller.character, roller.inventory);
    int bonus = 0;
    Skill skill;
    Ability ability;
    if (find_skill(what, skill)) {
        bonus = skill_bonus(scored, skill);  // proficiency and Expertise
        if (!has_skill_proficiency(scored, skill)) bonus += jack_of_all_trades(scored, roller.class_name);
    } else if (check_ability(what, ability)) {
        bonus = ability_modifier(ability_score(scored, ability)) + jack_of_all_trades(scored, roller.class_name);
    }
    return bonus - roller.character.revival_penalty;
}

CheckMode check_mode(const CheckRoller& roller, const std::string& what, bool helped, bool inspired) {
    CheckMode mode;
    if (helped) mode.advantage.push_back("Help");
    if (inspired) mode.advantage.push_back("inspiration");
    if (roller.character.exhaustion >= 1) mode.disadvantage.push_back("exhaustion");
    Ability ability;
    if (check_ability(what, ability) && (ability == Ability::Strength || ability == Ability::Dexterity) &&
        wearing_armor_without_proficiency(roller.class_name, roller.inventory, roller.character.subclass)) {
        mode.disadvantage.push_back("armor without proficiency");
    }
    if (!mode.advantage.empty() && mode.disadvantage.empty()) mode.mode = RollMode::Advantage;
    if (mode.advantage.empty() && !mode.disadvantage.empty()) mode.mode = RollMode::Disadvantage;
    return mode;
}

std::string CheckResult::text() const {
    std::string line = who + ": " + what + " d20(" + std::to_string(d20.result) + ") " + format_bonus(bonus);
    if (mode.mode == RollMode::Advantage) line += " with advantage";
    if (mode.mode == RollMode::Disadvantage) line += " with disadvantage";
    std::vector<std::string> why = mode.mode == RollMode::Advantage      ? mode.advantage
                                   : mode.mode == RollMode::Disadvantage ? mode.disadvantage
                                                                         : std::vector<std::string>{};
    if (!why.empty()) {
        line += " (";
        for (size_t i = 0; i < why.size(); ++i) line += (i ? ", " : "") + why[i];
        line += ")";
    }
    return line + " = " + std::to_string(total) + " vs DC " + std::to_string(dc) + " -> " +
           (success ? "success" : "failure");
}

CheckResult ability_check(const Dice& dice, const CheckRoller& roller, const std::string& what, int dc, bool helped,
                          bool inspired) {
    CheckResult result;
    result.who = roller.character.character_name;
    result.what = what;
    result.dc = dc;
    result.mode = check_mode(roller, what, helped, inspired);
    result.d20 = dice.roll_d20(result.mode.mode);
    result.bonus = check_bonus(roller, what);
    result.total = result.d20.result + result.bonus;
    result.success = result.total >= dc;
    return result;
}

GroupCheckResult group_check(const Dice& dice, const std::vector<CheckRoller>& party, const std::string& what, int dc) {
    GroupCheckResult group;
    int passed = 0;
    for (const auto& roller : party) {
        group.rolls.push_back(ability_check(dice, roller, what, dc));
        if (group.rolls.back().success) ++passed;
    }
    group.success = !party.empty() && passed * 2 >= static_cast<int>(party.size());
    return group;
}

int passive_score(const CheckRoller& roller, const std::string& what) {
    int score = 10 + check_bonus(roller, what);
    CheckMode mode = check_mode(roller, what, false, false);
    if (mode.mode == RollMode::Advantage) score += 5;
    if (mode.mode == RollMode::Disadvantage) score -= 5;
    return score;
}

}  // namespace dnd
