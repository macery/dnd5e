#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <vector>

#include "dnd/progression.hpp"

namespace {

class FixedRandomSource : public dnd::IRandomSource {
public:
    explicit FixedRandomSource(int value) : value_(value) {}
    int roll_die(int /*sides*/) override { return value_; }

private:
    int value_;
};

}  // namespace

TEST_CASE("xp_for_challenge_rating matches the 5e table", "[progression]") {
    REQUIRE(dnd::xp_for_challenge_rating(0.0) == 10);
    REQUIRE(dnd::xp_for_challenge_rating(0.125) == 25);
    REQUIRE(dnd::xp_for_challenge_rating(0.25) == 50);
    REQUIRE(dnd::xp_for_challenge_rating(0.5) == 100);
    REQUIRE(dnd::xp_for_challenge_rating(1.0) == 200);
    REQUIRE(dnd::xp_for_challenge_rating(5.0) == 1800);
    REQUIRE(dnd::xp_for_challenge_rating(30.0) == 155000);
    REQUIRE_THROWS_AS(dnd::xp_for_challenge_rating(0.75), std::invalid_argument);
    REQUIRE_THROWS_AS(dnd::xp_for_challenge_rating(31.0), std::invalid_argument);
}

TEST_CASE("level thresholds match the 5e advancement table", "[progression]") {
    REQUIRE(dnd::xp_threshold_for_level(1) == 0);
    REQUIRE(dnd::xp_threshold_for_level(2) == 300);
    REQUIRE(dnd::xp_threshold_for_level(5) == 6500);
    REQUIRE(dnd::xp_threshold_for_level(20) == 355000);
    REQUIRE_THROWS_AS(dnd::xp_threshold_for_level(0), std::invalid_argument);
    REQUIRE_THROWS_AS(dnd::xp_threshold_for_level(21), std::invalid_argument);

    REQUIRE(dnd::level_for_xp(0) == 1);
    REQUIRE(dnd::level_for_xp(299) == 1);
    REQUIRE(dnd::level_for_xp(300) == 2);
    REQUIRE(dnd::level_for_xp(2699) == 3);
    REQUIRE(dnd::level_for_xp(1000000) == 20);
}

TEST_CASE("hit_die_for_class covers every seeded class", "[progression]") {
    REQUIRE(dnd::hit_die_for_class("Barbarian") == 12);
    REQUIRE(dnd::hit_die_for_class("Fighter") == 10);
    REQUIRE(dnd::hit_die_for_class("Rogue") == 8);
    REQUIRE(dnd::hit_die_for_class("Wizard") == 6);
    REQUIRE_THROWS_AS(dnd::hit_die_for_class("Artificer"), std::invalid_argument);

    std::vector<dnd::LookupEntry> classes = {{1, "Barbarian"}, {2, "Artificer"}};
    REQUIRE(dnd::hit_die_for_class_id(classes, 1) == 12);
    REQUIRE(dnd::hit_die_for_class_id(classes, 2) == 8);   // unknown name
    REQUIRE(dnd::hit_die_for_class_id(classes, 99) == 8);  // unknown id
}

TEST_CASE("level_up_hp_gain uses the fixed average plus CON, minimum 1", "[progression]") {
    REQUIRE(dnd::level_up_hp_gain(10, 14) == 8);  // 6 + 2
    REQUIRE(dnd::level_up_hp_gain(6, 10) == 4);   // 4 + 0
    REQUIRE(dnd::level_up_hp_gain(6, 1) == 1);    // 4 - 5, floored at 1
}

TEST_CASE("award_experience levels up and grows HP", "[progression]") {
    dnd::Character character;
    character.constitution = 14;  // +2
    character.max_hp = 12;
    character.current_hp = 5;

    SECTION("below the next threshold, only XP changes") {
        REQUIRE(dnd::award_experience(character, 299, 10) == 0);
        REQUIRE(character.experience == 299);
        REQUIRE(character.level == 1);
        REQUIRE(character.max_hp == 12);
    }

    SECTION("crossing several thresholds applies every level") {
        REQUIRE(dnd::award_experience(character, 900, 10) == 2);
        REQUIRE(character.level == 3);
        REQUIRE(character.max_hp == 12 + 2 * 8);
        REQUIRE(character.current_hp == 5 + 2 * 8);
    }

    SECTION("never goes past level 20") {
        character.level = 20;
        character.experience = 355000;
        REQUIRE(dnd::award_experience(character, 100000, 10) == 0);
        REQUIRE(character.level == 20);
    }
}

TEST_CASE("short rest spends hit dice; long rest recovers them", "[progression]") {
    FixedRandomSource fixed(4);
    dnd::Dice dice(fixed);

    dnd::Character character;
    character.level = 4;
    character.constitution = 12;  // +1
    character.max_hp = 30;
    character.current_hp = 10;

    REQUIRE(dnd::hit_dice_remaining(character) == 4);
    REQUIRE(dnd::spend_hit_die(character, dice, 8) == 5);  // 4 + 1
    REQUIRE(character.current_hp == 15);
    REQUIRE(dnd::hit_dice_remaining(character) == 3);

    SECTION("healing is capped at max HP") {
        character.current_hp = 28;
        REQUIRE(dnd::spend_hit_die(character, dice, 8) == 2);
        REQUIRE(character.current_hp == 30);
    }

    SECTION("with no hit dice left, nothing happens") {
        character.hit_dice_used = 4;
        REQUIRE(dnd::spend_hit_die(character, dice, 8) == -1);
        REQUIRE(character.current_hp == 15);
    }

    SECTION("long rest restores HP and half the hit dice (minimum one)") {
        character.hit_dice_used = 4;
        dnd::take_long_rest(character);
        REQUIRE(character.current_hp == 30);
        REQUIRE(character.hit_dice_used == 2);

        character.level = 1;
        character.hit_dice_used = 1;
        dnd::take_long_rest(character);
        REQUIRE(character.hit_dice_used == 0);
    }
}

TEST_CASE("rests never revive the dead", "[progression]") {
    FixedRandomSource fixed(4);
    dnd::Dice dice(fixed);
    dnd::Character character;
    character.level = 2;
    character.max_hp = 20;
    character.current_hp = 0;
    character.dead = true;

    dnd::take_long_rest(character);
    REQUIRE(character.current_hp == 0);
    REQUIRE(dnd::spend_hit_die(character, dice, 8) == -1);
    REQUIRE(character.current_hp == 0);
}
