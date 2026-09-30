# Bonus actions and class features design

Status: **phase 1 (turn economy and bonus actions) done;** phase 2 (core features) is next. The open questions were settled on 2026-09-30; see [Decisions](#decisions).

**Phase 1 as built:**
- **`dnd/turn`:** `TurnState` (actions left, attacks left in an Attack action, bonus action used) and the PHB ch. 10 spell rule (`spell_casting_blocked` / `record_spell_cast`).
- **`dnd/class_features`:** starts with `attacks_per_attack_action`, for Extra Attack.
- **`Encounter`:**
  - Tracks the turn state. The first attack spends the action; with Extra Attack, the rest of the Attack action follows, and any other action gives it up.
  - A spell's own casting time picks the action or the bonus action. `castable_spells()` only lists what the rule allows right now, and reaction spells aren't offered.
  - Refused actions say why in the log.
- **Combat dialog:**
  - A live turn status line, e.g. "Attacks left: 1 (Extra Attack) · Bonus action: available", or "Nothing left this turn".
  - Bonus-action spells tagged "(bonus action)" in the Cast a Spell list.
  - The button stays live while anything's left.
- **One change from the design:** bonus-action spells stay in the Cast a Spell list, tagged, rather than getting a row of their own. They need the same slot and target controls. Phase 2's non-spell bonus actions (Second Wind, Rage, Cunning Action) get the separate Bonus Action row.

This covers the TODO items **Bonus actions** and **Class features**, and touches **Level-up choices**. Rules follow the **2014 PHB**:
- ch. 3 for class features;
- ch. 9 ("Bonus Actions", "Two-Weapon Fighting") for the turn structure;
- ch. 10 ("Casting Time → Bonus Action") for the spellcasting rule.

## Where things stand

- **A turn is one action.** There's no bonus action: the "Bonus Action: none available" line in combat is text only.
- ***Healing Word*, *Shield of Faith*, *Divine Favor*, and *Sanctuary*** are bonus actions in the PHB, but are cast as actions here.
- **Every class fights the same way:** attack, dodge, drink a potion, or cast a spell. No class feature is modeled except Barbarian and Monk Unarmored Defense (in AC) and spellcasting.
- **Nothing tracks per-rest uses** besides spell slots and hit dice.

## Goals

- **A real turn economy:** one action, one bonus action, and extras from features (Action Surge, Extra Attack).
- **Bonus-action spells** follow the PHB rule: if you cast a spell as a bonus action, the only other spell you can cast that turn is a cantrip with a casting time of 1 action.
- **The signature level 1–5 combat features of each class**, so classes play differently.
- **Uses that come back on a short or long rest**, saved on the character and shown on the sheet.

## Non-goals for now

- **Subclasses** (Champion, Thief, Life Domain, ...), which start at level 3, and their features.
- ***Wild Shape*, *Channel Divinity*, Eldritch Invocations, Sorcery Points, and *Hunter's Mark*.** They need subclasses, stat-block swaps, or new spells.
- **Anything needing a map or reactions:** Hide's real benefit, Disengage's benefit, opportunity attacks, the Protection fighting style.
- **Features past level 5.**

## Turn economy

Each turn, `Encounter` tracks:
- **Actions left:** one normally, two after Action Surge.
- **Bonus action used** (yes/no).
- **Attacks left in the current Attack action:** one, or two with Extra Attack at level 5. Taking the Attack action with Extra Attack gives two separate attack rolls; the second can target someone else.
- **"Cast a levelled spell as a bonus action this turn":** blocks casting anything except an action cantrip for the rest of the turn.
- **Once-per-turn flags** (Sneak Attack used) and **"this turn" choices** (Reckless Attack).

**UI:** the "Your Turn" box gets a **Bonus Action** row (a choice plus a button) next to the Action row. Available bonus actions come from the character's class, level, equipment, and remaining uses. Next Turn ends the turn, as today.

## Class features (2014 PHB ch. 3, levels 1–5)

| Class | Feature (level) | Type | Uses | What it does in the engine |
|---|---|---|---|---|
| Barbarian | **Rage** (1) | Bonus action | 2 (3 from level 3), long rest | 1 minute. **Advantage** on STR checks and saves. **+2 damage** on STR melee weapon attacks. **Resistance** to bludgeoning, piercing, and slashing. No spellcasting or concentration. Ends early if you're knocked unconscious, or at the end of a turn in which you neither attacked nor took damage. Not in heavy armor. |
| Barbarian | Reckless Attack (2) | Choice on your first attack | — | Advantage on your STR melee attacks this turn; attacks against you have advantage until your next turn. |
| Bard | **Bardic Inspiration** (1) | Bonus action | CHA mod (min 1), long rest (short from level 5) | An ally (not you) gains a d6 (d8 at level 5) to add to one attack roll, ability check, or save within 10 minutes. It's used up by the next roll it would help. |
| Fighter | **Fighting Style** (1) | Passive, chosen | — | **Defense** +1 AC in armor; **Dueling** +2 damage with a one-handed melee weapon; **Archery** +2 to ranged attack rolls; **Great Weapon Fighting** reroll 1s and 2s on two-handed damage dice. (Protection and Two-Weapon Fighting need reactions or an off-hand; see Later.) |
| Fighter | **Second Wind** (1) | Bonus action | 1, short rest | Regain 1d10 + fighter level HP. |
| Fighter | **Action Surge** (2) | Free, on your turn | 1, short rest | One additional action this turn. |
| Monk | **Martial Arts** (1) | Passive + bonus action | — | Unarmed strikes and monk weapons can use DEX; unarmed strikes deal a d4 (d6 at level 5). After an Attack action with them, one unarmed strike as a bonus action. Only without armor or a shield. |
| Monk | **Ki** (2) | Bonus actions | Ki = monk level, short rest | **Flurry of Blows** (1 ki): two unarmed strikes after the Attack action. **Patient Defense** (1 ki): Dodge. **Step of the Wind** (1 ki): Dash (shown only). |
| Paladin | **Lay on Hands** (1) | Action | Pool of 5 × level HP, long rest | Heal an ally (or yourself) by any amount from the pool. |
| Paladin | **Divine Smite** (2) | On a melee hit | Spell slots | Spend a slot: +2d8 radiant, +1d8 per slot level above 1st (max 5d8). +1d8 against undead or fiends once creature types exist. |
| Rogue | **Sneak Attack** (1) | Once per turn, on a hit | — | +1d6 per 2 levels (rounded up) with a finesse or ranged weapon, if you have advantage, **or** an ally is fighting the target and you don't have disadvantage (see question 2). |
| Rogue | **Cunning Action** (2) | Bonus action | — | Dash (shown only) or Disengage (shown only; no map). Hide is left out until hiding means something. |
| Wizard | **Arcane Recovery** (1) | On a short rest | Once per day (until a long rest) | Recover spell slots with a combined level up to half the wizard level, rounded up (none 6th or higher). |
| Fighter, Barbarian, Paladin, Ranger, Monk | **Extra Attack** (5) | Part of the Attack action | — | Two attacks per Attack action. |
| Casters | **Bonus-action spells** | Bonus action | Spell slots | *Healing Word*, *Shield of Faith*, *Divine Favor*, and *Sanctuary* move to the Bonus Action row. The PHB bonus-action spell rule applies. |

Cleric, Druid, Ranger, Sorcerer, and Warlock gain nothing from this table in levels 1–5 beyond bonus-action spells and Extra Attack. Their level 1–5 features are subclass features or listed as non-goals above.

## Data and code

Class features are **code**: each has its own mechanics. They live in a new `dnd/class_features` module:
- **A registry:** for each feature, its id, class, minimum level, how it's used (action, bonus action, free, on hit, passive), a formula for its uses, when uses come back (short or long rest), and a description for tooltips.
- **Pure rules functions with tests:** e.g. `rage_damage_bonus(level)`, `sneak_attack_dice(level)`, `martial_arts_die(level)`, `arcane_recovery_budget(level)`, `feature_uses(feature, character)`.

**New effects** (in `dnd/effects`):
- `Raging`: advantage on STR saves, and the damage and resistance changes above.
- `Reckless`: attacks against it have advantage.
- `InspirationDie`: its die is added to the next attack or save, then it's gone.

**Resistance** comes in as a narrow rule only for Rage: bludgeoning, piercing, and slashing damage is halved. Weapons already record their damage type. Monster attacks aren't typed yet, but every seeded monster attack is a weapon or natural attack, so all of them count as physical. Proper damage types stay their own TODO.

**Per-character state** (a new migration):
- **`Characters.FeatureUses`:** uses spent per feature, as text like `"SecondWind:1,Rage:2"`. It's stored like spell slots, so rests stay pure functions on `Character`.
- **`Characters.FightingStyle`:** the Fighter's style, and later the Paladin's and Ranger's at level 2.

**Rests:** Short Rest restores short-rest features (Second Wind, Action Surge, Ki) and offers Arcane Recovery. Long Rest restores everything.

## Build phases

1. **Turn economy and bonus actions:**
   - `Encounter` tracks the action, bonus action, attacks left, and the spell rule.
   - The Bonus Action row in combat.
   - Bonus-action spells move to it.
   - Extra Attack.
   - Tests for the turn rules.
2. **Core features:**
   - Second Wind, Action Surge, Rage (with resistance), Sneak Attack, Cunning Action, Bardic Inspiration, Martial Arts, and Arcane Recovery.
   - Feature uses saved on the character, restored by rests, and shown on the sheet.
   - Tests.
3. **Choices and the rest:** Fighting Styles (chosen in the creation wizard for Fighters), Ki (Flurry of Blows, Patient Defense, Step of the Wind), Lay on Hands, Divine Smite, and Reckless Attack.
4. **Later:**
   - Fighting Style choices on level-up (Paladin, Ranger).
   - Two-weapon fighting (needs an off-hand slot in equipment).
   - Subclasses.
   - Invocations and Sorcery Points.
   - Reactions (Protection).
   - Divine Smite's extra die against undead and fiends (needs creature types).

## Decisions

Settled 2026-09-30:

1. **Three phases**, in the order under [Build phases](#build-phases): the turn economy and bonus actions first, then the core features, then choices and the rest.
2. **Sneak Attack without a map:** any other conscious party member counts as "an enemy of the target within 5 feet of it". The log says so when that's what enabled it. Advantage still works alone, and disadvantage still blocks it.
3. **Rage resistance comes in now,** as a narrow rule: while raging, bludgeoning, piercing, and slashing damage is halved (rounded down). Every seeded monster attack counts as physical until proper damage types exist.
4. **Fighting Style** is chosen in the creation wizard for Fighters, in phase 3. Until then, no Fighter has a style.
5. **Existing characters** start with every feature use available, and existing Fighters have no Fighting Style until they pick one.
