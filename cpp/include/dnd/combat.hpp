#pragma once

#include <vector>

#include "dnd/dice.hpp"

namespace dnd {

struct AttackResult {
    int attack_roll = 0;
    bool hit = false;
    bool critical = false;  // natural 20
    std::vector<int> damage_dice;
    int damage_total = 0;  // 0 on a miss
};

// A natural 1 always misses; a natural 20 always hits and doubles the
// damage dice count (not the flat bonus), per the 5e critical hit rule.
// Otherwise a hit if attack_roll + attack_bonus >= target_ac. Fully generic
// (no Character/Monster dependency) so it works for either side of a fight
// and is directly testable.
AttackResult resolve_attack(const Dice& dice, int attack_bonus, int target_ac, int damage_dice_count,
                             int damage_dice_sides, int damage_bonus);

}  // namespace dnd
