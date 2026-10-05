#include "dnd/combat.hpp"

#include <algorithm>

namespace dnd {

AttackResult resolve_attack(const Dice& dice, int attack_bonus, int target_ac, int damage_dice_count,
                             int damage_dice_sides, int damage_bonus, RollMode mode, bool critical_on_hit,
                             bool reroll_low_damage, int critical_threshold, int reroll_lowest) {
    AttackResult result;
    result.attack_roll = dice.roll_d20(mode).result;

    if (result.attack_roll == 1) {
        result.hit = false;
    } else if (result.attack_roll >= critical_threshold) {
        result.hit = true;
        result.critical = true;
    } else {
        result.hit = (result.attack_roll + attack_bonus) >= target_ac;
        result.critical = result.hit && critical_on_hit;
    }

    if (result.hit) {
        int dice_count = result.critical ? damage_dice_count * 2 : damage_dice_count;
        result.damage_dice = dice.roll(damage_dice_sides, dice_count);
        if (reroll_low_damage) {
            for (int& die : result.damage_dice) {
                if (die <= 2) {
                    die = dice.roll(damage_dice_sides, 1).front();
                    ++result.rerolled;
                }
            }
        }
        reroll_lowest_dice(dice, result.damage_dice, damage_dice_sides, reroll_lowest);
        result.damage_total = std::max(0, Dice::sum(result.damage_dice) + damage_bonus);
    }

    return result;
}

void reroll_lowest_dice(const Dice& dice, std::vector<int>& rolled, int sides, int count) {
    for (int n = 0; n < count; ++n) {
        auto lowest = std::min_element(rolled.begin(), rolled.end());
        if (lowest == rolled.end() || *lowest * 2 > sides) return;  // nothing below average left
        *lowest = dice.roll(sides, 1).front();
    }
}

}  // namespace dnd
