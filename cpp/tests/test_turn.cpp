#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>

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

TEST_CASE("which classes have which limited-use features", "[class_features]") {
    using dnd::Feature;
    REQUIRE(dnd::has_feature(Feature::SecondWind, "Fighter", 1));
    REQUIRE_FALSE(dnd::has_feature(Feature::ActionSurge, "Fighter", 1));
    REQUIRE(dnd::has_feature(Feature::ActionSurge, "Fighter", 2));
    REQUIRE(dnd::has_feature(Feature::Rage, "Barbarian", 1));
    REQUIRE_FALSE(dnd::has_feature(Feature::Rage, "Fighter", 20));
    REQUIRE(dnd::has_feature(Feature::BardicInspiration, "Bard", 1));
    REQUIRE(dnd::has_feature(Feature::ArcaneRecovery, "Wizard", 1));
    REQUIRE_FALSE(dnd::has_feature(Feature::Ki, "Monk", 1));
    REQUIRE(dnd::has_feature(Feature::Ki, "Monk", 2));
    REQUIRE(dnd::has_feature(Feature::LayOnHands, "Paladin", 1));
    for (const auto& info : dnd::limited_features()) REQUIRE(std::string(info.description).size() > 20);
}

TEST_CASE("feature uses per rest", "[class_features]") {
    using dnd::Feature;
    dnd::Character barbarian;
    barbarian.level = 1;
    REQUIRE(dnd::feature_max_uses(Feature::Rage, barbarian, "Barbarian") == 2);
    barbarian.level = 3;
    REQUIRE(dnd::feature_max_uses(Feature::Rage, barbarian, "Barbarian") == 3);
    barbarian.level = 6;
    REQUIRE(dnd::feature_max_uses(Feature::Rage, barbarian, "Barbarian") == 4);
    REQUIRE(dnd::feature_max_uses(Feature::Rage, barbarian, "Fighter") == 0);

    dnd::Character bard;
    bard.level = 1;
    bard.charisma = 16;
    REQUIRE(dnd::feature_max_uses(Feature::BardicInspiration, bard, "Bard") == 3);
    bard.charisma = 8;
    REQUIRE(dnd::feature_max_uses(Feature::BardicInspiration, bard, "Bard") == 1);  // minimum 1
    REQUIRE(dnd::feature_recovery(Feature::BardicInspiration, 4) == dnd::Recovery::LongRest);
    REQUIRE(dnd::feature_recovery(Feature::BardicInspiration, 5) == dnd::Recovery::ShortRest);

    dnd::Character fighter;
    fighter.level = 2;
    REQUIRE(dnd::spend_feature(fighter, Feature::SecondWind, "Fighter"));
    REQUIRE_FALSE(dnd::spend_feature(fighter, Feature::SecondWind, "Fighter"));
    REQUIRE(dnd::spend_feature(fighter, Feature::ActionSurge, "Fighter"));
    REQUIRE(fighter.feature_uses_spent.at("SecondWind") == 1);

    barbarian.level = 1;
    REQUIRE(dnd::spend_feature(barbarian, Feature::Rage, "Barbarian"));
    REQUIRE(dnd::feature_uses_left(Feature::Rage, barbarian, "Barbarian") == 1);

    // A short rest brings back Second Wind and Action Surge, not Rage.
    dnd::restore_short_rest_features(fighter, "Fighter");
    REQUIRE(dnd::feature_uses_left(Feature::SecondWind, fighter, "Fighter") == 1);
    REQUIRE(dnd::feature_uses_left(Feature::ActionSurge, fighter, "Fighter") == 1);
    dnd::restore_short_rest_features(barbarian, "Barbarian");
    REQUIRE(dnd::feature_uses_left(Feature::Rage, barbarian, "Barbarian") == 1);
    dnd::restore_all_features(barbarian);
    REQUIRE(dnd::feature_uses_left(Feature::Rage, barbarian, "Barbarian") == 2);
}

TEST_CASE("feature formulas follow the PHB class tables", "[class_features]") {
    REQUIRE(dnd::rage_damage_bonus(1) == 2);
    REQUIRE(dnd::rage_damage_bonus(9) == 3);
    REQUIRE(dnd::rage_damage_bonus(16) == 4);
    REQUIRE(dnd::sneak_attack_dice(1) == 1);
    REQUIRE(dnd::sneak_attack_dice(2) == 1);
    REQUIRE(dnd::sneak_attack_dice(3) == 2);
    REQUIRE(dnd::sneak_attack_dice(20) == 10);
    REQUIRE(dnd::martial_arts_die(1) == 4);
    REQUIRE(dnd::martial_arts_die(5) == 6);
    REQUIRE(dnd::martial_arts_die(17) == 10);
    REQUIRE(dnd::bardic_inspiration_die(1) == 6);
    REQUIRE(dnd::bardic_inspiration_die(5) == 8);
    REQUIRE(dnd::bardic_inspiration_die(15) == 12);
    REQUIRE(dnd::second_wind_bonus(3) == 3);
    REQUIRE(dnd::arcane_recovery_budget(1) == 1);
    REQUIRE(dnd::arcane_recovery_budget(5) == 3);
    REQUIRE(dnd::divine_smite_dice(1) == 2);
    REQUIRE(dnd::divine_smite_dice(3) == 4);
    REQUIRE(dnd::divine_smite_dice(4) == 5);
    REQUIRE(dnd::divine_smite_dice(9) == 5);  // max 5d8
}

TEST_CASE("Ki points and the Lay on Hands pool", "[class_features]") {
    using dnd::Feature;
    dnd::Character monk;
    monk.level = 3;
    REQUIRE(dnd::feature_max_uses(Feature::Ki, monk, "Monk") == 3);
    REQUIRE(dnd::spend_feature(monk, Feature::Ki, "Monk"));
    REQUIRE(dnd::spend_feature(monk, Feature::Ki, "Monk"));
    REQUIRE(dnd::feature_uses_left(Feature::Ki, monk, "Monk") == 1);
    dnd::restore_short_rest_features(monk, "Monk");  // ki comes back on a short rest
    REQUIRE(dnd::feature_uses_left(Feature::Ki, monk, "Monk") == 3);

    dnd::Character paladin;
    paladin.level = 2;
    REQUIRE(dnd::feature_max_uses(Feature::LayOnHands, paladin, "Paladin") == 10);
    REQUIRE(dnd::spend_feature(paladin, Feature::LayOnHands, "Paladin", 7));
    REQUIRE_FALSE(dnd::spend_feature(paladin, Feature::LayOnHands, "Paladin", 4));  // only 3 left
    REQUIRE_FALSE(dnd::spend_feature(paladin, Feature::LayOnHands, "Paladin", 0));
    REQUIRE(dnd::feature_uses_left(Feature::LayOnHands, paladin, "Paladin") == 3);
    dnd::restore_short_rest_features(paladin, "Paladin");  // the pool needs a long rest
    REQUIRE(dnd::feature_uses_left(Feature::LayOnHands, paladin, "Paladin") == 3);
    dnd::restore_all_features(paladin);
    REQUIRE(dnd::feature_uses_left(Feature::LayOnHands, paladin, "Paladin") == 10);
}

TEST_CASE("Fighting Styles belong to Fighters", "[class_features]") {
    REQUIRE(dnd::fighting_styles().size() == 4);
    dnd::Character c;
    c.fighting_style = "GreatWeaponFighting";
    REQUIRE(dnd::fighting_style(c, "Fighter") == dnd::FightingStyle::GreatWeaponFighting);
    REQUIRE(dnd::fighting_style(c, "Wizard") == dnd::FightingStyle::None);
    c.level = 1;
    REQUIRE(dnd::fighting_style(c, "Paladin") == dnd::FightingStyle::None);  // Paladins choose at level 2
    c.level = 2;
    REQUIRE(dnd::fighting_style(c, "Paladin") == dnd::FightingStyle::GreatWeaponFighting);
    REQUIRE(dnd::fighting_style(c, "Ranger") == dnd::FightingStyle::None);  // not a Ranger style
    REQUIRE(dnd::fighting_style_level("Ranger") == 2);
    REQUIRE(dnd::fighting_styles_for("Ranger").size() == 3);
    c.fighting_style = "Nonsense";
    REQUIRE(dnd::fighting_style(c, "Fighter") == dnd::FightingStyle::None);
    REQUIRE(std::string(dnd::fighting_style_info(dnd::FightingStyle::Defense)->name) == "Defense");
    REQUIRE(dnd::fighting_style_info(dnd::FightingStyle::None) == nullptr);
}

TEST_CASE("Arcane Recovery recovers slots up to the budget, highest first", "[class_features]") {
    dnd::Character wizard;
    wizard.level = 5;  // budget 3
    wizard.spell_slots_used = {3, 2, 1, 0, 0, 0, 0, 0, 0};

    auto recovered = dnd::use_arcane_recovery(wizard, "Wizard");
    REQUIRE(recovered == std::array<int, 9>{0, 0, 1, 0, 0, 0, 0, 0, 0});  // one 3rd-level slot uses the whole budget
    REQUIRE(wizard.spell_slots_used == std::array<int, 9>{3, 2, 0, 0, 0, 0, 0, 0, 0});

    // Once per day.
    REQUIRE(dnd::use_arcane_recovery(wizard, "Wizard") == std::array<int, 9>{});

    dnd::Character apprentice;
    apprentice.level = 3;  // budget 2
    apprentice.spell_slots_used = {2, 0, 0, 0, 0, 0, 0, 0, 0};
    REQUIRE(dnd::use_arcane_recovery(apprentice, "Wizard") == std::array<int, 9>{2, 0, 0, 0, 0, 0, 0, 0, 0});

    dnd::Character rested;
    rested.level = 3;
    REQUIRE(dnd::use_arcane_recovery(rested, "Wizard") == std::array<int, 9>{});  // nothing spent: use kept
    REQUIRE(dnd::feature_uses_left(dnd::Feature::ArcaneRecovery, rested, "Wizard") == 1);
    REQUIRE(dnd::use_arcane_recovery(apprentice, "Fighter") == std::array<int, 9>{});
}
