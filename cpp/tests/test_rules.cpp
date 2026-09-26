#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "dnd/rules.hpp"

TEST_CASE("ability_modifier matches the 5e table", "[rules]") {
    REQUIRE(dnd::ability_modifier(1) == -5);
    REQUIRE(dnd::ability_modifier(8) == -1);
    REQUIRE(dnd::ability_modifier(9) == -1);
    REQUIRE(dnd::ability_modifier(10) == 0);
    REQUIRE(dnd::ability_modifier(11) == 0);
    REQUIRE(dnd::ability_modifier(15) == 2);
    REQUIRE(dnd::ability_modifier(20) == 5);
    REQUIRE(dnd::ability_modifier(30) == 10);
}

TEST_CASE("proficiency_bonus matches the 5e level brackets", "[rules]") {
    REQUIRE(dnd::proficiency_bonus(1) == 2);
    REQUIRE(dnd::proficiency_bonus(4) == 2);
    REQUIRE(dnd::proficiency_bonus(5) == 3);
    REQUIRE(dnd::proficiency_bonus(8) == 3);
    REQUIRE(dnd::proficiency_bonus(9) == 4);
    REQUIRE(dnd::proficiency_bonus(12) == 4);
    REQUIRE(dnd::proficiency_bonus(13) == 5);
    REQUIRE(dnd::proficiency_bonus(16) == 5);
    REQUIRE(dnd::proficiency_bonus(17) == 6);
    REQUIRE(dnd::proficiency_bonus(20) == 6);
    REQUIRE_THROWS_AS(dnd::proficiency_bonus(0), std::invalid_argument);
}

TEST_CASE("ability_for_skill matches the 5e skill list", "[rules]") {
    REQUIRE(dnd::ability_for_skill(dnd::Skill::Stealth) == dnd::Ability::Dexterity);
    REQUIRE(dnd::ability_for_skill(dnd::Skill::Athletics) == dnd::Ability::Strength);
    REQUIRE(dnd::ability_for_skill(dnd::Skill::Arcana) == dnd::Ability::Intelligence);
    REQUIRE(dnd::ability_for_skill(dnd::Skill::Perception) == dnd::Ability::Wisdom);
    REQUIRE(dnd::ability_for_skill(dnd::Skill::Persuasion) == dnd::Ability::Charisma);
}

TEST_CASE("skill_bonus and saving_throw_bonus respect proficiency", "[rules]") {
    dnd::Character character;
    character.level = 5;                 // proficiency bonus +3
    character.dexterity = 16;             // modifier +3
    character.wisdom = 14;                // modifier +2

    SECTION("without proficiency, it's just the ability modifier") {
        REQUIRE(dnd::skill_bonus(character, dnd::Skill::Stealth) == 3);
        REQUIRE(dnd::saving_throw_bonus(character, dnd::Ability::Dexterity) == 3);
    }

    SECTION("with proficiency, the proficiency bonus is added") {
        character.skill_proficiencies = 1u << static_cast<int>(dnd::Skill::Stealth);
        character.save_proficiencies = 1u << static_cast<int>(dnd::Ability::Wisdom);

        REQUIRE(dnd::has_skill_proficiency(character, dnd::Skill::Stealth));
        REQUIRE_FALSE(dnd::has_skill_proficiency(character, dnd::Skill::Athletics));

        REQUIRE(dnd::skill_bonus(character, dnd::Skill::Stealth) == 6);       // +3 dex +3 prof
        REQUIRE(dnd::saving_throw_bonus(character, dnd::Ability::Wisdom) == 5);  // +2 wis +3 prof
    }
}

TEST_CASE("passive_perception and initiative are derived correctly", "[rules]") {
    dnd::Character character;
    character.level = 1;
    character.dexterity = 14;   // modifier +2
    character.wisdom = 16;      // modifier +3
    character.skill_proficiencies = 1u << static_cast<int>(dnd::Skill::Perception);

    REQUIRE(dnd::initiative(character) == 2);
    REQUIRE(dnd::passive_perception(character) == 10 + 3 + 2);  // 10 + wis mod + proficiency bonus
}

TEST_CASE("weapon_attack_ability_modifier picks the right ability", "[rules]") {
    dnd::Character character;
    character.strength = 16;   // modifier +3
    character.dexterity = 14;  // modifier +2

    dnd::Weapon longsword;
    longsword.is_finesse = false;
    longsword.is_ranged = false;
    REQUIRE(dnd::weapon_attack_ability_modifier(character, longsword) == 3);

    dnd::Weapon longbow;
    longbow.is_ranged = true;
    REQUIRE(dnd::weapon_attack_ability_modifier(character, longbow) == 2);

    dnd::Weapon rapier;
    rapier.is_finesse = true;
    REQUIRE(dnd::weapon_attack_ability_modifier(character, rapier) == 3);  // STR still higher

    character.dexterity = 18;  // modifier +4, now higher than STR
    REQUIRE(dnd::weapon_attack_ability_modifier(character, rapier) == 4);
}

TEST_CASE("weapon_attack_bonus adds the level's proficiency bonus", "[rules]") {
    dnd::Character character;
    character.level = 5;       // proficiency bonus +3
    character.strength = 16;   // modifier +3

    dnd::Weapon longsword;
    REQUIRE(dnd::weapon_attack_bonus(character, longsword) == 6);
}
