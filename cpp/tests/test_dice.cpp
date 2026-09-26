#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <stdexcept>
#include <vector>

#include "dnd/dice.hpp"

namespace {

class FixedRandomSource : public dnd::IRandomSource {
public:
    explicit FixedRandomSource(int value) : value_(value) {}
    int roll_die(int /*sides*/) override { return value_; }

private:
    int value_;
};

// Returns values from a fixed sequence, cycling -- unlike FixedRandomSource,
// this can produce two different results for a two-die roll, which is what
// distinguishing advantage from disadvantage actually requires.
class SequenceRandomSource : public dnd::IRandomSource {
public:
    explicit SequenceRandomSource(std::vector<int> values) : values_(std::move(values)) {}
    int roll_die(int /*sides*/) override {
        int value = values_[index_ % values_.size()];
        ++index_;
        return value;
    }

private:
    std::vector<int> values_;
    size_t index_ = 0;
};

}  // namespace

TEST_CASE("Dice::roll returns count results within [1, sides] for every die size", "[dice]") {
    auto count = GENERATE(1, 2, 3, 4, 5, 6);
    dnd::SystemRandomSource rng;
    dnd::Dice dice(rng);

    for (int sides : {4, 6, 8, 12, 20, 100}) {
        auto rolls = dice.roll(sides, count);
        REQUIRE(rolls.size() == static_cast<size_t>(count));
        for (int roll : rolls) {
            REQUIRE(roll >= 1);
            REQUIRE(roll <= sides);
        }
        REQUIRE(dnd::Dice::sum(rolls) >= count);
        REQUIRE(dnd::Dice::sum(rolls) <= count * sides);
    }
}

TEST_CASE("Dice::roll uses the injected random source", "[dice]") {
    FixedRandomSource fixed(3);
    dnd::Dice dice(fixed);

    auto rolls = dice.roll(6, 4);
    REQUIRE(rolls == std::vector<int>{3, 3, 3, 3});
    REQUIRE(dnd::Dice::sum(rolls) == 12);
}

TEST_CASE("Dice::roll rejects invalid arguments", "[dice]") {
    dnd::SystemRandomSource rng;
    dnd::Dice dice(rng);

    REQUIRE_THROWS_AS(dice.roll(0, 1), std::invalid_argument);
    REQUIRE_THROWS_AS(dice.roll(6, -1), std::invalid_argument);
}

TEST_CASE("roll_d20 with Normal mode rolls exactly one die", "[dice]") {
    FixedRandomSource fixed(14);
    dnd::Dice dice(fixed);

    auto d20 = dice.roll_d20(dnd::RollMode::Normal);
    REQUIRE(d20.rolls == std::vector<int>{14});
    REQUIRE(d20.result == 14);
}

TEST_CASE("roll_d20 with Advantage takes the higher of two rolls", "[dice]") {
    SequenceRandomSource sequence({9, 17});
    dnd::Dice dice(sequence);

    auto d20 = dice.roll_d20(dnd::RollMode::Advantage);
    REQUIRE(d20.rolls == std::vector<int>{9, 17});
    REQUIRE(d20.result == 17);
}

TEST_CASE("roll_d20 with Disadvantage takes the lower of two rolls", "[dice]") {
    SequenceRandomSource sequence({9, 17});
    dnd::Dice dice(sequence);

    auto d20 = dice.roll_d20(dnd::RollMode::Disadvantage);
    REQUIRE(d20.rolls == std::vector<int>{9, 17});
    REQUIRE(d20.result == 9);
}
