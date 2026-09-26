#include "dnd/dice.hpp"

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

int Dice::sum(const std::vector<int>& rolls) {
    return std::accumulate(rolls.begin(), rolls.end(), 0);
}

}  // namespace dnd
