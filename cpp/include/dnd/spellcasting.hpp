#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "dnd/character.hpp"
#include "dnd/character_creation.hpp"
#include "dnd/dice.hpp"
#include "dnd/spell.hpp"

namespace dnd {

constexpr int kMaxSpellLevel = 9;

const char* spell_resolution_name(SpellResolution resolution);
SpellResolution spell_resolution_from_name(const std::string& name);
const char* casting_time_name(CastingTime casting_time);
CastingTime casting_time_from_name(const std::string& name);
const char* spell_effect_trigger_name(SpellEffectTrigger trigger);
SpellEffectTrigger spell_effect_trigger_from_name(const std::string& name);
const char* spell_effect_duration_name(SpellEffectDuration duration);
SpellEffectDuration spell_effect_duration_from_name(const std::string& name);
const char* cantrip_scaling_name(CantripScaling scaling);
CantripScaling cantrip_scaling_from_name(const std::string& name);
// Inverse of ability_name. All *_from_name throw std::invalid_argument on
// unknown text.
Ability ability_from_name(const std::string& name);

bool is_spellcaster(const ClassInfo& class_info);

// "1st", "2nd", "3rd", "4th", ... -- for spell and slot levels.
std::string ordinal(int number);

// --- Caster stats ----------------------------------------------------------

int spellcasting_modifier(const Character& character, const ClassInfo& class_info);
// 8 + proficiency bonus + spellcasting modifier.
int spell_save_dc(const Character& character, const ClassInfo& class_info);
// Proficiency bonus + spellcasting modifier.
int spell_attack_bonus(const Character& character, const ClassInfo& class_info);

// --- Spell counts ----------------------------------------------------------

// Cantrips known: base at level 1, +1 at level 4, +1 at level 10. 0 for
// classes without cantrips (Paladin, Ranger, non-casters).
int cantrips_known(const ClassInfo& class_info, int level);
// Levelled spells known, for SpellPreparation::Known classes (PHB class
// tables); 0 for everyone else.
int spells_known(const ClassInfo& class_info, int level);
// Wizard spellbook size from leveling alone: 6 at level 1, +2 per level.
int spellbook_size(int level);
// Spells prepared for Prepared/Spellbook classes: spellcasting modifier +
// level (Paladin: + half level), minimum 1. 0 for other classes, and for a
// Paladin before level 2.
int prepared_spell_count(const Character& character, const ClassInfo& class_info);

// How many spells a character may have, and of what level. For Known
// classes `spells` is spells known; for Prepared classes it's spells
// prepared; for a Wizard it's the spellbook's size (6 + 2 per level), and
// prepared_spell_count says how many of those are prepared
// (docs/preparing-spells.md).
struct SpellAllowance {
    int cantrips = 0;
    int spells = 0;
    int max_spell_level = 0;  // highest slot level the character has; 0 = none
};

SpellAllowance spell_allowance(const Character& character, const ClassInfo& class_info);

// --- Preparing (docs/preparing-spells.md) -------------------------------------

// Clerics, Druids, Paladins, and Wizards choose prepared spells.
bool prepares_spells(const ClassInfo& class_info);
// Why the character can't change prepared spells now, or "" if they can:
// only after a long rest, until their next fight.
std::string preparation_refusal(const Character& character, const ClassInfo& class_info);
// Leveled spells marked prepared in a character's list (cantrips aside).
int prepared_leveled_count(const std::vector<Spell>& known);
// Whether a spell newly added to a Wizard's spellbook should start prepared:
// a leveled spell while there's room under prepared_spell_count. Always true
// for cantrips and for every other class (their list is what's ready).
bool starts_prepared(const Character& character, const ClassInfo& class_info, const std::vector<Spell>& known,
                     const Spell& spell);

// Checks a set of chosen spells against the class list and the allowance:
// every spell on the class's list, no duplicates, no spell above
// max_spell_level, and no more cantrips/spells than allowed. With
// `require_full`, the counts must be exactly the allowance (character
// creation). Returns an error for the player, or nullopt.
std::optional<std::string> validate_spell_selection(const Character& character, const ClassInfo& class_info,
                                                    const std::vector<Spell>& chosen, bool require_full);

// --- Slots -----------------------------------------------------------------

// Total slots per slot level (index = slot level - 1) for a single-class
// character. Full casters use the PHB table; half casters use the full
// table at half their level (rounded up) from level 2; Warlocks have 1-4
// pact slots, all of one level (1st at level 1 up to 5th at level 9+).
std::array<int, kMaxSpellLevel> spell_slots(const ClassInfo& class_info, int level);

int slots_remaining(const Character& character, const ClassInfo& class_info, int slot_level);
// Spends one slot of `slot_level`. Returns false (changing nothing) if
// none remain.
bool spend_spell_slot(Character& character, const ClassInfo& class_info, int slot_level);

// 1 at levels 1-4, 2 at 5-10, 3 at 11-16, 4 at 17+.
int cantrip_tier(int level);

// --- Casting ---------------------------------------------------------------

struct CasterStats {
    int level = 1;
    int spell_attack_bonus = 0;
    int spell_save_dc = 10;
    int spellcasting_modifier = 0;
};

CasterStats caster_stats(const Character& character, const ClassInfo& class_info);

// What cast_spell needs to know about each chosen target. Heal targets
// only need to exist; the caller applies healing and caps it at max HP.
struct SpellTarget {
    int armor_class = 10;
    int save_bonus = 0;  // for the spell's save ability

    // Attack spells: the attack roll against this target (see dnd/effects.hpp).
    RollMode attack_mode = RollMode::Normal;
    bool critical_on_hit = false;
    std::vector<int> attack_bonus_dice;  // sides of dice added to the attack roll (Bless)

    // Save spells: this target's saving throw.
    RollMode save_mode = RollMode::Normal;
    bool save_auto_fail = false;
    std::vector<int> save_bonus_dice;  // sides of dice added to the save (Bless)
};

// One projectile's (or, for a Save spell, one target's) outcome.
struct SpellEffectRoll {
    int target = 0;         // index into the targets passed to cast_spell
    int attack_roll = 0;    // Attack: the d20 (natural)
    bool hit = false;       // Attack: hit; AutoHit/Heal: always true
    bool critical = false;  // Attack: natural 20 (dice doubled)
    int save_roll = 0;      // Save: the d20 (natural)
    bool saved = false;     // Save: met the DC
    bool auto_failed = false;      // Save: failed automatically (e.g. stunned vs a DEX save)
    std::vector<int> bonus_rolls;  // extra dice added to the attack or save (Bless)
    int amount = 0;         // damage dealt, or HP restored for Heal
};

struct SpellCastResult {
    int slot_level = 0;  // 0 for a cantrip
    int projectiles = 1;
    std::vector<int> dice;  // Save spells: the one damage roll every target shares
    std::vector<SpellEffectRoll> effects;
};

// Projectiles for this cast after cantrip scaling / upcasting.
int spell_projectiles(const Spell& spell, int caster_level, int slot_level);
// Dice count per projectile after cantrip scaling / upcasting.
int spell_dice_count(const Spell& spell, int caster_level, int slot_level);
// Most targets one cast can affect: its projectile count for multi-
// projectile spells, otherwise spell.max_targets plus upcast_targets per
// slot level above the spell's.
int spell_max_targets(const Spell& spell, int caster_level, int slot_level);

// A one-line, player-facing summary of what a spell does, e.g.
// "ranged spell attack, 1d10 fire", "DEX save (half on success), 3d6 fire",
// "buff: Blessed (up to 3 allies)", "WIS save; on a failure: Charmed".
std::string spell_summary(const Spell& spell);

// True for spells that pick no targets: Utility spells, and Buff spells
// whose effects all land on the caster (Divine Favor).
bool spell_targets_nobody(const Spell& spell);

// Resolves a cast without touching any character or monster: the caller
// applies damage/healing and spends the slot. Rules:
//  - Attack: one resolve_attack per projectile (crits double dice).
//  - Save: damage is rolled once and shared; each target saves separately
//    (d20 + save_bonus >= DC), taking half (rounded down) or none on a success.
//  - AutoHit / Heal: every projectile lands.
//  - Utility (and self-only Buff): nothing is rolled; `targets` is ignored
//    and effects is empty.
//  - Buff / Stabilize: nothing is rolled; one effect entry (hit = true) per
//    target, for the caller to apply the spell's effects to.
// Projectiles are shared round-robin across `targets`.
// Throws std::invalid_argument if slot_level is invalid (a cantrip must use
// 0; a levelled spell needs slot_level in [spell.level, 9]) or the target
// count is outside [1, spell_max_targets].
SpellCastResult cast_spell(const Dice& dice, const Spell& spell, const CasterStats& caster, int slot_level,
                           const std::vector<SpellTarget>& targets);

}  // namespace dnd
