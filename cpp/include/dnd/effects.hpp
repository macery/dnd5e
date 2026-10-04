#pragma once

#include <optional>
#include <string>
#include <vector>

#include "dnd/dice.hpp"
#include "dnd/rules.hpp"

namespace dnd {

// The 2014 PHB conditions (Appendix A). See docs/effects-system.md for what
// each one does in the engine.
enum class Condition {
    Blinded,
    Charmed,
    Deafened,
    Frightened,
    Grappled,
    Incapacitated,
    Invisible,
    Paralyzed,
    Petrified,
    Poisoned,
    Prone,
    Restrained,
    Stunned,
    Unconscious,
};
constexpr int kConditionCount = 14;

const char* condition_name(Condition condition);
// Inverse of condition_name. Throws std::invalid_argument on unknown text.
Condition condition_from_name(const std::string& name);
// A short, beginner-facing summary of the rules, for tooltips.
const char* condition_description(Condition condition);

// What an effect does. `Condition` effects carry which condition; the rest
// are the modifiers spells and monsters apply (docs/effects-system.md).
enum class EffectKind {
    Condition,
    Dodging,                      // attacks against: disadvantage; own DEX saves: advantage
    AttackBonusDice,              // + 1d(amount) to own attack rolls (Bless)
    SaveBonusDice,                // + 1d(amount) to own saving throws (Bless)
    ArmorClassBonus,              // + amount AC (Shield of Faith)
    ExtraWeaponDamageDice,        // + 1d(amount) on own weapon hits (Divine Favor)
    AdvantageOnNextAttackAgainst, // the next attack against it has advantage (Guiding Bolt)
    AdvantageOnAttacksAgainst,    // every attack against it has advantage (Faerie Fire)
    DisadvantageOnNextAttack,     // its next attack has disadvantage (Vicious Mockery)
    CantRegainHitPoints,          // Chill Touch
    SpeedReduction,               // amount feet; shown only (Ray of Frost)
    ArmorNotProficient,           // wearing armor it isn't trained with: disadvantage on STR/DEX
                                  // attacks, checks, and saves, and no spellcasting (PHB ch. 5)
    Raging,                       // Barbarian Rage: advantage on STR saves (the damage bonus and
                                  // resistance are applied by Encounter)
    InspirationDie,               // Bardic Inspiration: + 1d(amount) to the next attack roll or save
    AttackBonusFlat,              // + amount to its weapon attack rolls (Sacred Weapon)
    Turned,                       // Turn Undead: keeps away and doesn't attack; ends if it takes damage
};

// Stable text names for storing effect kinds (SpellEffects.Kind): the
// enumerator's own name, e.g. "AttackBonusDice".
const char* effect_kind_name(EffectKind kind);
EffectKind effect_kind_from_name(const std::string& name);

enum class DurationKind {
    UntilRemoved,      // until something ends it (e.g. Unconscious while dying)
    Rounds,            // `rounds` left, counted down at the start of the source's turn
    UntilStartOfTurn,  // ends when `participant`'s next turn starts
    UntilEndOfTurn,    // ends when `participant`'s next turn ends
};

struct EffectDuration {
    DurationKind kind = DurationKind::UntilRemoved;
    int rounds = 0;
    int participant = -1;
    bool until_used = false;  // also ends the first time it affects a roll
    // UntilEndOfTurn only: the effect was applied during `participant`'s own
    // turn, so it lasts through the end of their *next* turn instead of
    // ending when the current one does (Guiding Bolt: "until the end of your
    // next turn"). Cleared, rather than expiring, at the first turn end.
    bool skip_current_turn = false;
};

// The target repeats this save at the end of each of its turns; success ends the effect.
struct SaveToEnd {
    Ability ability = Ability::Wisdom;
    int dc = 10;
};

struct ActiveEffect {
    int id = 0;  // assigned by whoever tracks effects (Encounter)
    EffectKind kind = EffectKind::Condition;
    Condition condition = Condition::Prone;  // for EffectKind::Condition
    int target = -1;  // combatant index it's on
    int source = -1;  // combatant index that caused it (-1 = none)
    EffectDuration duration;
    int amount = 0;  // dice sides, AC bonus, or feet, depending on kind
    bool concentration = false;  // ends when the source's concentration ends
    std::optional<SaveToEnd> save_to_end;
    std::string label;  // shown in the UI and log, e.g. "Blessed"
};

// What an effect does, in a sentence for tooltips: the condition's
// description, or e.g. "Adds 1d4 to its attack rolls." for AttackBonusDice.
std::string effect_description(const ActiveEffect& effect);

// Does `target` have `condition`, directly or implied? Paralyzed, Petrified,
// Stunned, and Unconscious imply Incapacitated; Unconscious implies Prone.
bool has_condition(const std::vector<ActiveEffect>& effects, int target, Condition condition);
bool is_incapacitated(const std::vector<ActiveEffect>& effects, int target);
bool has_effect(const std::vector<ActiveEffect>& effects, int target, EffectKind kind);
int armor_class_bonus(const std::vector<ActiveEffect>& effects, int target);

// Everything effects contribute to one d20 roll. Advantage and disadvantage
// cancel out regardless of how many of each there are (PHB ch. 7).
struct RollModifiers {
    std::vector<std::string> advantage;     // reasons, for the log
    std::vector<std::string> disadvantage;  // reasons, for the log
    bool auto_fail = false;  // saves: STR/DEX while paralyzed, stunned, petrified, unconscious
    bool auto_crit = false;  // attacks: a melee hit on a paralyzed or unconscious target
    std::vector<int> bonus_dice;  // sides of extra dice added to the roll (Bless)
    std::vector<int> used_effects;  // ids of "until used" effects this roll uses up

    RollMode mode() const;
};

// `effects` are every effect in the encounter; `alive[i]` tells Frightened
// whether combatant i -- the source of the fear -- is still in the fight.
RollModifiers attack_roll_modifiers(const std::vector<ActiveEffect>& effects, int attacker, int target, bool melee,
                                    const std::vector<bool>& alive);
RollModifiers saving_throw_modifiers(const std::vector<ActiveEffect>& effects, int target, Ability ability);
// Ability checks (e.g. a Medicine check to stabilize): disadvantage while
// poisoned or frightened (source alive).
RollModifiers ability_check_modifiers(const std::vector<ActiveEffect>& effects, int target,
                                      const std::vector<bool>& alive);

// --- Death saving throws (PHB ch. 9) -----------------------------------------

struct DeathSaveState {
    int successes = 0;
    int failures = 0;
    bool stable = false;
    bool dead = false;
};

struct DeathSaveResult {
    int roll = 0;
    bool regained_hp = false;  // natural 20: back to 1 HP, conscious
    bool became_stable = false;
    bool died = false;
};

// Rolls one death save and applies it: 10+ is a success, below is a failure,
// a natural 1 counts as two failures, a natural 20 regains 1 HP (and resets
// the state). Three successes: stable. Three failures: dead. `modifier`
// (a *raise dead* penalty) applies to the 10+ test, not to natural 1s and 20s.
DeathSaveResult roll_death_save(const Dice& dice, DeathSaveState& state, int modifier = 0);

// Damage while at 0 HP: one failure, two for a critical hit. Damage of at
// least `max_hp` kills outright. Returns true if this killed the character.
// A stable character hit this way starts dying again.
bool damage_at_zero_hp(DeathSaveState& state, int damage, bool critical, int max_hp);

// Massive damage (PHB ch. 9): the damage left over after dropping to 0 HP is
// at least the character's max HP.
bool is_massive_damage(int hp_before, int damage, int max_hp);

// --- Concentration (PHB ch. 10) ----------------------------------------------

// DC of the CON save to keep concentrating after taking `damage`: 10 or half
// the damage, whichever is higher.
int concentration_save_dc(int damage);

}  // namespace dnd
