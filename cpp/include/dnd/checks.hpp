#pragma once

#include <string>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/dice.hpp"
#include "dnd/item.hpp"
#include "dnd/rules.hpp"

namespace dnd {

// Ability checks outside combat (PHB ch. 7; docs/scenes-and-npcs.md): a
// skill ("Athletics") or a plain ability ("Strength") against a DC.

// What a check needs to know about the one rolling it.
struct CheckRoller {
    Character character;
    std::string class_name;
    std::vector<InventoryEntry> inventory;  // raised scores, armor proficiency
};

// The check's bonus: the ability modifier (with magic items), plus
// proficiency (doubled with Expertise) or Jack of All Trades, minus a raise
// dead penalty. `what` is a skill or ability name; unknown names get +0.
int check_bonus(const CheckRoller& roller, const std::string& what);

// Advantage and disadvantage on the check, and why: Help and inspiration
// give advantage; exhaustion (1+), and wearing armor without proficiency on
// STR or DEX checks, give disadvantage.
struct CheckMode {
    RollMode mode = RollMode::Normal;
    std::vector<std::string> advantage;
    std::vector<std::string> disadvantage;
};
CheckMode check_mode(const CheckRoller& roller, const std::string& what, bool helped, bool inspired);

struct CheckResult {
    std::string who;
    std::string what;
    int dc = 0;
    D20Roll d20;
    int bonus = 0;
    int total = 0;
    bool success = false;
    CheckMode mode;
    // "Aria: Athletics d20(14) +5 with advantage (Help) = 19 vs DC 15 -> success"
    std::string text() const;
};

CheckResult ability_check(const Dice& dice, const CheckRoller& roller, const std::string& what, int dc,
                          bool helped = false, bool inspired = false);

// A group check (PHB ch. 7): everyone rolls; it succeeds if at least half do.
struct GroupCheckResult {
    std::vector<CheckResult> rolls;
    bool success = false;
};
GroupCheckResult group_check(const Dice& dice, const std::vector<CheckRoller>& party, const std::string& what, int dc);

// 10 + the check bonus, +5 with advantage or -5 with disadvantage (PHB ch. 7).
int passive_score(const CheckRoller& roller, const std::string& what);

// "Athletics" -> Strength, "Strength" -> Strength; false for unknown names.
bool check_ability(const std::string& what, Ability& ability);

}  // namespace dnd
