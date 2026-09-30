#include <catch2/catch_test_macros.hpp>

#include "dnd/class_features.hpp"
#include "dnd/turn.hpp"

namespace {
dnd::Spell spell(const char* name, int level, dnd::CastingTime time) {
    dnd::Spell s;
    s.name = name;
    s.level = level;
    s.casting_time = time;
    return s;
}
const dnd::Spell kHealingWord = spell("Healing Word", 1, dnd::CastingTime::BonusAction);
const dnd::Spell kSacredFlame = spell("Sacred Flame", 0, dnd::CastingTime::Action);
const dnd::Spell kGuidingBolt = spell("Guiding Bolt", 1, dnd::CastingTime::Action);
const dnd::Spell kFeatherFall = spell("Feather Fall", 1, dnd::CastingTime::Reaction);
}  // namespace

TEST_CASE("Extra Attack", "[turn][class_features]") {
    REQUIRE(dnd::attacks_per_attack_action("Fighter", 4) == 1);
    REQUIRE(dnd::attacks_per_attack_action("Fighter", 5) == 2);
    REQUIRE(dnd::attacks_per_attack_action("Fighter", 11) == 3);
    REQUIRE(dnd::attacks_per_attack_action("Fighter", 20) == 4);
    REQUIRE(dnd::attacks_per_attack_action("Barbarian", 5) == 2);
    REQUIRE(dnd::attacks_per_attack_action("Paladin", 5) == 2);
    REQUIRE(dnd::attacks_per_attack_action("Ranger", 5) == 2);
    REQUIRE(dnd::attacks_per_attack_action("Monk", 5) == 2);
    REQUIRE(dnd::attacks_per_attack_action("Monk", 20) == 2);
    REQUIRE(dnd::attacks_per_attack_action("Rogue", 5) == 1);
    REQUIRE(dnd::attacks_per_attack_action("Wizard", 20) == 1);
}

TEST_CASE("the bonus-action spell rule", "[turn]") {
    dnd::TurnState turn;
    REQUIRE_FALSE(dnd::spell_casting_blocked(turn, kHealingWord));
    REQUIRE_FALSE(dnd::spell_casting_blocked(turn, kGuidingBolt));
    REQUIRE(dnd::spell_casting_blocked(turn, kFeatherFall));  // reactions aren't supported

    SECTION("a bonus-action spell first: then only an action cantrip") {
        dnd::record_spell_cast(turn, kHealingWord);
        REQUIRE(turn.bonus_action_used);
        REQUIRE(turn.actions_left == 1);
        REQUIRE_FALSE(dnd::spell_casting_blocked(turn, kSacredFlame));
        REQUIRE(dnd::spell_casting_blocked(turn, kGuidingBolt));
        REQUIRE(dnd::spell_casting_blocked(turn, kHealingWord));  // bonus action used
    }

    SECTION("a levelled action spell first: no bonus-action spell after") {
        dnd::record_spell_cast(turn, kGuidingBolt);
        REQUIRE(turn.actions_left == 0);
        REQUIRE(dnd::spell_casting_blocked(turn, kHealingWord));
        REQUIRE(dnd::spell_casting_blocked(turn, kSacredFlame));  // no action left
    }

    SECTION("an action cantrip first: a bonus-action spell is still fine") {
        dnd::record_spell_cast(turn, kSacredFlame);
        REQUIRE_FALSE(dnd::spell_casting_blocked(turn, kHealingWord));
    }

    SECTION("two actions (Action Surge): two levelled action spells are allowed") {
        turn.actions_left = 2;
        dnd::record_spell_cast(turn, kGuidingBolt);
        REQUIRE_FALSE(dnd::spell_casting_blocked(turn, kGuidingBolt));
    }
}
