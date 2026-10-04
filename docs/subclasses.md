# Subclasses design

Status: **decided; phase 1 in progress.** Decisions were settled on 2026-10-03: the twelve SRD subclasses, with every recommendation taken (1a, 2a, 3a, 4a, 5a, 6a, 7a).

Other PHB subclasses stay out because the SRD is the freely licensed part of the rules, and the repo is public. The design keeps subclasses as data plus feature code, so more can be added later.

**Phase 1 as built:**
- **`dnd/subclass`:**
  - The twelve subclasses as data: features (with `automated` flags), spells (always prepared, or a Fiend's expanded list), and the dragon or land choice.
  - `subclass_level`, `subclass_kind`, `subclass_due`, `choose_subclass` (with Draconic HP), `on_spell_list`, and `always_prepared_spell_names`.
- **Migration 28:** `Characters.Subclass`, `SubclassChoice`, `Exhaustion`, and `TempHP`, plus 16 Utility spells the subclass lists name (Acid Arrow, Blur, Darkness, ... Stinking Cloud, Water Walk). Exports carry the new fields.
- **Rules:**
  - Channel Divinity and Natural Recovery as limited features.
  - The Land druid's bonus cantrip, and Fiend expanded spells in `validate_spell_selection`.
  - Long rests remove one exhaustion level and all temporary HP.
  - Draconic AC 13 + DEX and +1 HP per level, and Life Domain heavy armor.
  - Level Up names subclass features in place of the "… feature" placeholders. Subclass choices, Channel Divinity, Destroy Undead, and Uncanny Dodge now count as automated.
- **Combat:**
  - Temporary HP (absorbed first, shown as "+N temp").
  - Exhaustion: disadvantage on initiative at 1, on attacks and saves at 3, max HP halved at 4, death at 6.
  - Champion crits on 19–20, Colossus Slayer, and Dark One's Blessing.
  - Cutting Words (an automatic reaction for any Lore bard in the party).
  - Rage with Frenzy and Frenzy attacks, with exhaustion when the rage ends.
  - Open Hand Technique (prone on a failed DEX save), Fast Hands (a potion as a bonus action), and Disciple of Life.
  - Channel Divinity: Turn Undead (and Destroy Undead), Preserve Life, Sacred Weapon, and Turn the Unholy. The new **Turned** effect ends on damage.
  - Always-prepared subclass spells reach combat and the Spellbook through `spells_ready`.
- **Tests:** `tests/test_subclasses.cpp`.

Rules follow the 2014 Player's Handbook. The subclasses are the twelve in the free System Reference Document (SRD 5.1), one per class.

## Where things stand

- **No subclasses.** Level Up's summary lists "Divine Domain", "Martial Archetype", "Primal Path", and so on as "not automated yet". Nothing is chosen, stored, or applied.
- **Every class has picked one by level 3:**
  - Clerics, Sorcerers, and Warlocks at level 1;
  - Druids and Wizards at level 2;
  - everyone else at level 3.

  A level-5 Fighter plays exactly like every other level-5 Fighter.
- **The content tops out around level 5** (the Hill Giant's Toll), and the new spells stop at 3rd level.

## Goals

- **Choose a subclass** when the class gets one:
  - in the creation wizard (Cleric, Sorcerer, Warlock);
  - in Level Up (everyone else);
  - for existing characters who are already past that level (question 3).
- **Show it** on the sheet ("Fighter 5 (Champion)") and in the table.
- **Automate each subclass's features through level 5** where combat can apply them, and list the rest as "not automated yet":

  | Class | Subclass | Features to level 5, and what the app does with them |
  |---|---|---|
  | Barbarian | Path of the Berserker | 3: **Frenzy**: while raging, an extra melee weapon attack as a bonus action each turn; **one level of exhaustion** when the rage ends |
  | Bard | College of Lore | 3: three bonus skill proficiencies; **Cutting Words**: a reaction that spends Bardic Inspiration to subtract the die from a monster's attack roll (automatic, like *Shield*) |
  | Cleric | Life Domain | 1: heavy armor proficiency; **Disciple of Life**: healing spells restore 2 + the spell's level extra; domain spells always prepared (*Bless*, *Cure Wounds*; then *Lesser Restoration*, *Spiritual Weapon*; *Beacon of Hope*, *Revivify*). 2: **Channel Divinity: Preserve Life**, healing 5 × cleric level, split among allies, none above half their max |
  | Druid | Circle of the Land | 2: a bonus cantrip; **Natural Recovery**: on a short rest, regain spell slots worth up to half the druid's level (like Arcane Recovery). 3, 5: circle spells for a chosen land, always prepared |
  | Fighter | Champion | 3: **Improved Critical**: crits on 19–20 |
  | Monk | Way of the Open Hand | 3: **Open Hand Technique**: each Flurry of Blows hit can knock the target prone (DEX save) or stop its reactions |
  | Paladin | Oath of Devotion | 3: **Channel Divinity: Sacred Weapon** (+CHA to attack rolls for a minute); oath spells always prepared (*Protection from Evil and Good*, *Sanctuary*; then *Lesser Restoration*, *Zone of Truth*) |
  | Ranger | Hunter | 3: **Hunter's Prey: Colossus Slayer**: +1d8 once per turn against a creature below its max HP |
  | Rogue | Thief | 3: **Fast Hands**: Cunning Action can also drink a potion (question 6) |
  | Sorcerer | Draconic Bloodline | 1: a dragon ancestor; **Draconic Resilience**: +1 HP per sorcerer level, and AC 13 + DEX unarmored |
  | Warlock | The Fiend | 1: **Dark One's Blessing**: temporary HP (CHA + warlock level) on dropping a hostile creature to 0; expanded spells (*Burning Hands*, *Command*; then *Blindness/Deafness*, *Scorching Ray*; *Fireball*, *Stinking Cloud*) |
  | Wizard | School of Evocation | 2: Evocation Savant and Sculpt Spells. Shown only: there's no map, so area spells already hit only the targets you choose |

- **New mechanics these need, built along the way:**
  - **Temporary hit points** (Dark One's Blessing). This also clears the README TODO.
  - **Exhaustion**, saved on the character and recovered by a long rest. It clears that half of "Effects that outlast combat".
  - **Channel Divinity uses**: Clerics from level 2, Paladins from level 3, back on a short rest. They also cover the base **Turn Undead** (question 4).
  - **Always-prepared subclass spells** that don't count against the allowance.
- **Tests:** each automated feature, the choice in creation, Level Up, and existing characters, plus UI tests.

## Non-goals for now

- **Features above level 5**, except as names in the Level Up summary.
- **Non-SRD subclasses** (Battle Master, Assassin, Arcane Trickster, Eldritch Knight, ...). Their names aren't in the SRD.
- **Features that need a map, exploration, or social play:** Second-Story Work, Turn the Unholy's movement, Natural Recovery outside short rests, and so on.

## Data model

Migration 28:
- `Characters.Subclass` (a name; empty for none) and `Characters.SubclassChoice` (the dragon ancestor, or the druid's land).
- `Characters.Exhaustion` (0–6) and `Characters.TempHP`.
- Subclass spells as data: `SubclassSpells (Subclass, ClassLevel, SpellName)`.
- Channel Divinity uses ride on the existing `FeatureUses` ("ChannelDivinity").

## Build phases

1. **Rules and data:**
   - Subclass definitions, choice, and storage.
   - Temporary HP, exhaustion, and Channel Divinity.
   - Each subclass's features in the rules and the encounter.
   - Always-prepared spells.
   - Tests.
2. **UI:**
   - Choosing in the creation wizard and Level Up, and a prompt for existing characters.
   - The sheet and table.
   - Combat controls: Frenzy, Preserve Life, Sacred Weapon, Turn Undead, and Open Hand Technique's choice.
   - Docs and the release.

## Open questions

1. **Scope:**
   - **(a) Recommended:** all twelve SRD subclasses at once.
   - **(b)** A first batch of the simplest (Champion, Life, Draconic, Hunter, Thief, Evocation), then the rest.
2. **Levels:**
   - **(a) Recommended:** automate through level 5, and list later features as names.
   - **(b)** Through level 10.
3. **Existing characters who are already past their subclass level:**
   - **(a) Recommended:** a **Choose Subclass…** button on the main window (enabled when one is due), plus a highlight in the table like "ready to level up".
   - **(b)** Only at their next Level Up.
4. **Channel Divinity and Turn Undead:**
   - **(a) Recommended:** include the base **Turn Undead** too: undead fail a WIS save or are *turned* for a minute (here: frightened and unable to attack, until they take damage).
   - **(b)** Only the subclass options (Preserve Life, Sacred Weapon).
5. **Choices inside subclasses:**
   - **(a) Recommended:**
     - Hunter's Prey offers only **Colossus Slayer**. The others need reactions against specific attacks or several attackers in reach.
     - The Draconic ancestor is chosen and stored. It matters at level 6, and shows on the sheet now.
     - The druid's land is chosen, setting the circle spells.
   - **(b)** Skip the ancestor and land choices for now.
6. **Thief's Fast Hands:**
   - **(a) Recommended:** a Thief can drink a potion as a bonus action. That's the common reading of "Use an Object".
   - **(b)** Fast Hands is shown only, as RAW potions take the Drink Potion action.
7. **Berserker exhaustion:**
   - **(a) Recommended:** full exhaustion levels (PHB appendix A):
     1. disadvantage on ability checks (initiative here);
     2. speed halved (shown only);
     3. disadvantage on attacks and saves;
     4. max HP halved;
     5. speed 0 (shown only);
     6. death.

     A long rest removes one level. Frenzy then always asks you to confirm.
   - **(b)** Track the level but apply only level 1–3 effects.
