#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <vector>

#include "dnd/effects.hpp"

namespace {

using dnd::ActiveEffect;
using dnd::Condition;
using dnd::EffectKind;
using dnd::RollMode;

class ScriptedRandomSource : public dnd::IRandomSource {
public:
    explicit ScriptedRandomSource(std::vector<int> values) : values_(std::move(values)) {}
    int roll_die(int /*sides*/) override { return values_[index_++ % values_.size()]; }

private:
    std::vector<int> values_;
    size_t index_ = 0;
};

ActiveEffect condition(int target, Condition c, int source = -1) {
    ActiveEffect e;
    e.kind = EffectKind::Condition;
    e.condition = c;
    e.target = target;
    e.source = source;
    return e;
}

ActiveEffect effect(int id, int target, EffectKind kind, int amount = 0, bool until_used = false) {
    ActiveEffect e;
    e.id = id;
    e.kind = kind;
    e.target = target;
    e.amount = amount;
    e.duration.until_used = until_used;
    return e;
}

const std::vector<bool> kAllAlive(4, true);

}  // namespace

TEST_CASE("every condition has a name and description", "[effects]") {
    for (int i = 0; i < dnd::kConditionCount; ++i) {
        auto c = static_cast<Condition>(i);
        REQUIRE(std::string(dnd::condition_name(c)).size() > 0);
        REQUIRE(std::string(dnd::condition_description(c)).size() > 0);
    }
    REQUIRE(std::string(dnd::condition_name(Condition::Unconscious)) == "Unconscious");
}

TEST_CASE("implied conditions", "[effects]") {
    std::vector<ActiveEffect> effects = {condition(0, Condition::Unconscious), condition(1, Condition::Stunned)};
    REQUIRE(dnd::has_condition(effects, 0, Condition::Prone));
    REQUIRE(dnd::is_incapacitated(effects, 0));
    REQUIRE(dnd::is_incapacitated(effects, 1));
    REQUIRE_FALSE(dnd::has_condition(effects, 1, Condition::Prone));
    REQUIRE_FALSE(dnd::is_incapacitated(effects, 2));
}

TEST_CASE("attack roll modifiers from the attacker's and target's conditions", "[effects]") {
    SECTION("no effects: a normal roll") {
        auto m = dnd::attack_roll_modifiers({}, 0, 1, true, kAllAlive);
        REQUIRE(m.mode() == RollMode::Normal);
        REQUIRE_FALSE(m.auto_crit);
    }

    SECTION("a poisoned attacker has disadvantage") {
        auto m = dnd::attack_roll_modifiers({condition(0, Condition::Poisoned)}, 0, 1, true, kAllAlive);
        REQUIRE(m.mode() == RollMode::Disadvantage);
        REQUIRE(m.disadvantage == std::vector<std::string>{"poisoned"});
    }

    SECTION("prone targets: advantage in melee, disadvantage at range") {
        std::vector<ActiveEffect> effects = {condition(1, Condition::Prone)};
        REQUIRE(dnd::attack_roll_modifiers(effects, 0, 1, true, kAllAlive).mode() == RollMode::Advantage);
        REQUIRE(dnd::attack_roll_modifiers(effects, 0, 1, false, kAllAlive).mode() == RollMode::Disadvantage);
    }

    SECTION("advantage and disadvantage cancel, however many of each") {
        std::vector<ActiveEffect> effects = {condition(0, Condition::Poisoned), condition(0, Condition::Prone),
                                             condition(1, Condition::Stunned)};
        auto m = dnd::attack_roll_modifiers(effects, 0, 1, true, kAllAlive);
        REQUIRE(m.disadvantage.size() == 2);
        REQUIRE(m.advantage.size() == 1);
        REQUIRE(m.mode() == RollMode::Normal);
    }

    SECTION("melee hits on paralyzed or unconscious targets are critical") {
        std::vector<ActiveEffect> effects = {condition(1, Condition::Unconscious)};
        auto melee = dnd::attack_roll_modifiers(effects, 0, 1, true, kAllAlive);
        REQUIRE(melee.auto_crit);
        REQUIRE(melee.mode() == RollMode::Advantage);  // unconscious + prone, both favor melee
        auto ranged = dnd::attack_roll_modifiers(effects, 0, 1, false, kAllAlive);
        REQUIRE_FALSE(ranged.auto_crit);
        REQUIRE(ranged.mode() == RollMode::Normal);  // unconscious helps, prone hurts at range
    }

    SECTION("dodging gives disadvantage unless the dodger is incapacitated") {
        std::vector<ActiveEffect> effects = {effect(1, 1, EffectKind::Dodging)};
        REQUIRE(dnd::attack_roll_modifiers(effects, 0, 1, true, kAllAlive).mode() == RollMode::Disadvantage);
        effects.push_back(condition(1, Condition::Stunned));
        REQUIRE(dnd::attack_roll_modifiers(effects, 0, 1, true, kAllAlive).mode() == RollMode::Advantage);
    }

    SECTION("frightened only matters while the source is alive") {
        std::vector<ActiveEffect> effects = {condition(0, Condition::Frightened, 2)};
        REQUIRE(dnd::attack_roll_modifiers(effects, 0, 1, true, kAllAlive).mode() == RollMode::Disadvantage);
        std::vector<bool> source_down = {true, true, false, true};
        REQUIRE(dnd::attack_roll_modifiers(effects, 0, 1, true, source_down).mode() == RollMode::Normal);
    }

    SECTION("one-use effects are reported as used; bonus dice are collected") {
        std::vector<ActiveEffect> effects = {effect(7, 1, EffectKind::AdvantageOnNextAttackAgainst, 0, true),
                                             effect(8, 0, EffectKind::AttackBonusDice, 4),
                                             effect(9, 1, EffectKind::AdvantageOnAttacksAgainst)};
        auto m = dnd::attack_roll_modifiers(effects, 0, 1, false, kAllAlive);
        REQUIRE(m.mode() == RollMode::Advantage);
        REQUIRE(m.used_effects == std::vector<int>{7});
        REQUIRE(m.bonus_dice == std::vector<int>{4});
    }
}

TEST_CASE("saving throw modifiers", "[effects]") {
    using dnd::Ability;
    std::vector<ActiveEffect> effects = {condition(0, Condition::Paralyzed), condition(1, Condition::Restrained),
                                         effect(1, 2, EffectKind::Dodging), effect(2, 3, EffectKind::SaveBonusDice, 4)};

    REQUIRE(dnd::saving_throw_modifiers(effects, 0, Ability::Dexterity).auto_fail);
    REQUIRE(dnd::saving_throw_modifiers(effects, 0, Ability::Strength).auto_fail);
    REQUIRE_FALSE(dnd::saving_throw_modifiers(effects, 0, Ability::Wisdom).auto_fail);
    REQUIRE(dnd::saving_throw_modifiers(effects, 1, Ability::Dexterity).mode() == RollMode::Disadvantage);
    REQUIRE(dnd::saving_throw_modifiers(effects, 1, Ability::Strength).mode() == RollMode::Normal);
    REQUIRE(dnd::saving_throw_modifiers(effects, 2, Ability::Dexterity).mode() == RollMode::Advantage);
    REQUIRE(dnd::saving_throw_modifiers(effects, 3, Ability::Wisdom).bonus_dice == std::vector<int>{4});
}

TEST_CASE("armor class bonuses add up", "[effects]") {
    std::vector<ActiveEffect> effects = {effect(1, 0, EffectKind::ArmorClassBonus, 2),
                                         effect(2, 0, EffectKind::ArmorClassBonus, 1),
                                         effect(3, 1, EffectKind::ArmorClassBonus, 5)};
    REQUIRE(dnd::armor_class_bonus(effects, 0) == 3);
    REQUIRE(dnd::armor_class_bonus(effects, 2) == 0);
}

TEST_CASE("death saving throws", "[effects]") {
    dnd::DeathSaveState state;

    SECTION("three successes: stable") {
        ScriptedRandomSource rolls({10, 15, 19});
        dnd::Dice dice(rolls);
        REQUIRE_FALSE(dnd::roll_death_save(dice, state).became_stable);
        REQUIRE_FALSE(dnd::roll_death_save(dice, state).became_stable);
        REQUIRE(dnd::roll_death_save(dice, state).became_stable);
        REQUIRE(state.stable);
        REQUIRE_FALSE(state.dead);
    }

    SECTION("three failures: dead; a natural 1 counts twice") {
        ScriptedRandomSource rolls({9, 1});
        dnd::Dice dice(rolls);
        REQUIRE(state.failures == 0);
        dnd::roll_death_save(dice, state);
        REQUIRE(state.failures == 1);
        auto result = dnd::roll_death_save(dice, state);
        REQUIRE(result.died);
        REQUIRE(state.dead);
    }

    SECTION("a natural 20 regains 1 HP and resets") {
        ScriptedRandomSource rolls({5, 20});
        dnd::Dice dice(rolls);
        dnd::roll_death_save(dice, state);
        auto result = dnd::roll_death_save(dice, state);
        REQUIRE(result.regained_hp);
        REQUIRE(state.failures == 0);
        REQUIRE(state.successes == 0);
    }

    SECTION("damage at 0 HP: one failure, two on a crit, death at max HP") {
        REQUIRE_FALSE(dnd::damage_at_zero_hp(state, 3, false, 12));
        REQUIRE(state.failures == 1);
        REQUIRE(dnd::damage_at_zero_hp(state, 3, true, 12));
        REQUIRE(state.dead);

        dnd::DeathSaveState fresh;
        REQUIRE(dnd::damage_at_zero_hp(fresh, 12, false, 12));  // massive damage
    }

    SECTION("damage makes a stable character start dying again") {
        state.stable = true;
        REQUIRE_FALSE(dnd::damage_at_zero_hp(state, 2, false, 12));
        REQUIRE_FALSE(state.stable);
        REQUIRE(state.failures == 1);
    }

    SECTION("massive damage when dropping to 0") {
        REQUIRE(dnd::is_massive_damage(5, 17, 12));        // 12 left over
        REQUIRE_FALSE(dnd::is_massive_damage(5, 16, 12));  // 11 left over
    }
}

TEST_CASE("concentration save DC", "[effects]") {
    REQUIRE(dnd::concentration_save_dc(1) == 10);
    REQUIRE(dnd::concentration_save_dc(21) == 10);
    REQUIRE(dnd::concentration_save_dc(22) == 11);
    REQUIRE(dnd::concentration_save_dc(40) == 20);
}

TEST_CASE("condition and effect kind names round-trip", "[effects]") {
    for (int i = 0; i < dnd::kConditionCount; ++i) {
        auto c = static_cast<Condition>(i);
        REQUIRE(dnd::condition_from_name(dnd::condition_name(c)) == c);
    }
    for (auto kind : {EffectKind::Condition, EffectKind::Dodging, EffectKind::AttackBonusDice,
                      EffectKind::SaveBonusDice, EffectKind::ArmorClassBonus, EffectKind::ExtraWeaponDamageDice,
                      EffectKind::AdvantageOnNextAttackAgainst, EffectKind::AdvantageOnAttacksAgainst,
                      EffectKind::DisadvantageOnNextAttack, EffectKind::CantRegainHitPoints,
                      EffectKind::SpeedReduction, EffectKind::ArmorNotProficient}) {
        REQUIRE(dnd::effect_kind_from_name(dnd::effect_kind_name(kind)) == kind);
    }
    REQUIRE_THROWS_AS(dnd::condition_from_name("Sleepy"), std::invalid_argument);
    REQUIRE_THROWS_AS(dnd::effect_kind_from_name("Flying"), std::invalid_argument);
}

TEST_CASE("ability check modifiers", "[effects]") {
    std::vector<ActiveEffect> effects = {condition(0, Condition::Poisoned), condition(1, Condition::Frightened, 2)};
    REQUIRE(dnd::ability_check_modifiers(effects, 0, kAllAlive).mode() == RollMode::Disadvantage);
    REQUIRE(dnd::ability_check_modifiers(effects, 1, kAllAlive).mode() == RollMode::Disadvantage);
    std::vector<bool> source_down = {true, true, false, true};
    REQUIRE(dnd::ability_check_modifiers(effects, 1, source_down).mode() == RollMode::Normal);
    REQUIRE(dnd::ability_check_modifiers(effects, 3, kAllAlive).mode() == RollMode::Normal);
}

TEST_CASE("effect descriptions", "[effects]") {
    REQUIRE(dnd::effect_description(effect(1, 0, EffectKind::AttackBonusDice, 4)) == "Adds 1d4 to its attack rolls.");
    REQUIRE(dnd::effect_description(effect(1, 0, EffectKind::ArmorClassBonus, 2)) == "+2 AC.");
    REQUIRE(dnd::effect_description(condition(0, Condition::Stunned)) ==
            dnd::condition_description(Condition::Stunned));
    for (auto kind : {EffectKind::Dodging, EffectKind::SaveBonusDice, EffectKind::ExtraWeaponDamageDice,
                      EffectKind::AdvantageOnNextAttackAgainst, EffectKind::AdvantageOnAttacksAgainst,
                      EffectKind::DisadvantageOnNextAttack, EffectKind::CantRegainHitPoints,
                      EffectKind::SpeedReduction}) {
        REQUIRE_FALSE(dnd::effect_description(effect(1, 0, kind, 4)).empty());
    }
}

TEST_CASE("armor without proficiency hinders STR and DEX", "[effects]") {
    std::vector<ActiveEffect> effects = {effect(1, 0, EffectKind::ArmorNotProficient)};
    auto attack = dnd::attack_roll_modifiers(effects, 0, 1, true, kAllAlive);
    REQUIRE(attack.mode() == RollMode::Disadvantage);
    REQUIRE(attack.disadvantage == std::vector<std::string>{"armor not proficient"});
    REQUIRE(dnd::saving_throw_modifiers(effects, 0, dnd::Ability::Dexterity).mode() == RollMode::Disadvantage);
    REQUIRE(dnd::saving_throw_modifiers(effects, 0, dnd::Ability::Strength).mode() == RollMode::Disadvantage);
    REQUIRE(dnd::saving_throw_modifiers(effects, 0, dnd::Ability::Wisdom).mode() == RollMode::Normal);
    REQUIRE_FALSE(dnd::effect_description(effects[0]).empty());
}
