# Effects system design: conditions, death saves, concentration

Status: **approved in part; step zero in progress.** Decisions so far are under [Decisions](#decisions). Questions 2, 4, and 5 are still open; they only matter from phase 2.

This covers the *Conditions* and *Death saving throws* TODOs, and phase 3 of the [magic system](magic-system.md): concentration, spell side effects, and a proper Dodge. All three need the same thing: **something attached to a creature that changes the rules until it ends**. Rules follow the **2014 PHB**: Appendix A (Conditions), chapter 9 (Dropping to 0 Hit Points), and chapter 10 (Concentration).

## Goals

- **Conditions** from Appendix A that actually change rolls and turns (advantage, disadvantage, auto-fail, skipped turns), shown in the combat UI with a plain-language tooltip.
- **Death saving throws:** characters at 0 HP are dying, not "Down". They roll each turn, can be stabilized or healed back up, and can die.
- **Concentration:** one spell at a time, lost on a failed CON save when taking damage.
- **Spell side effects** that phase 1 left out: *Bless*, *Guiding Bolt*, *Vicious Mockery*, *Ray of Frost*, *Faerie Fire*, *Shield of Faith*, *Divine Favor*, *Chill Touch*, *Charm Person*.
- **Dodge** as a real effect: disadvantage on attacks against you and advantage on DEX saves, spells included.
- **Monster effects:** a Wolf's bite can knock you prone.

## Non-goals for now

- **Effects that outlast combat:** *Mage Armor* for 8 hours, exhaustion levels. These need persisted effects; see [later](#later).
- **Reactions:** *Shield*, *Hellish Rebuke*, opportunity attacks. They need an interrupt in the middle of another creature's turn; that's its own change.
- **Temporary HP** (*Heroism*, *Armor of Agathys*), **damage resistances**, and **creature types** (*Protection from Evil and Good*).
- **Positioning:** there's still no map, so range, "within 5 feet", and forced movement stay simplified (see [rules without a map](#rules-without-a-map)).

## Step zero: move combat into the core library

Today all combat logic lives in the Qt dialog, `app/CombatEncounterDialog.cpp`: turn order, monster turns, damage, victory and defeat, and spellcasting. Only the attack roll itself (`resolve_attack`) is in `dndcore`, so none of the rest is unit-tested. Effects will touch almost every part of it: who can act, every roll, every point of damage, and the start and end of every turn. Adding that to the dialog would make the untested part much bigger.

**Proposal:** first extract a UI-free `dnd::Encounter` into `dndcore`, with **no behaviour change**. The dialog becomes a thin view over it. The encounter would hold:
- the participants, initiative, and turn order;
- the monster AI;
- the actions: attack, dash, dodge, drink potion, cast a spell;
- damage and healing, and victory and defeat;
- a list of **log events** that the dialog renders.

Pass in the dice (as today, via `IRandomSource`) and tests can play out whole fights deterministically. Persisting HP and spent slots stays the caller's job, reported through events, so `Encounter` doesn't depend on the repository.

This is the same "rules in `dndcore`, Qt is presentation only" split the rest of the app follows. Combat is the one place that drifted.

## The effect model

```cpp
struct ActiveEffect {
    EffectKind kind;          // a Condition, or a modifier (below)
    int target;               // participant it's on
    int source;               // participant who caused it (for durations, Frightened, Charmed)
    EffectDuration duration;  // how it ends (below)
    int amount = 0;           // e.g. +2 AC, 1d4 dice sides
    bool concentration = false;          // ends when the source's concentration ends
    std::optional<SaveToEnd> save_to_end;  // ability + DC, rolled at the end of the target's turn
    std::string label;        // "Blessed", "Prone", "Guiding Bolt"
};
```

### Conditions (2014 PHB, Appendix A)

What each one does in the engine. Nothing is "shown only" unless the table says so.

| Condition | Effect in the engine |
|---|---|
| Blinded | Its attacks have disadvantage; attacks against it have advantage. |
| Charmed | Can't attack or target the charmer with harmful spells. Informational for players; the monster AI respects it. |
| Deafened | Shown only. |
| Frightened | Disadvantage on attacks and ability checks while the source is alive. (Line of sight is assumed.) |
| Grappled | Shown only; speed isn't tracked. |
| Incapacitated | Can't take actions: the turn is skipped. |
| Invisible | Its attacks have advantage; attacks against it have disadvantage. |
| Paralyzed | Incapacitated. Auto-fails STR and DEX saves. Attacks against it have advantage, and melee hits are critical. |
| Petrified | Incapacitated; auto-fails STR and DEX saves; attacks against it have advantage. (Resistance to all damage waits for resistances.) |
| Poisoned | Disadvantage on attacks and ability checks. |
| Prone | Its attacks have disadvantage. Melee attacks against it have advantage; ranged attacks have disadvantage. It stands up automatically at the start of its turn. |
| Restrained | Its attacks and DEX saves have disadvantage; attacks against it have advantage. |
| Stunned | Incapacitated; auto-fails STR and DEX saves; attacks against it have advantage. |
| Unconscious | Incapacitated and prone; auto-fails STR and DEX saves; attacks against it have advantage, and melee hits are critical. |

**Dodging** becomes an effect as well: attacks against you have disadvantage and your DEX saves have advantage, until the start of your next turn.

### Modifiers (for spell and monster effects)

A short, fixed list, the same idea as the spell resolution kinds:

| Modifier | Used by |
|---|---|
| `AttackBonusDice(1d4)`, `SaveBonusDice(1d4)` | *Bless* |
| `ArmorClassBonus(+2)` | *Shield of Faith* |
| `ExtraWeaponDamageDice(1d4 radiant)` | *Divine Favor* |
| `AdvantageOnNextAttackAgainst` (used up by the first attack) | *Guiding Bolt* |
| `AdvantageOnAttacksAgainst` | *Faerie Fire* |
| `DisadvantageOnNextAttack` (used up) | *Vicious Mockery* |
| `CantRegainHitPoints` | *Chill Touch* |
| `SpeedReduction(10 ft)` (shown only) | *Ray of Frost* |

### Durations

| Duration | Example |
|---|---|
| `Rounds(n)`: counted down at the start of the **source's** turn | *Bless*: 1 minute = 10 rounds |
| `UntilStartOfTurn(participant)` | Dodge (own turn); *Ray of Frost*, *Chill Touch* (the caster's next turn) |
| `UntilEndOfTurn(participant)` | *Guiding Bolt*, *Vicious Mockery* (end of the caster's or target's next turn) |
| `UntilUsed` + one of the above | "the next attack against it" |
| `UntilRemoved` | Unconscious while dying; Charmed until damaged |
| any + `concentration` | ends when the caster's concentration does |
| any + `save_to_end` | the target repeats the save at the end of each of its turns (for later spells like *Hold Person*) |

Combat ends every effect. Effects that outlast combat are a [later](#later) change.

## How effects change the rules

Two pure functions sit between effects and the existing rolls. Nothing else needs to know about conditions:

- **`attack_roll_modifiers(attacker, target, melee)`** returns advantage and disadvantage sources, auto-crit, and bonus dice. Advantage and disadvantage cancel out, per the PHB, even if there are several of one. The result feeds the `RollMode` that `resolve_attack` already takes.
- **`saving_throw_modifiers(target, ability)`** returns advantage, disadvantage, auto-fail, and bonus dice. `cast_spell` gains a per-target roll mode and auto-fail flag. Today it assumes a plain d20.

Turn handling in `Encounter`:
- **Start of a turn:**
  - Durations tick, and Dodge and Prone end.
  - A dying character rolls a death save.
  - An incapacitated creature's turn is skipped.
- **End of a turn:** `save_to_end` rolls, and "until end of turn" effects expire.
- **After damage:**
  - A concentration check: a CON save with DC max(10, half the damage).
  - A creature at 0 HP when hit takes death-save failures.
  - A charmed creature hit by its charmer stops being charmed.

## Death and dying (PHB chapter 9)

- **Characters at 0 HP** fall unconscious and are **dying**. At the start of each turn they roll a d20:
  - 10 or higher is a success; lower is a failure.
  - A natural 1 counts as two failures.
  - A natural 20 means they regain 1 HP and are conscious again.
- **Three results end it:**
  - Three successes: **stable**.
  - Three failures: **dead**.
  - Any healing: conscious again, and the saves reset.
- **Damage at 0 HP** is one failure, or two on a critical hit. Melee hits on an unconscious target are always critical. **Massive damage:** if the damage left over after dropping to 0 is at least the character's max HP, they die outright.
- **Stabilizing** a dying ally:
  - A new **Stabilize** action: a DC 10 WIS (Medicine) check.
  - A *Healer's Kit* from the inventory makes it automatic.
  - *Spare the Dying* stops being a utility spell and does the same.
- **Monsters** die at 0 HP, as the PHB suggests for most monsters.

## Rules without a map

- **Melee attacks** are assumed to be within 5 feet. That's what makes Prone and Unconscious work: melee attackers get advantage, and melee hits on an unconscious target are crits.
- **Ranged attacks** are assumed to be *not* within 5 feet of the target, so there's no disadvantage for shooting in melee.
- **Frightened:** the source is assumed to be in sight.
- **Speed and forced movement** (*Ray of Frost*, *Thunderwave*, Grappled) are shown and logged only.

## Data changes

- **`SpellEffects` table:** which effects a spell applies, and when. The triggers are: **on hit**, **on a failed save**, or **on cast** (on the chosen targets, or on the caster). Each row gives the effect, duration, amount, and whether it uses concentration. It's seeded for the spells in [Goals](#goals). *Bless* and *Shield of Faith* become "on cast" spells that target allies. Their resolution kind changes from `Utility` to a new `Buff`, which rolls nothing and applies effects.
- **Monster on-hit effects:** two columns on `Monsters` (a save ability and DC, plus a condition), seeded for the Wolf: DC 11 STR save or knocked prone.
- **Characters:** a dead flag. Death saves themselves only exist during combat.

## Build phases

1. **Extract `Encounter`** into `dndcore`, with no behaviour change. It gets tests for turn order, monster turns, attacks, potions, spells, and victory or defeat, and the dialog becomes a view over it.
2. **Core effects:**
   - The effect model, conditions, modifiers, and durations.
   - The attack and save modifier functions, wired into attacks and `cast_spell`.
   - Dodge as an effect, death saves, and concentration.
   - Tests throughout.
3. **Data:** the `SpellEffects` migration and seeds, the `Buff` resolution, the Wolf's knock-prone, and the Stabilize action (plus Healer's Kit and *Spare the Dying*).
4. **UI:**
   - Conditions and effects in the participant table, with tooltips.
   - Death save pips (✓✓✗) for dying characters.
   - "Concentrating: Bless" on the caster.
   - The Stabilize action.
   - The dead state shown on the main window and sheet.

## Later

- **Persisted effects:** a `CharacterEffects` table for things that outlast combat, like *Mage Armor* (8 hours, ended by a long rest) and exhaustion levels (a long rest removes one).
- **Reactions:** *Shield*, *Hellish Rebuke*, *Feather Fall*, and opportunity attacks.
- **Temporary HP, resistances, and creature types.**

## Decisions

Settled 2026-09-29:

- **Step zero** (question 1): yes. Move combat into `dnd::Encounter` first, with no behaviour change.
- **Defeat** (question 3): when every character is at 0 HP, the fight ends in defeat at once, and anyone still dying is automatically **stabilized**. No one dies after the fight is lost.

## Open questions

1. **Step zero:** is it OK to spend phase 1 moving combat into `dndcore` with no visible change? It makes everything after it testable. The alternative is adding effects straight into the dialog: faster to see, but mostly untested.
2. **Character death:** when a character dies, should that be **permanent**? They'd be marked dead and couldn't join encounters, but you could still view them. Or should there be an option to revive them? The 2014 PHB's way back is *Revivify* and similar spells, which aren't seeded.
3. **Defeat:** today, all characters at 0 HP means defeat. With death saves, should defeat still end the fight at once, with anyone still dying automatically **stabilized**? That's forgiving, and suits solo play. Or should the monsters keep attacking and the death saves play out? That's brutal: melee hits on unconscious characters are critical.
4. **Monster targeting:** should monsters ignore dying characters while anyone else is still standing? I'd suggest yes, which also matches what most tables do.
5. **Bless-style bonus dice:** roll them automatically, adding the 1d4 to every eligible attack and save, and log them? I'd suggest yes.
