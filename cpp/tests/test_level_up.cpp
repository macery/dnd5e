#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>

#include "dnd/level_up.hpp"
#include "dnd/progression.hpp"

using dnd::Ability;
using dnd::HitPointMethod;

namespace {

const dnd::ClassInfo& info(const char* name) {
    const dnd::ClassInfo* found = dnd::find_class_info(name);
    REQUIRE(found != nullptr);
    return *found;
}

// A character at `level` with the XP for the next level already earned.
dnd::Character ready(int level, int con = 14) {
    dnd::Character c;
    c.character_name = "Aria";
    c.level = level;
    c.experience = dnd::xp_threshold_for_level(level + 1);
    c.strength = 16;
    c.dexterity = 12;
    c.constitution = con;
    c.intelligence = 10;
    c.wisdom = 12;
    c.charisma = 14;
    c.max_hp = 10 * level;
    c.current_hp = 4;
    return c;
}

const dnd::ScalingChange* change(const dnd::LevelUpPlan& plan, const std::string& label) {
    for (const auto& c : plan.changes) {
        if (c.label == label) return &c;
    }
    return nullptr;
}

bool has_feature_named(const dnd::LevelUpPlan& plan, const std::string& name) {
    return std::any_of(plan.features.begin(), plan.features.end(),
                       [&](const dnd::ClassFeatureName& f) { return name == f.name; });
}

}  // namespace

TEST_CASE("pending levels follow XP", "[level_up]") {
    dnd::Character c;
    REQUIRE(dnd::pending_levels(c) == 0);
    c.experience = 900;  // level 3
    REQUIRE(dnd::pending_levels(c) == 2);
    c.level = 3;
    REQUIRE(dnd::pending_levels(c) == 0);
    c.level = 5;  // ahead of XP (set by hand before levels followed XP)
    REQUIRE(dnd::pending_levels(c) == 0);
}

TEST_CASE("Ability Score Improvement levels", "[level_up]") {
    for (int level : {4, 8, 12, 16, 19}) REQUIRE(dnd::has_ability_score_improvement("Wizard", level));
    REQUIRE_FALSE(dnd::has_ability_score_improvement("Wizard", 6));
    REQUIRE(dnd::has_ability_score_improvement("Fighter", 6));
    REQUIRE(dnd::has_ability_score_improvement("Fighter", 14));
    REQUIRE_FALSE(dnd::has_ability_score_improvement("Fighter", 10));
    REQUIRE(dnd::has_ability_score_improvement("Rogue", 10));
}

TEST_CASE("every class has a PHB feature table for levels 1-20", "[level_up]") {
    for (const auto& c : dnd::class_infos()) {
        const auto& table = dnd::class_feature_names(c.name);
        INFO(c.name);
        REQUIRE_FALSE(table.empty());
        REQUIRE(table.front().level == 1);
        REQUIRE(std::is_sorted(table.begin(), table.end(),
                               [](const auto& a, const auto& b) { return a.level < b.level; }));
        for (const auto& feature : table) {
            REQUIRE(feature.level >= 1);
            REQUIRE(feature.level <= 20);
            REQUIRE(std::string(feature.name).find("Ability Score") == std::string::npos);
        }
    }
    auto fifth = dnd::class_features_at("Fighter", 5);
    REQUIRE(fifth.size() == 1);
    REQUIRE(std::string(fifth[0].name) == "Extra Attack");
    REQUIRE(fifth[0].automated);
    REQUIRE(dnd::class_features_at("Rogue", 5).front().automated);  // Uncanny Dodge: a reaction now
    REQUIRE_FALSE(dnd::class_features_at("Rogue", 7).front().automated);  // Evasion: shown only
}

TEST_CASE("a level-up plan says what the next level brings", "[level_up]") {
    SECTION("Fighter 3 -> 4: an ASI") {
        auto plan = dnd::level_up_plan(ready(3), info("Fighter"));
        REQUIRE(plan.new_level == 4);
        REQUIRE(plan.hit_die == 10);
        REQUIRE(plan.average_hp == 6);
        REQUIRE(plan.ability_score_improvement);
        REQUIRE_FALSE(plan.fighting_style);
        REQUIRE_FALSE(plan.spellcaster);
    }

    SECTION("Fighter 4 -> 5: proficiency and Extra Attack") {
        auto plan = dnd::level_up_plan(ready(4), info("Fighter"));
        REQUIRE(has_feature_named(plan, "Extra Attack"));
        REQUIRE(change(plan, "Proficiency bonus")->before == "+2");
        REQUIRE(change(plan, "Proficiency bonus")->after == "+3");
        REQUIRE(change(plan, "Attacks per Attack action")->before.empty());
        REQUIRE(change(plan, "Attacks per Attack action")->after == "2");
        REQUIRE(change(plan, "Max HP")->after == "48");  // 40 + 6 + CON 2
    }

    SECTION("Paladin 1 -> 2: Fighting Style, spells, Divine Smite") {
        auto plan = dnd::level_up_plan(ready(1), info("Paladin"));
        REQUIRE(plan.fighting_style);
        REQUIRE(plan.fighting_style_options.size() == 3);
        REQUIRE(has_feature_named(plan, "Divine Smite"));
        REQUIRE(plan.spellcaster);
        REQUIRE(plan.spells_after.max_spell_level == 1);
        REQUIRE(change(plan, "Spell slots")->after == "2 × 1st");
        REQUIRE(change(plan, "Lay on Hands pool (HP)")->after == "10");
    }

    SECTION("Monk 1 -> 2: ki") {
        auto plan = dnd::level_up_plan(ready(1), info("Monk"));
        REQUIRE(change(plan, "Ki points")->before.empty());
        REQUIRE(change(plan, "Ki points")->after == "2");
    }

    SECTION("Wizard 1 -> 2: more spells") {
        auto plan = dnd::level_up_plan(ready(1), info("Wizard"));
        REQUIRE(plan.spells_after.spells > plan.spells_before.spells);
    }

    SECTION("not past 20") {
        dnd::Character max;
        max.level = 20;
        REQUIRE_THROWS_AS(dnd::level_up_plan(max, info("Fighter")), std::invalid_argument);
    }
}

TEST_CASE("level-up choices are validated", "[level_up]") {
    auto fighter = ready(3);
    auto plan = dnd::level_up_plan(fighter, info("Fighter"));
    dnd::LevelUpChoices choices;
    choices.ability_increases = {Ability::Strength, Ability::Strength};
    REQUIRE_FALSE(dnd::validate_level_up(fighter, plan, choices).has_value());

    SECTION("needs the XP") {
        fighter.experience = 2000;
        REQUIRE(dnd::validate_level_up(fighter, plan, choices) == "Aria needs 2700 XP for level 4.");
    }
    SECTION("the ASI is required at an ASI level") {
        choices.ability_increases.clear();
        REQUIRE(dnd::validate_level_up(fighter, plan, choices).has_value());
        choices.ability_increases = {Ability::Strength};
        REQUIRE(dnd::validate_level_up(fighter, plan, choices).has_value());
    }
    SECTION("no score above 20") {
        fighter.strength = 19;
        REQUIRE(dnd::validate_level_up(fighter, plan, choices) == "Strength can't go above 20.");
        choices.ability_increases = {Ability::Strength, Ability::Constitution};
        REQUIRE_FALSE(dnd::validate_level_up(fighter, plan, choices).has_value());
    }
    SECTION("no ASI when none is due") {
        auto two = ready(1);
        auto plan2 = dnd::level_up_plan(two, info("Fighter"));
        REQUIRE(dnd::validate_level_up(two, plan2, choices).has_value());
    }
    SECTION("the HP roll fits the die") {
        choices.hp_method = HitPointMethod::Roll;
        choices.hp_roll = 11;
        REQUIRE(dnd::validate_level_up(fighter, plan, choices) == "Roll your hit die (1 to 10).");
        choices.hp_roll = 10;
        REQUIRE_FALSE(dnd::validate_level_up(fighter, plan, choices).has_value());
    }
    SECTION("a due Fighting Style must be one the class can take") {
        auto ranger = ready(1);
        auto rangerPlan = dnd::level_up_plan(ranger, info("Ranger"));
        dnd::LevelUpChoices rangerChoices;
        REQUIRE(dnd::validate_level_up(ranger, rangerPlan, rangerChoices) == "Choose a Fighting Style.");
        rangerChoices.fighting_style = "GreatWeaponFighting";
        REQUIRE(dnd::validate_level_up(ranger, rangerPlan, rangerChoices) == "Choose a Fighting Style.");
        rangerChoices.fighting_style = "Archery";
        REQUIRE_FALSE(dnd::validate_level_up(ranger, rangerPlan, rangerChoices).has_value());
    }
}

TEST_CASE("level-up HP: roll or average, plus CON, and CON raises are retroactive", "[level_up]") {
    auto fighter = ready(3, 15);  // CON +2
    auto plan = dnd::level_up_plan(fighter, info("Fighter"));
    dnd::LevelUpChoices choices;
    choices.ability_increases = {Ability::Strength, Ability::Strength};
    REQUIRE(dnd::level_up_hp_increase(fighter, plan, choices) == 8);  // 6 + 2

    choices.hp_method = HitPointMethod::Roll;
    choices.hp_roll = 3;
    REQUIRE(dnd::level_up_hp_increase(fighter, plan, choices) == 5);  // 3 + 2

    // CON 15 -> 16 (+3): 3 + 3 for this level, +1 for each of the 3 earlier levels.
    choices.ability_increases = {Ability::Strength, Ability::Constitution};
    REQUIRE(dnd::level_up_hp_increase(fighter, plan, choices) == 9);

    // Never less than 1 for the new level.
    auto frail = ready(1, 3);  // CON -4
    auto frailPlan = dnd::level_up_plan(frail, info("Wizard"));
    dnd::LevelUpChoices low;
    low.hp_method = HitPointMethod::Roll;
    low.hp_roll = 1;
    REQUIRE(dnd::level_up_hp_increase(frail, frailPlan, low) == 1);
}

TEST_CASE("apply_level_up", "[level_up]") {
    SECTION("an ASI level") {
        auto fighter = ready(3);
        auto plan = dnd::level_up_plan(fighter, info("Fighter"));
        dnd::LevelUpChoices choices;
        choices.ability_increases = {Ability::Strength, Ability::Dexterity};
        dnd::apply_level_up(fighter, plan, choices);
        REQUIRE(fighter.level == 4);
        REQUIRE(fighter.strength == 17);
        REQUIRE(fighter.dexterity == 13);
        REQUIRE(fighter.max_hp == 38);  // 30 + 6 + 2
        REQUIRE(fighter.current_hp == 12);
        REQUIRE(dnd::pending_levels(fighter) == 0);
        REQUIRE_THROWS_AS(dnd::apply_level_up(fighter, plan, choices), std::invalid_argument);  // stale plan
    }

    SECTION("a Paladin's Fighting Style at level 2") {
        auto paladin = ready(1);
        auto plan = dnd::level_up_plan(paladin, info("Paladin"));
        dnd::LevelUpChoices choices;
        choices.fighting_style = "Defense";
        dnd::apply_level_up(paladin, plan, choices);
        REQUIRE(paladin.level == 2);
        REQUIRE(dnd::fighting_style(paladin, "Paladin") == dnd::FightingStyle::Defense);
    }

    SECTION("several levels, one at a time") {
        auto rogue = ready(1);
        rogue.experience = 2700;  // level 4
        REQUIRE(dnd::pending_levels(rogue) == 3);
        while (dnd::pending_levels(rogue) > 0) {
            auto plan = dnd::level_up_plan(rogue, info("Rogue"));
            dnd::LevelUpChoices choices;
            if (plan.ability_score_improvement) choices.ability_increases = {Ability::Dexterity, Ability::Dexterity};
            dnd::apply_level_up(rogue, plan, choices);
        }
        REQUIRE(rogue.level == 4);
        REQUIRE(rogue.dexterity == 14);
    }
}
