#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "dnd/damage.hpp"

using dnd::DamageModifier;

TEST_CASE("damage modifiers from resistances, vulnerabilities, and immunities", "[damage]") {
    dnd::DamageTraits skeleton;
    skeleton.vulnerabilities = {"bludgeoning"};
    skeleton.immunities = {"poison"};
    REQUIRE(dnd::damage_modifier(skeleton, "bludgeoning") == DamageModifier::Vulnerable);
    REQUIRE(dnd::damage_modifier(skeleton, "Bludgeoning") == DamageModifier::Vulnerable);  // case-insensitive
    REQUIRE(dnd::damage_modifier(skeleton, "poison") == DamageModifier::Immune);
    REQUIRE(dnd::damage_modifier(skeleton, "slashing") == DamageModifier::Normal);
    REQUIRE(dnd::damage_modifier(skeleton, "") == DamageModifier::Normal);  // untyped

    dnd::DamageTraits both;
    both.resistances = {"fire"};
    both.vulnerabilities = {"fire"};
    REQUIRE(dnd::damage_modifier(both, "fire") == DamageModifier::Normal);  // they cancel
    both.immunities = {"fire"};
    REQUIRE(dnd::damage_modifier(both, "fire") == DamageModifier::Immune);  // immunity wins
}

TEST_CASE("adjusting damage", "[damage]") {
    REQUIRE(dnd::adjust_damage(7, DamageModifier::Resisted) == 3);  // rounded down
    REQUIRE(dnd::adjust_damage(7, DamageModifier::Vulnerable) == 14);
    REQUIRE(dnd::adjust_damage(7, DamageModifier::Immune) == 0);
    REQUIRE(dnd::adjust_damage(7, DamageModifier::Normal) == 7);
    REQUIRE(dnd::damage_modifier_note(DamageModifier::Resisted, "slashing") == "halved: resistant to slashing");
    REQUIRE(dnd::damage_modifier_note(DamageModifier::Vulnerable, "bludgeoning") ==
            "doubled: vulnerable to bludgeoning");
    REQUIRE(dnd::damage_modifier_note(DamageModifier::Immune, "poison") == "immune to poison");
    REQUIRE(dnd::damage_modifier_note(DamageModifier::Normal, "fire").empty());
}

TEST_CASE("comma lists", "[damage]") {
    REQUIRE(dnd::split_list("") == std::vector<std::string>{});
    REQUIRE(dnd::split_list("bludgeoning, piercing,slashing ") ==
            std::vector<std::string>{"bludgeoning", "piercing", "slashing"});
    REQUIRE(dnd::join_list({"fire", "poison"}) == "fire, poison");
}
