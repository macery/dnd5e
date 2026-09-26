#pragma once

#include <vector>

#include "dnd/rng.hpp"

namespace dnd {

class Dice {
public:
    explicit Dice(IRandomSource& random_source) : random_source_(random_source) {}

    // Rolls `count` dice with `sides` faces each. Throws std::invalid_argument
    // if sides < 1 or count < 0.
    std::vector<int> roll(int sides, int count) const;

    static int sum(const std::vector<int>& rolls);

private:
    IRandomSource& random_source_;
};

}  // namespace dnd
