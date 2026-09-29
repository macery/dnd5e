# Magic system design

Status: **phase 1 (core and data) implemented;** phase 2 (UI) is next. The open questions were settled on 2026-09-29; see [Decisions](#decisions).

### Phase 1 status

The starter list alone left several classes short of their level-1 picks, e.g. a Cleric needs 3 cantrips and the PHB's only damaging Cleric cantrip is *Sacred Flame*. **Decision (2026-09-29):** add a fifth resolution kind, **`Utility`**. These spells have no automated effect yet: casting one spends the slot and shows its description. 24 PHB utility spells are seeded by migration 12, such as *Light*, *Guidance*, *Detect Magic*, *Mage Armor*, *Bless*, and *Charm Person*. A test checks that every caster can fill its level-1 picks, even with a 20 in its casting ability. Utility spells gain real effects over time, e.g. *Mage Armor* once armor and AC are modeled, and *Bless* with phase 3 effects.

## Goals

- Spellcasting classes (Bard, Cleric, Druid, Sorcerer, Warlock, Wizard) can pick spells at creation, see their spellcasting stats on the sheet, and cast spells in combat.
- Rules follow the **2014 PHB**, like the class, race, and background data.
- Spells should be data wherever possible, so adding a spell usually means adding a seeded row, not writing code.
- Keep the existing layering:
  - **Rules** go in `dndcore` as pure, tested functions.
  - **Seeded content** goes in the database.
  - **Qt** is presentation only.

## Non-goals for now

- **No map.** Range, areas, and movement stay informational, as they are for weapons today.
- **Nothing that needs an active-effects system.** That means concentration, durations, conditions, reactions (e.g. *Shield*), and ongoing damage. They get designed together with the *Conditions* TODO in phase 3.
- **No utility or ritual spells** with no combat or healing effect (e.g. *Detect Magic*).
- **Paladin and Ranger** are half-casters who only get spells at level 2. The data model covers them, but only as far as slots go (see phase 2).

## What's data and what's code

The engine interprets a small, fixed set of **resolution kinds**. Anything that fits one of them is a seeded row. A spell that fits none of them is left out until the kind it needs exists. That keeps the starter list honest instead of half-working.

| Resolution kind | How it resolves | Starter examples |
|---|---|---|
| `Attack` | Spell attack roll vs AC, reusing `resolve_attack`: crits double the dice, a natural 1 misses. | Fire Bolt, Eldritch Blast, Guiding Bolt, Inflict Wounds |
| `Save` | Each target makes a saving throw against the spell save DC. On a success they take half damage or none, depending on the spell. | Sacred Flame, Burning Hands, Thunderwave |
| `AutoHit` | No roll; each projectile always hits. | Magic Missile |
| `Heal` | Heals a friendly target by the dice plus the spellcasting modifier, capped at max HP. | Cure Wounds, Healing Word |
| `Utility` | No automated effect yet; casting spends the slot and shows the description. | Light, Detect Magic, Bless |

"Code only" effects are anything that changes state over time or reacts to events: concentration, conditions, reactions, temporary HP, summons, or HP-pool effects like *Sleep*. They wait for phase 3.

### `Spells` table (seeded by a migration)

| Column | Meaning |
|---|---|
| `Name`, `Level` (0 = cantrip), `School`, `Description` | Shown in the UI. |
| `CastingTime` | `Action`, `BonusAction`, or `Reaction`. A bonus action spell like *Healing Word* is still an action in combat for now, because bonus actions don't exist there yet. |
| `RangeText`, `DurationText` | Shown only. |
| `Concentration` | Stored now, enforced in phase 3. Starter spells avoid it. |
| `Resolution` | One of the four kinds above. |
| `AttackRange` | `Melee` or `Ranged`. Only used for `Attack` spells. |
| `SaveAbility`, `SaveHalves` | For `Save` spells: which ability saves, and whether a success still takes half damage. |
| `DiceCount`, `DiceSides`, `FlatBonus`, `AddsModifier` | The effect roll. *Cure Wounds* is 1d8 plus the modifier (`AddsModifier`); a *Magic Missile* dart is 1d4+1 (`FlatBonus`). |
| `DamageType` | Text for now; used later by resistances. |
| `Projectiles` | Separate rolls per cast: 3 for Magic Missile, and Eldritch Blast beams. Defaults to 1. |
| `MaxTargets` | How many creatures one cast affects. See [areas without a map](#areas-without-a-map). |
| `UpcastDice`, `UpcastProjectiles` | Added per slot level above the spell's level: +1 die for *Cure Wounds*, +1 dart for *Magic Missile*. |
| `CantripScaling` | `Dice` (most cantrips add a die at levels 5, 11, and 17) or `Projectiles` (Eldritch Blast adds a beam instead). |

**Class spell lists:** a `ClassSpells(ClassID, SpellID)` join table. This keeps "which classes can learn this spell" as seeded content, next to the spell itself.

### Class spellcasting rules (code, added to `ClassInfo`)

These are fixed rules tables, like hit dice, so they belong in `dnd/character_creation.hpp` rather than the database:
- **Spellcasting ability:** INT for Wizard; WIS for Cleric, Druid, and Ranger; CHA for Bard, Sorcerer, Warlock, and Paladin.
- **Caster type:** `Full`, `Half`, `Pact`, or `None`. This picks the slot table.
- **How spells are chosen:**
  - **Known:** Bard, Sorcerer, Warlock, and Ranger learn a fixed list.
  - **Prepared:** Cleric, Druid, and Paladin prepare from their whole class list; the Wizard prepares from their spellbook.
- **Per-level counts:** cantrips known, plus spells known or the spellbook size.

Level-1 counts:

| Class | Cantrips | Level-1 spells | Slots |
|---|---|---|---|
| Bard | 2 | 4 known | 2 × 1st |
| Cleric | 3 | prepare WIS mod + level (min 1) | 2 × 1st |
| Druid | 2 | prepare WIS mod + level (min 1) | 2 × 1st |
| Sorcerer | 4 | 2 known | 2 × 1st |
| Warlock | 2 | 2 known | 1 pact slot (1st) |
| Wizard | 3 | 6 in spellbook, prepare INT mod + level | 2 × 1st |

### New rules functions (`dnd/spellcasting`)

- `spell_save_dc(character, class)`: 8 + proficiency + spellcasting modifier.
- `spell_attack_bonus(character, class)`: proficiency + spellcasting modifier.
- `spell_slots(class, level)`: an array of 9 slot counts, from the full, half, or pact table. The tables cover levels 1 to 20.
- `cantrip_tier(level)`: 1, 2, 3, or 4 at levels 1, 5, 11, and 17.
- `cast_spell(dice, spell, caster stats, slot level, targets) → SpellCastResult`: one result per target and projectile. For an attack, that's the roll, hit or crit, and damage. For a save, it's the save roll, success, and damage. For a heal, it's the HP healed. Like `resolve_attack`, it doesn't touch the database or HP; the caller applies the result.
- Slot bookkeeping as pure `Character` functions, like `spend_hit_die`: `spend_spell_slot` and `slots_remaining`. `take_long_rest` restores every slot. A new `take_short_rest` restores Warlock pact slots.

**Monster saving throws:** use the plain ability modifier. The seeded monsters' stat blocks have no save proficiencies, so this is accurate for them. A `Monsters.SaveBonuses` column can come later if a monster needs one.

### Per-character state

- **Known spells:** a `CharacterSpells(CharacterID, SpellID)` join table, managed through the repository like `CharacterInventory` and deleted with the character.
- **Spent slots:** stored on `Character` as `std::array<int, 9> spell_slots_used`, persisted as one TEXT column. Rests are then pure functions on `Character`, like `hit_dice_used`, and `update_character` stays a single statement. Slot totals are never stored; they're derived from class and level.

## Areas without a map

There's no grid, so an area spell (a cone, cube, or sphere) lets the caster **choose up to `MaxTargets` enemies**, set per spell as a reasonable "how many fit" number:
- *Burning Hands* (15 ft cone): 3.
- *Thunderwave* (15 ft cube): 3.
- *Arms of Hadar* (10 ft radius): 4.

*Magic Missile* darts are shared round-robin across the chosen targets. That covers the usual choices, all darts on one target or one dart each, without a per-dart picker.

## Starter spell list

Chosen because each one fits a resolution kind with no concentration, reaction, or condition. Together they give every full caster something to cast from level 1.

| Spell | Lvl | Kind | Effect | Upcast / scaling | Classes |
|---|---|---|---|---|---|
| Fire Bolt | 0 | Attack (ranged) | 1d10 fire | +1d10 per tier | Sorcerer, Wizard |
| Ray of Frost | 0 | Attack (ranged) | 1d8 cold | +1d8 per tier | Sorcerer, Wizard |
| Chill Touch | 0 | Attack (ranged) | 1d8 necrotic | +1d8 per tier | Sorcerer, Warlock, Wizard |
| Shocking Grasp | 0 | Attack (melee) | 1d8 lightning | +1d8 per tier | Sorcerer, Wizard |
| Produce Flame | 0 | Attack (ranged) | 1d8 fire | +1d8 per tier | Druid |
| Eldritch Blast | 0 | Attack (ranged) | 1d10 force per beam | +1 beam per tier | Warlock |
| Sacred Flame | 0 | Save (DEX, none) | 1d8 radiant | +1d8 per tier | Cleric |
| Poison Spray | 0 | Save (CON, none) | 1d12 poison | +1d12 per tier | Druid, Sorcerer, Warlock, Wizard |
| Vicious Mockery | 0 | Save (WIS, none) | 1d4 psychic | +1d4 per tier | Bard |
| Magic Missile | 1 | AutoHit | 3 darts × 1d4+1 force | +1 dart / slot | Sorcerer, Wizard |
| Burning Hands | 1 | Save (DEX, half), 3 targets | 3d6 fire | +1d6 / slot | Sorcerer, Wizard |
| Thunderwave | 1 | Save (CON, half), 3 targets | 2d8 thunder | +1d8 / slot | Bard, Druid, Sorcerer, Wizard |
| Guiding Bolt | 1 | Attack (ranged) | 4d6 radiant | +1d6 / slot | Cleric |
| Inflict Wounds | 1 | Attack (melee) | 3d10 necrotic | +1d10 / slot | Cleric |
| Dissonant Whispers | 1 | Save (WIS, half) | 3d6 psychic | +1d6 / slot | Bard |
| Arms of Hadar | 1 | Save (STR, half), 4 targets | 2d6 necrotic | +1d6 / slot | Warlock |
| Cure Wounds | 1 | Heal (touch) | 1d8 + mod | +1d8 / slot | Bard, Cleric, Druid, Paladin, Ranger |
| Healing Word | 1 | Heal (range) | 1d4 + mod | +1d4 / slot | Bard, Cleric, Druid |

Side effects (Ray of Frost's slow, Guiding Bolt's advantage, Vicious Mockery's disadvantage, Thunderwave's push) are left out in phase 1. The spell descriptions will say so. They come back with the active-effects system.

*Shield* is on the current TODO, but it's a reaction, so it moves to phase 3 along with the reaction system.

## Phases

1. **Core and data** (tests only, no UI):
   - Migration 11: `Spells` + `ClassSpells` seeded with the list above, `CharacterSpells`, and `Characters.SpellSlotsUsed`.
   - `ClassInfo` spellcasting fields, the slot tables, DC and attack bonus.
   - `cast_spell`, slot spending, and rests that restore slots.
   - Repository methods for spells and known spells.
2. **UI:**
   - **Wizard step "Choose Spells"** (casters only, after Skills). Pick cantrips and level-1 spells, with counts from the table above.
   - **Character sheet "Spellcasting" section:** ability, DC, attack bonus, slots remaining, known spells with descriptions.
   - **"Cast a Spell" combat action:** pick a spell, a slot level (for levelled spells), and targets. Results go to the log, in the same style as weapon attacks.
   - **Healing out of combat:** cast *Cure Wounds* and *Healing Word* from the sheet or a small spellbook dialog, the same way potions work in the Inventory dialog.
   - **Short Rest** restores pact slots; **Long Rest** restores all slots.
   - Paladin and Ranger get their slots at level 2.
   - A way to learn new spells on level-up. Phase 2 could be a simple "Learn spells" dialog that opens when a caster levels up.
3. **Effects** (designed together with the *Conditions* and *Death saving throws* TODOs):
   - Durations and concentration.
   - Conditions caused by spells.
   - Reactions (*Shield*, *Hellish Rebuke*).
   - The phase-1 side effects listed above.
   - Changing prepared spells on a long rest.
   - Scrolls and wands through the inventory.

## Decisions

Settled 2026-09-29:

1. **Areas:** area spells let the caster choose up to `MaxTargets` enemies, as described in [Areas without a map](#areas-without-a-map). This may be revisited if a map is ever added.
2. **Prepared casters:** the spells a Cleric, Druid, or Wizard picks in the creation wizard are their prepared list. Re-preparing on a long rest is phase 3.
3. **Bonus actions:** every spell is cast as an action for now, *Healing Word* included. A real bonus-action slot in combat is a separate, later change.
4. **Monster spellcasters:** out of scope. Monsters keep their single attack.
5. **Rules edition:** the 2014 PHB throughout, matching the class, race, and background data.
