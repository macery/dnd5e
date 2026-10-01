# Monsters and encounter balance design

Status: **phases 1 (damage and creature types) and 2 (Multiattack and the roster) done;** phase 3 (encounter difficulty) is next. Decisions were settled on 2026-10-01; see [Decisions](#decisions).

**Phase 2 as built:**
- **Migration 19:**
  - **The roster:** the 13 SRD monsters (Hobgoblin through Troll) with their attacks, and a `Traits` column.
  - **On-hit conditions** can last N rounds with a repeat save (`OnHitRounds`, `OnHitRepeatSave`). A Ghoul's paralysis lasts 1 minute, with a DC 10 CON save at the end of each turn, and spares elves and undead.
  - **Existing monsters:** Kobolds and Wolves get Pack Tactics, and Zombies get Undead Fortitude.
- **Multiattack:** each attack is made its `count` times, and later attacks move to another conscious character if the target drops. Without a Multiattack only the first attack is used. That matches the SRD for the Ghoul, Gnoll, and Minotaur: the Ghoul has no Multiattack, so it claws (the claws paralyze) rather than bites. The design's roster table was wrong about that.
- **Monster attacks' extra dice** (a Hell Hound's +2d6 fire) are adjusted for the target's resistances separately.
- **Traits:**
  - **Pack Tactics:** advantage while another monster is up and not incapacitated.
  - **Martial Advantage:** +2d6 once per turn under the same condition.
  - **Undead Fortitude:** a CON save, DC 5 + the damage, to stay at 1 HP. It doesn't work against radiant damage or a critical hit.
  - **Regeneration:** 10 HP at the start of the Troll's turn, unless it took fire or acid since its last one. **A simplification:** a Troll at 0 HP is defeated like any monster. In the SRD it only dies if it starts its turn at 0 HP and doesn't regenerate.
  - **Turn state** now resets for each combatant's turn, so a monster's once-per-turn flags never leak into a character's turn.
- **Five adventures:** The Ogre's Larder, The Ghoul Crypt, The Bandit Captain's Camp, Hounds of the Burning Pass, and The Troll Bridge.
- **Monster list:** shows trait names, the Multiattack summary, and "or ..." for alternative attacks.

**Phase 1 as built:**
- **Migration 18:**
  - **Monster traits:** `Monsters` gains `CreatureType`, the damage resistance, vulnerability, and immunity lists, `ConditionImmunities`, and `Multiattack`. The six seeded monsters get their SRD types and traits.
  - **The `MonsterAttacks` table**, with damage types, extra dice, the on-hit save, and a count per turn. The old single-attack columns stay but are no longer read.
  - **`Spells.TargetCreatureType`:** "Humanoid" for *Charm Person*.
- **`dnd/damage`:** `DamageTraits`, `damage_modifier` (immunity wins; resistance and vulnerability cancel), `adjust_damage`, and log notes.
- **`Encounter`:**
  - **Typed hits:** every hit is typed and adjusted per part. A weapon's own damage (with Rage and Sneak Attack), radiant (Divine Favor and Divine Smite), a monster attack's extra dice, and spell damage are each adjusted for the target's traits separately.
  - **Rage** is resistance to bludgeoning, piercing, and slashing (`damage_traits()`).
  - **`add_effect`** refuses conditions the target is immune to.
  - ***Charm Person*** targets humanoids only, and **Divine Smite** adds 1d8 against undead and fiends.
  - **Monster turns** use the first `MonsterAttacks` row. Multiattack comes in phase 2.
  - **Status tooltips** list a monster's type and traits.
- **Monster list:** Type, Attacks (with damage types and riders), and Traits columns. Rules follow the 2014 Player's Handbook, and monster stat blocks the 2014 rules' SRD 5.1 (see question 1).

## Where things stand

- **Six monsters,** all CR ½ or lower: Kobold, Goblin, Skeleton, Wolf, Zombie, and Orc. Now that characters level up, a level-5 party has nothing worth fighting.
- **One attack per monster:** `Monster` has a single attack (name, bonus, damage dice) and one optional on-hit save (the Wolf's knock-prone). There's no Multiattack.
- **No creature types:**
  - *Charm Person* works on any monster; its description admits it.
  - Divine Smite's extra die against undead and fiends is left for later.
- **No damage types on monsters, and no resistances:**
  - Weapons and spells record a damage type, but nothing uses it except Rage.
  - Rage halves every monster attack as if it were physical.
  - A skeleton isn't vulnerable to bludgeoning, and a zombie isn't immune to poison.
- **No sense of difficulty:**
  - Adventures are fixed monster lists. In testing, a lone level-3 Wizard loses the three-goblin adventure about half the time.
  - Combat Encounter lets you add anything with no hint of how hard the fight is.

## Goals

- **Creature types** on every monster (humanoid, undead, beast, fiend, giant, monstrosity, ...), used by *Charm Person* (humanoids only) and Divine Smite (+1d8 against undead and fiends).
- **Typed damage everywhere,** applied in one place:
  - Monster attacks get a damage type, plus an optional extra damage type (Hell Hound: + fire).
  - **Resistance** halves damage (rounded down), **vulnerability** doubles it, and **immunity** makes it 0, applied after critical dice.
  - Rage becomes "resistance to bludgeoning, piercing, and slashing" in the same system.
- **Condition immunities:** a ghoul can't be charmed, and a skeleton can't be poisoned.
- **Multiattack:** a stat block with several attacks, made in order each turn. A later attack moves to another target if the first one drops.
- **A CR 1–5 roster** of SRD 5.1 monsters that fit a map-free engine (question 2).
- **Encounter difficulty:**
  - Rate any fight Easy, Medium, Hard, or Deadly against the party, using the 2014 encounter-building rules (XP thresholds by character level, with a multiplier for the number of monsters).
  - Show the rating in Combat Encounter and the adventure picker, and scale adventures to the party (question 5).

## Non-goals for now

- **Monster spellcasting, legendary and lair actions, and reactions** (Parry and the like).
- **Monster saving throw and skill proficiencies.** Monsters keep saving with their raw ability modifiers.
- **Magic weapons.** "Resistance to nonmagical bludgeoning, piercing, and slashing" applies to every weapon until magic items exist (question 4).
- **Traits that need a map or movement:** Charge, Pounce, Nimble Escape, Keen Smell.
- **A monster editor.** It stays its own TODO.

## Data model (one migration)

**`Monsters`, new columns:**
- **`CreatureType`:** "Humanoid", "Undead", "Beast", "Fiend", "Giant", "Monstrosity", "Fey", ...
- **`DamageResistances`, `DamageVulnerabilities`, `DamageImmunities`:** comma lists of damage types, such as `"poison"`, or `"bludgeoning,piercing,slashing"`.
- **`ConditionImmunities`:** a comma list of condition names (`"Charmed,Poisoned"`).
- **`Multiattack`:** a text summary for the UI, such as "Two claws and a bite". The attack rows decide what actually happens.

**`MonsterAttacks`,** a new table that replaces the single-attack columns:
- **`MonsterId`, `Position`:** the order attacks are made in a turn.
- **`Name`, `AttackBonus`, `DamageDiceCount`, `DamageDiceSides`, `DamageBonus`, `DamageType`, `Ranged`.**
- **`ExtraDiceCount`, `ExtraDiceSides`, `ExtraDamageType`:** a Hell Hound's bite adds 2d6 fire.
- **`OnHitSaveAbility`, `OnHitSaveDC`, `OnHitCondition`:** moved from `Monsters`, so the Wolf's bite and a Ghoul's paralyzing claws are per attack.
- **`Count`:** made this many times per turn (an Ogre's greatclub: 1; a Hill Giant's greatclub: 2).

The six existing monsters' attacks move into `MonsterAttacks` with their SRD damage types, creature types, and immunities:
- **Skeleton:** vulnerable to bludgeoning; immune to poison and the Poisoned condition. (Its exhaustion immunity has nothing to apply to: exhaustion isn't modeled.)
- **Zombie:** immune to poison and Poisoned.

**`dnd::Monster`** gets `creature_type`, the four lists, and `std::vector<MonsterAttack> attacks`. It drops the flat attack fields.

## Rules (pure functions in `dndcore`, with tests)

**`dnd/damage`:**
- `DamageModifier damage_modifier(const DamageTraits&, const std::string& type)`: returns Normal, Resisted, Vulnerable, or Immune.
- `int adjust_damage(int amount, DamageModifier)`.
- `Encounter::apply_damage` gains a damage type and applies the target's traits. A character's traits are Rage (bludgeoning, piercing, and slashing while raging) for now. The log names what happened: "(halved: resistant to slashing)", "(doubled: vulnerable to bludgeoning)", "(immune to poison)".
- Spells pass their `damage_type`, weapons theirs, and monster attacks theirs plus any extra-damage type, each part adjusted separately.

**Condition immunities:** `add_effect` refuses a condition the target is immune to and logs it ("The Ghoul is immune to being charmed").

**Creature types:**
- *Charm Person*'s targets are limited to humanoids. The spell's text loses "(any monster can be targeted)".
- Divine Smite adds 1d8 against undead and fiends.

**Multiattack (monster turns):**
- Each attack row is made `Count` times.
- The target is chosen once per turn, as today. If it drops, the next attack picks another conscious character.
- Each attack applies its own on-hit save.

**Difficulty (`dnd/encounter_difficulty`):**
- `xp_thresholds(level)`: Easy / Medium / Hard / Deadly per character, from the 2014 table.
- `encounter_multiplier(monster_count, party_size)`:
  - ×1 for 1 monster, ×1.5 for 2, ×2 for 3–6, ×2.5 for 7–10, ×3 for 11–14, ×4 for 15+;
  - one step higher for a party of fewer than 3, one step lower for 6 or more.
- `rate_encounter(party_levels, monsters)`: returns the adjusted XP, the four party thresholds, and the rating (Trivial below Easy). The awarded XP stays unadjusted, as the rules say.

## The roster (SRD 5.1, question 2)

Every one fights with weapons or natural attacks:

| CR | Monster | Why it's interesting here |
|----|---------|---------------------------|
| ½ | Hobgoblin | AC 18; Martial Advantage (question 3) |
| ½ | Gnoll | Bite or spear |
| 1 | Bugbear | 2d8 morningstar hits hard at low level |
| 1 | Dire Wolf | Bite with a DC 13 knock-prone |
| 1 | Ghoul | Claws paralyze (DC 10 CON; no Multiattack); immune to Charmed and poison; **undead** |
| 2 | Ogre | One big greatclub, 2d8+4; **giant** |
| 2 | Bandit Captain | Multiattack with three attacks; a humanoid for *Charm Person* |
| 3 | Owlbear | Multiattack: beak and claws; **monstrosity** |
| 3 | Hell Hound | Bite + 2d6 fire; immune to fire; **fiend**, so Divine Smite's extra die applies |
| 3 | Minotaur | Greataxe 2d12+4 |
| 4 | Ettin | Multiattack: battleaxe and morningstar |
| 5 | Hill Giant | Two greatclubs at 3d8+5 |
| 5 | Troll | Multiattack: bite and two claws; Regeneration (question 3) |

New adventures to match:
- an ogre's lair at around level 3;
- a ghoul crypt at around level 3, an undead fight for Paladins and Clerics;
- a bandit captain's camp at around level 4;
- a hell hound at around level 5;
- a troll bridge at around level 6–7.

## UI

- **Monster list:** shows the type, CR, resistances, immunities, and every attack.
- **Combat Encounter:**
  - A **difficulty line** under the monster picker updates as monsters are added: "Hard: 1,100 XP adjusted (party thresholds 500 / 1,000 / 1,500 / 2,200)". It has a tooltip explaining the bands.
  - The log names resistance, vulnerability, and immunity.
  - A monster's status tooltip lists its traits.
- **Adventure picker:** each adventure shows its rating for the party about to play it, scaled per question 5.

## Build phases

1. **Damage types and creature types:**
   - The migration (types, damage traits, condition immunities, the `MonsterAttacks` table) and the six existing monsters' data.
   - `dnd/damage`, with typed `apply_damage` and Rage folded in.
   - Condition immunities, *Charm Person* limited to humanoids, and Divine Smite's undead and fiend die.
   - The monster list shows it all.
   - Tests.
2. **Multiattack and the roster:** multiattack monster turns, extra-damage dice, the 13 SRD monsters, the chosen traits, and the new adventures. Tests.
3. **Encounter difficulty:** `dnd/encounter_difficulty`, the Combat Encounter difficulty line, adventure ratings and scaling. Tests, plus an offscreen walkthrough.

## Decisions

Settled 2026-10-01; every recommendation was taken:

1. **Stat blocks come from the 2014 SRD 5.1.**
2. **The roster is the 13 monsters above.**
3. **Traits:** Pack Tactics, Undead Fortitude, Regeneration, and Martial Advantage, in phase 2.
4. **Every weapon counts as nonmagical** until magic items exist.
5. **Adventures show their rating for the party** and scale monster counts toward Medium–Hard.
6. **Rage folds into the resistance rules:** bludgeoning, piercing, and slashing only.

## Open questions

1. **Stat block source:** the 2014 SRD 5.1. It's the open-licensed subset of the 2014 Monster Manual, and every monster above is in it (recommended). Or do you have the Monster Manual and want to go beyond the SRD?
2. **The roster:** the 13 above (recommended), or a different mix: fewer and more iconic, more undead or fiends, more humanoids for *Charm Person*?
3. **Monster traits that work without a map:**
   - **(a) Recommended:**
     - **Pack Tactics** (Kobold, Wolf): advantage when an ally is fighting, using the same "another party member counts" rule as Sneak Attack.
     - **Undead Fortitude** (Zombie): a CON save to drop to 1 HP instead of 0.
     - **Regeneration** (Troll): 10 HP at the start of its turn unless it took fire or acid damage.
     - **Martial Advantage** (Hobgoblin): +2d6 once per turn when one of its allies is also fighting the target.
   - **(b)** Data and damage only for now, and traits later.
4. **"Nonmagical" resistances:** treat every weapon as nonmagical until magic items exist (recommended), or leave out monsters with that resistance? (None of the 13 above have it.)
5. **Adventures and difficulty:**
   - **(a) Recommended:** show each adventure's rating for the selected party, and scale monster counts to aim at Medium–Hard.
   - **(b)** Show the rating only, and leave adventures fixed.
   - **(c)** Adventures list a recommended level, with no scaling.
6. **Rage and the new system:** fold Rage's halving into the resistance rules. Its log text becomes "(halved: resistant to slashing)", and it stops halving fire or poison from future monsters, as the PHB says (recommended).
