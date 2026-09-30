#include "dnd/effects.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace dnd {

namespace {

constexpr std::array<const char*, kConditionCount> kConditionNames = {
    "Blinded",   "Charmed",   "Deafened", "Frightened", "Grappled",   "Incapacitated", "Invisible",
    "Paralyzed", "Petrified", "Poisoned", "Prone",      "Restrained", "Stunned",       "Unconscious",
};

constexpr std::array<const char*, kConditionCount> kConditionDescriptions = {
    "Can't see. Its attacks have disadvantage; attacks against it have advantage.",
    "Can't attack the charmer or target it with harmful spells.",
    "Can't hear. (No effect in combat here yet.)",
    "Disadvantage on attacks and ability checks while the source of its fear is still in the fight.",
    "Its speed is 0. (Speed isn't tracked yet.)",
    "Can't take actions: its turn is skipped.",
    "Can't be seen. Its attacks have advantage; attacks against it have disadvantage.",
    "Incapacitated and can't move. Automatically fails STR and DEX saves. Attacks against it have advantage, "
    "and melee hits are critical.",
    "Turned to stone: incapacitated, automatically fails STR and DEX saves, and attacks against it have "
    "advantage.",
    "Disadvantage on attacks and ability checks.",
    "Its attacks have disadvantage. Melee attacks against it have advantage; ranged attacks have "
    "disadvantage. It stands up at the start of its turn.",
    "Its attacks and DEX saves have disadvantage; attacks against it have advantage.",
    "Incapacitated. Automatically fails STR and DEX saves; attacks against it have advantage.",
    "Incapacitated and prone. Automatically fails STR and DEX saves. Attacks against it have advantage, and "
    "melee hits are critical.",
};

bool has_direct_condition(const std::vector<ActiveEffect>& effects, int target, Condition condition) {
    return std::any_of(effects.begin(), effects.end(), [&](const ActiveEffect& e) {
        return e.target == target && e.kind == EffectKind::Condition && e.condition == condition;
    });
}

void add_used(RollModifiers& modifiers, const ActiveEffect& effect) {
    if (effect.duration.until_used) modifiers.used_effects.push_back(effect.id);
}

}  // namespace

const char* condition_name(Condition condition) {
    return kConditionNames.at(static_cast<size_t>(condition));
}

Condition condition_from_name(const std::string& name) {
    for (int i = 0; i < kConditionCount; ++i) {
        if (name == kConditionNames[static_cast<size_t>(i)]) return static_cast<Condition>(i);
    }
    throw std::invalid_argument("unknown condition: " + name);
}

namespace {
constexpr std::array<std::pair<EffectKind, const char*>, 11> kEffectKindNames = {{
    {EffectKind::Condition, "Condition"},
    {EffectKind::Dodging, "Dodging"},
    {EffectKind::AttackBonusDice, "AttackBonusDice"},
    {EffectKind::SaveBonusDice, "SaveBonusDice"},
    {EffectKind::ArmorClassBonus, "ArmorClassBonus"},
    {EffectKind::ExtraWeaponDamageDice, "ExtraWeaponDamageDice"},
    {EffectKind::AdvantageOnNextAttackAgainst, "AdvantageOnNextAttackAgainst"},
    {EffectKind::AdvantageOnAttacksAgainst, "AdvantageOnAttacksAgainst"},
    {EffectKind::DisadvantageOnNextAttack, "DisadvantageOnNextAttack"},
    {EffectKind::CantRegainHitPoints, "CantRegainHitPoints"},
    {EffectKind::SpeedReduction, "SpeedReduction"},
}};
}  // namespace

const char* effect_kind_name(EffectKind kind) {
    for (const auto& [k, name] : kEffectKindNames) {
        if (k == kind) return name;
    }
    throw std::invalid_argument("unknown effect kind");
}

EffectKind effect_kind_from_name(const std::string& name) {
    for (const auto& [kind, text] : kEffectKindNames) {
        if (name == text) return kind;
    }
    throw std::invalid_argument("unknown effect kind: " + name);
}

const char* condition_description(Condition condition) {
    return kConditionDescriptions.at(static_cast<size_t>(condition));
}

bool has_condition(const std::vector<ActiveEffect>& effects, int target, Condition condition) {
    if (has_direct_condition(effects, target, condition)) return true;
    switch (condition) {
        case Condition::Incapacitated:
            for (auto implies : {Condition::Paralyzed, Condition::Petrified, Condition::Stunned, Condition::Unconscious}) {
                if (has_direct_condition(effects, target, implies)) return true;
            }
            return false;
        case Condition::Prone:
            return has_direct_condition(effects, target, Condition::Unconscious);
        default:
            return false;
    }
}

bool is_incapacitated(const std::vector<ActiveEffect>& effects, int target) {
    return has_condition(effects, target, Condition::Incapacitated);
}

bool has_effect(const std::vector<ActiveEffect>& effects, int target, EffectKind kind) {
    return std::any_of(effects.begin(), effects.end(),
                       [&](const ActiveEffect& e) { return e.target == target && e.kind == kind; });
}

int armor_class_bonus(const std::vector<ActiveEffect>& effects, int target) {
    int bonus = 0;
    for (const auto& effect : effects) {
        if (effect.target == target && effect.kind == EffectKind::ArmorClassBonus) bonus += effect.amount;
    }
    return bonus;
}

RollMode RollModifiers::mode() const {
    bool adv = !advantage.empty();
    bool dis = !disadvantage.empty();
    if (adv == dis) return RollMode::Normal;
    return adv ? RollMode::Advantage : RollMode::Disadvantage;
}

RollModifiers attack_roll_modifiers(const std::vector<ActiveEffect>& effects, int attacker, int target, bool melee,
                                    const std::vector<bool>& alive) {
    RollModifiers m;
    auto is = [&](int who, Condition c) { return has_condition(effects, who, c); };

    // The attacker's own state.
    if (is(attacker, Condition::Blinded)) m.disadvantage.push_back("blinded");
    if (is(attacker, Condition::Invisible)) m.advantage.push_back("invisible");
    if (is(attacker, Condition::Poisoned)) m.disadvantage.push_back("poisoned");
    if (is(attacker, Condition::Prone)) m.disadvantage.push_back("prone");
    if (is(attacker, Condition::Restrained)) m.disadvantage.push_back("restrained");
    for (const auto& effect : effects) {
        if (effect.target != attacker) continue;
        if (effect.kind == EffectKind::Condition && effect.condition == Condition::Frightened) {
            bool source_alive = effect.source < 0 || (effect.source < static_cast<int>(alive.size()) &&
                                                      alive[static_cast<size_t>(effect.source)]);
            if (source_alive) m.disadvantage.push_back("frightened");
        } else if (effect.kind == EffectKind::DisadvantageOnNextAttack) {
            m.disadvantage.push_back(effect.label.empty() ? "hindered" : effect.label);
            add_used(m, effect);
        } else if (effect.kind == EffectKind::AttackBonusDice) {
            m.bonus_dice.push_back(effect.amount);
        }
    }

    // The target's state.
    if (is(target, Condition::Blinded)) m.advantage.push_back("target blinded");
    if (is(target, Condition::Invisible)) m.disadvantage.push_back("target invisible");
    for (auto helpless : {Condition::Paralyzed, Condition::Petrified, Condition::Restrained, Condition::Stunned,
                          Condition::Unconscious}) {
        if (has_direct_condition(effects, target, helpless)) {
            m.advantage.push_back(std::string("target ") + condition_name(helpless));
        }
    }
    if (is(target, Condition::Prone)) {
        (melee ? m.advantage : m.disadvantage).push_back("target prone");
    }
    if (melee && (is(target, Condition::Paralyzed) || is(target, Condition::Unconscious))) m.auto_crit = true;
    for (const auto& effect : effects) {
        if (effect.target != target) continue;
        if (effect.kind == EffectKind::Dodging && !is_incapacitated(effects, target)) {
            m.disadvantage.push_back("target dodging");
        } else if (effect.kind == EffectKind::AdvantageOnNextAttackAgainst ||
                   effect.kind == EffectKind::AdvantageOnAttacksAgainst) {
            m.advantage.push_back(effect.label.empty() ? "target marked" : effect.label);
            add_used(m, effect);
        }
    }

    // Condition names read better lowercase in a log line ("target stunned").
    for (auto* reasons : {&m.advantage, &m.disadvantage}) {
        for (auto& reason : *reasons) {
            for (auto& c : reason) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    return m;
}

RollModifiers saving_throw_modifiers(const std::vector<ActiveEffect>& effects, int target, Ability ability) {
    RollModifiers m;
    bool physical = ability == Ability::Strength || ability == Ability::Dexterity;
    if (physical) {
        for (auto helpless : {Condition::Paralyzed, Condition::Petrified, Condition::Stunned, Condition::Unconscious}) {
            if (has_direct_condition(effects, target, helpless)) m.auto_fail = true;
        }
    }
    if (ability == Ability::Dexterity) {
        if (has_condition(effects, target, Condition::Restrained)) m.disadvantage.push_back("restrained");
        if (has_effect(effects, target, EffectKind::Dodging) && !is_incapacitated(effects, target)) {
            m.advantage.push_back("dodging");
        }
    }
    for (const auto& effect : effects) {
        if (effect.target == target && effect.kind == EffectKind::SaveBonusDice) m.bonus_dice.push_back(effect.amount);
    }
    return m;
}

RollModifiers ability_check_modifiers(const std::vector<ActiveEffect>& effects, int target,
                                      const std::vector<bool>& alive) {
    RollModifiers m;
    if (has_condition(effects, target, Condition::Poisoned)) m.disadvantage.push_back("poisoned");
    for (const auto& effect : effects) {
        if (effect.target != target || effect.kind != EffectKind::Condition ||
            effect.condition != Condition::Frightened) {
            continue;
        }
        bool source_alive = effect.source < 0 || (effect.source < static_cast<int>(alive.size()) &&
                                                  alive[static_cast<size_t>(effect.source)]);
        if (source_alive) m.disadvantage.push_back("frightened");
    }
    return m;
}

// --- Death saving throws -----------------------------------------------------

DeathSaveResult roll_death_save(const Dice& dice, DeathSaveState& state) {
    DeathSaveResult result;
    result.roll = dice.roll(20, 1).front();

    if (result.roll == 20) {
        result.regained_hp = true;
        state = DeathSaveState{};
        return result;
    }
    if (result.roll == 1) {
        state.failures += 2;
    } else if (result.roll >= 10) {
        ++state.successes;
    } else {
        ++state.failures;
    }

    if (state.failures >= 3) {
        state.dead = true;
        result.died = true;
    } else if (state.successes >= 3) {
        state.stable = true;
        state.successes = 0;
        state.failures = 0;
        result.became_stable = true;
    }
    return result;
}

bool damage_at_zero_hp(DeathSaveState& state, int damage, bool critical, int max_hp) {
    if (state.dead) return false;
    if (damage >= max_hp) {
        state.dead = true;
        return true;
    }
    if (state.stable) {
        state = DeathSaveState{};  // no longer stable: dying again
    }
    state.failures += critical ? 2 : 1;
    if (state.failures >= 3) {
        state.dead = true;
        return true;
    }
    return false;
}

bool is_massive_damage(int hp_before, int damage, int max_hp) {
    return damage - hp_before >= max_hp;
}

int concentration_save_dc(int damage) {
    return std::max(10, damage / 2);
}

}  // namespace dnd
