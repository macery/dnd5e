# 2nd- and 3rd-level spells

Status: **done.** As with campaigns, the questions were settled with the recommended answers without a review round (2026-10-02). They're open to revisiting.

**As built:**
- **Migration 25:**
  - `Spells.ConsumedItem` and `SpellEffects.RepeatSave`.
  - The 8 automated spells and 28 Utility spells, with class lists.
  - Hold Person's and Blindness/Deafness's effects.
  - **Diamonds (300 gp)** (Gear, 300 gp, in the Shop).
  - Scrolls of the 8 new automated spells.
- **Engine:**
  - Repeat saves through `SaveToEnd`, with the caster's DC.
  - Heal spells roll per target.
  - A `Revive` resolution: it targets characters with `died_this_fight` (set when they die) and calls `Encounter::revive`.
  - `cast_spell` refuses a spell whose `consumed_item` the caster doesn't carry, and uses one up (ItemUsed) when it's cast. Scrolls skip the component.
- **Tests:** `tests/test_spells_2_3.cpp` (seeding, Hold Person, Mass Healing Word, Fireball, Revivify with and without diamonds, too late), and a combat UI test casting Fireball at three orcs.

## Where things stand

- **Only cantrips and 1st-level spells exist:** 18 cantrips and 24 first-level spells (16 automated, the rest Utility).
- **Casters outgrow the list.** From level 3, full casters get 2nd-level slots with no 2nd-level spells to learn; at level 5, 3rd-level slots. Paladins and Rangers reach 2nd-level slots at level 5. Level Up and the Spellbook can only offer 1st-level spells, and higher slots only upcast them.
- **Spell scrolls** exist only for levels 0–1, because only those spells are automated.

## Goals

- **Automated spells that fit what combat already does** (attacks, saves, conditions, healing):

  | Level | Spell | Classes | What it does here |
  |---|---|---|---|
  | 2nd | *Scorching Ray* | Sorcerer, Wizard | Three ranged spell attacks, 2d6 fire each, at up to 3 targets; +1 ray per slot level above 2nd |
  | 2nd | *Shatter* | Bard, Sorcerer, Warlock, Wizard | CON save, 3d8 thunder, half on a success; up to 4 creatures (a 10-foot sphere) |
  | 2nd | *Hold Person* | Bard, Cleric, Druid, Sorcerer, Warlock, Wizard | WIS save or **paralyzed** (humanoids); concentration, up to 1 minute; a new save at the end of each of its turns; +1 target per slot level |
  | 2nd | *Blindness/Deafness* | Bard, Cleric, Sorcerer, Wizard | CON save or **blinded**; 1 minute, no concentration; a new save at the end of each of its turns; +1 target per slot level |
  | 3rd | *Fireball* | Sorcerer, Wizard | DEX save, 8d6 fire, half on a success; up to 6 creatures (a 20-foot sphere) |
  | 3rd | *Lightning Bolt* | Sorcerer, Wizard | DEX save, 8d6 lightning, half on a success; up to 4 creatures (a 100-foot line) |
  | 3rd | *Mass Healing Word* | Cleric | Bonus action: up to 6 creatures regain 1d4 + modifier each |
  | 3rd | *Revivify* | Cleric, Paladin | A creature that died **in this fight** (the PHB's "within the last minute") returns with 1 HP; uses up **Diamonds (300 gp)**, a new Shop item |

- **Utility spells (no automated effect yet) to fill every class's picks:**
  - 2nd: *Aid*, *Barkskin*, *Darkvision*, *Enhance Ability*, *Invisibility*, *Lesser Restoration*, *Magic Weapon*, *Mirror Image*, *Misty Step*, *Moonbeam*, *Pass without Trace*, *Silence*, *Spike Growth*, *Spiritual Weapon*, *Suggestion*.
  - 3rd: *Beacon of Hope*, *Call Lightning*, *Conjure Animals*, *Crusader's Mantle*, *Daylight*, *Dispel Magic*, *Fly*, *Haste*, *Hunger of Hadar*, *Hypnotic Pattern*, *Remove Curse*, *Spirit Guardians*, *Vampiric Touch*.
- **Spell scrolls** of the new automated spells: 2nd level 150 gp, 3rd level 300 gp (uncommon), DC 13/+5 and 15/+7.
- **Tests:** each new mechanic (repeat saves, multi-target healing, Revivify and its diamonds), plus a UI test.

## Non-goals for now

- **Spells that need a map, auras, or summons:** *Spiritual Weapon*'s later turns, *Spirit Guardians*, *Moonbeam*, *Call Lightning*'s later bolts, *Conjure Animals*. They're Utility for now.
- **Reactions:** *Counterspell* waits for reactions and isn't seeded.
- **Effects that outlast combat:** *Aid*'s 8 hours. It's Utility too.
- **Revivify outside combat.** There's no clock to tell "within the last minute" once the fight ends.

## Engine changes

- **`SpellEffects.RepeatSave`:** the target repeats the spell's save against the caster's DC at the end of each of its turns, and success ends the effect. This uses the same `SaveToEnd` that monster attacks already use (a Ghoul's paralysis).
- **Healing per target:** a Heal spell rolls once per target, so *Mass Healing Word* heals everyone it touches. Existing Heal spells target one creature, so they're unchanged.
- **A `Revive` resolution:**
  - Targets characters who died during this encounter.
  - Clears the dead flag and death saves, and sets HP to 1.
  - The character takes turns again.
- **`Spells.ConsumedItem`:** an item the spell uses up ("Diamonds (300 gp)"). Casting without one is refused. A spell scroll needs no material components (DMG), so reading *Revivify* from a scroll doesn't use the diamonds.

## Build phases

One phase:
- Migration 25 adds the columns, the spells, the class lists, effects, the diamonds, and scrolls.
- The engine changes and tests.
- A UI test, docs, and the release.
