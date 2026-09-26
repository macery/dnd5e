#include "dnd/dice.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace dnd {

std::vector<int> Dice::roll(int sides, int count) const {
    if (sides < 1) throw std::invalid_argument("sides must be >= 1");
    if (count < 0) throw std::invalid_argument("count must be >= 0");

    std::vector<int> results;
    results.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        results.push_back(random_source_.roll_die(sides));
    }
    return results;
}

D20Roll Dice::roll_d20(RollMode mode) const {
    if (mode == RollMode::Normal) {
        auto rolls = roll(20, 1);
        return D20Roll{rolls, rolls.front()};
    }

    auto rolls = roll(20, 2);
    int result = (mode == RollMode::Advantage) ? std::max(rolls[0], rolls[1]) : std::min(rolls[0], rolls[1]);
    return D20Roll{rolls, result};
}

int Dice::sum(const std::vector<int>& rolls) {
    return std::accumulate(rolls.begin(), rolls.end(), 0);
}

}  // namespace dnd
