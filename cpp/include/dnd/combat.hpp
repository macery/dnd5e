#pragma once

#include <vector>

#include "dnd/dice.hpp"

namespace dnd {

struct AttackResult {
    int attack_roll = 0;
    bool hit = false;
    bool critical = false;  // natural 20, or any hit with critical_on_hit
    std::vector<int> damage_dice;
    int damage_total = 0;  // 0 on a miss
    int rerolled = 0;      // damage dice rerolled (Great Weapon Fighting)
};

// A natural 1 always misses; a natural 20 always hits and doubles the
// damage dice count (not the flat bonus), per the 5e critical hit rule.
// Otherwise a hit if attack_roll + attack_bonus >= target_ac. Fully generic
// (no Character/Monster dependency) so it works for either side of a fight
// and is directly testable. `mode` lets a caller apply advantage/disadvantage
// (e.g. attacking a Dodging target) without changing anything else.
// `critical_on_hit` makes any hit critical (a melee hit on a paralyzed or
// unconscious target, PHB Appendix A). `reroll_low_damage` rerolls each
// damage die that comes up 1 or 2, once, keeping the new roll (Great Weapon
// Fighting).
// `critical_threshold` is the lowest natural roll that crits (19 for a Champion).
AttackResult resolve_attack(const Dice& dice, int attack_bonus, int target_ac, int damage_dice_count,
                             int damage_dice_sides, int damage_bonus, RollMode mode = RollMode::Normal,
                             bool critical_on_hit = false, bool reroll_low_damage = false, int critical_threshold = 20,
                             int reroll_lowest = 0);

// Rerolls up to `count` of the lowest dice that came up below the die's
// average, keeping the new rolls (Empowered Spell).
void reroll_lowest_dice(const Dice& dice, std::vector<int>& rolled, int sides, int count);

}  // namespace dnd
