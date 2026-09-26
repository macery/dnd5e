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
