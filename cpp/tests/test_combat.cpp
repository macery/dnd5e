#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "dnd/combat.hpp"

namespace {

class FixedRandomSource : public dnd::IRandomSource {
public:
    explicit FixedRandomSource(int value) : value_(value) {}
    int roll_die(int /*sides*/) override { return value_; }

private:
    int value_;
};

// Returns the first value for the initial roll, then the second value for
// every roll after that -- enough to give the attack roll one value and the
// damage roll(s) another, deterministically.
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

TEST_CASE("resolve_attack: a natural 1 always misses", "[combat]") {
    FixedRandomSource fixed(1);
    dnd::Dice dice(fixed);

    // Attack bonus is huge and target AC is trivially low -- would hit under
    // ordinary math, but a natural 1 always misses regardless.
    auto result = dnd::resolve_attack(dice, /*attack_bonus=*/50, /*target_ac=*/1, 1, 6, 0);

    REQUIRE(result.attack_roll == 1);
    REQUIRE_FALSE(result.hit);
    REQUIRE_FALSE(result.critical);
    REQUIRE(result.damage_total == 0);
    REQUIRE(result.damage_dice.empty());
}

TEST_CASE("resolve_attack: a natural 20 always hits and doubles damage dice", "[combat]") {
    SequenceRandomSource sequence({20, 4, 5});  // attack roll, then two damage dice
    dnd::Dice dice(sequence);

    // Target AC is unreachable by the math alone -- a natural 20 still hits.
    auto result = dnd::resolve_attack(dice, /*attack_bonus=*/0, /*target_ac=*/100, 1, 6, 3);

    REQUIRE(result.attack_roll == 20);
    REQUIRE(result.hit);
    REQUIRE(result.critical);
    REQUIRE(result.damage_dice == std::vector<int>{4, 5});  // doubled from 1 die to 2
    REQUIRE(result.damage_total == 4 + 5 + 3);
}

TEST_CASE("resolve_attack: an ordinary roll hits or misses by the threshold", "[combat]") {
    FixedRandomSource fixed(10);
    dnd::Dice dice(fixed);

    SECTION("hits when roll + bonus >= AC") {
        auto result = dnd::resolve_attack(dice, /*attack_bonus=*/5, /*target_ac=*/15, 1, 8, 2);
        REQUIRE(result.hit);
        REQUIRE_FALSE(result.critical);
        // The fixed source returns 10 regardless of die size, same for the damage roll.
        REQUIRE(result.damage_dice == std::vector<int>{10});
        REQUIRE(result.damage_total == 12);
    }

    SECTION("misses when roll + bonus < AC") {
        auto result = dnd::resolve_attack(dice, /*attack_bonus=*/1, /*target_ac=*/15, 1, 8, 2);
        REQUIRE_FALSE(result.hit);
        REQUIRE(result.damage_total == 0);
        REQUIRE(result.damage_dice.empty());
    }
}
