#pragma once

#include <string>
#include <vector>

#include "dnd/effects.hpp"
#include "dnd/rules.hpp"

namespace dnd {

// How a spell's effect is resolved -- the engine's fixed vocabulary (see
// docs/magic-system.md). A spell that fits none of these isn't seeded yet.
enum class SpellResolution {
    Attack,   // spell attack roll vs AC, via resolve_attack
    Save,     // each target saves vs the caster's spell save DC
    AutoHit,  // every projectile hits (Magic Missile)
    Heal,     // restores HP to a friendly target
    Utility,    // no automated effect yet: casting just spends the slot and shows the description
    Buff,       // no roll: applies its effects to allies or the caster (Bless, Shield of Faith)
    Stabilize,  // a dying creature becomes stable (Spare the Dying)
    Revive,     // a character who died in this fight returns with 1 HP (Revivify)
};

// When a spell's effect lands (SpellEffects.Trigger).
enum class SpellEffectTrigger {
    OnHit,         // Attack spells, on each target hit
    OnFailedSave,  // Save spells, on each target that fails
    OnCastTarget,  // on each chosen target when cast (Buff spells)
    OnCastSelf,    // on the caster when cast
};

// How long a spell's effect lasts, relative to the caster or target
// (SpellEffects.Duration). Turned into an EffectDuration when applied.
enum class SpellEffectDuration {
    UntilRemoved,
    Rounds,                  // `rounds`, counted on the caster's turns (1 minute = 10)
    UntilStartOfCasterTurn,  // Ray of Frost, Chill Touch
    UntilEndOfCasterTurn,    // Guiding Bolt: through the end of the caster's next turn
    UntilStartOfTargetTurn,
    UntilEndOfTargetTurn,    // Vicious Mockery: through the end of the target's next turn
};

// One effect a spell applies (a SpellEffects row).
struct SpellEffectSpec {
    SpellEffectTrigger trigger = SpellEffectTrigger::OnHit;
    EffectKind kind = EffectKind::Condition;
    Condition condition = Condition::Prone;  // for EffectKind::Condition
    int amount = 0;
    SpellEffectDuration duration = SpellEffectDuration::UntilRemoved;
    int rounds = 0;
    bool until_used = false;
    std::string label;
    // The target repeats the spell's save against the caster's DC at the end
    // of each of its turns; success ends it (Hold Person).
    bool repeat_save = false;
};

enum class CastingTime { Action, BonusAction, Reaction };

// How a cantrip grows at levels 5, 11, and 17.
enum class CantripScaling {
    Dice,         // one more set of the base dice per tier (Fire Bolt: 1d10 -> 2d10 ...)
    Projectiles,  // one more projectile per tier (Eldritch Blast beams)
};

// Seeded, read-only spell data. Enums are stored as text in Spells.
struct Spell {
    int spell_id = 0;
    std::string name;
    int level = 0;  // 0 = cantrip
    std::string school;
    std::string description;
    CastingTime casting_time = CastingTime::Action;
    std::string range_text;
    std::string duration_text;
    bool concentration = false;  // stored now, enforced in phase 3

    SpellResolution resolution = SpellResolution::Attack;
    bool melee_attack = false;  // Attack spells only: melee vs ranged
    Ability save_ability = Ability::Dexterity;  // Save spells only
    bool save_halves = false;                   // Save spells: half damage on a success (else none)

    // One effect roll: dice_count d dice_sides + flat_bonus, plus the
    // caster's spellcasting modifier if adds_modifier. Per projectile.
    int dice_count = 1;
    int dice_sides = 4;
    int flat_bonus = 0;
    bool adds_modifier = false;
    std::string damage_type;  // empty for healing

    int projectiles = 1;  // separate rolls per cast (Magic Missile darts, Eldritch Blast beams)
    int max_targets = 1;  // creatures one cast can affect (area spells: see docs)
    int upcast_dice = 0;         // extra dice per slot level above `level`
    int upcast_projectiles = 0;  // extra projectiles per slot level above `level`
    int upcast_targets = 0;      // extra targets per slot level above `level` (Bless)
    CantripScaling cantrip_scaling = CantripScaling::Dice;
    // Save spells: targets save with advantage in combat ("if you or your
    // companions are fighting it", Charm Person).
    bool combat_save_advantage = false;
    // Only creatures of this type can be targeted ("Humanoid" for Charm
    // Person); empty for any.
    std::string target_creature_type;
    // An item the spell uses up, which the caster must carry ("Diamonds
    // (300 gp)" for Revivify); empty for none. Scrolls don't need it (DMG).
    std::string consumed_item;
    std::vector<SpellEffectSpec> effects;  // SpellEffects rows

    std::vector<std::string> class_names;  // classes whose list includes it (ClassSpells)

    // From Repository::get_character_spells only: ready to cast. Always true
    // except for a Wizard's spellbook spells it hasn't prepared.
    bool prepared = true;
};

}  // namespace dnd
