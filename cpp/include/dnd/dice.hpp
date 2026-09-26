#pragma once

#include <vector>

#include "dnd/rng.hpp"

namespace dnd {

enum class RollMode { Normal, Advantage, Disadvantage };

// One roll for Normal, two for Advantage/Disadvantage -- `rolls` keeps both
// so the UI can show which die was actually used, not just the result.
struct D20Roll {
    std::vector<int> rolls;
    int result = 0;
};

class Dice {
public:
    explicit Dice(IRandomSource& random_source) : random_source_(random_source) {}

    // Rolls `count` dice with `sides` faces each. Throws std::invalid_argument
    // if sides < 1 or count < 0.
    std::vector<int> roll(int sides, int count) const;

    // Advantage/disadvantage per the 5e rule: roll 2d20, take the
    // higher/lower. Never more than two dice, regardless of how many
    // sources of advantage or disadvantage apply.
    D20Roll roll_d20(RollMode mode) const;

    static int sum(const std::vector<int>& rolls);

private:
    IRandomSource& random_source_;
};

}  // namespace dnd
