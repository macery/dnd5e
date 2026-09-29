#pragma once

#include <string>
#include <vector>

#include "dnd/rules.hpp"

namespace dnd {

// How a spell's effect is resolved -- the engine's fixed vocabulary (see
// docs/magic-system.md). A spell that fits none of these isn't seeded yet.
enum class SpellResolution {
    Attack,   // spell attack roll vs AC, via resolve_attack
    Save,     // each target saves vs the caster's spell save DC
    AutoHit,  // every projectile hits (Magic Missile)
    Heal,     // restores HP to a friendly target
    Utility,  // no automated effect yet: casting just spends the slot and shows the description
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
    CantripScaling cantrip_scaling = CantripScaling::Dice;

    std::vector<std::string> class_names;  // classes whose list includes it (ClassSpells)
};

}  // namespace dnd
