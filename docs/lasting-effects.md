# Effects that outlast a fight

Status: **built** (2026-10-09): *Mage Armor* and *Aid*.

## Before
Every spell effect lived in the fight's `Encounter` and ended with it. *Mage Armor* and *Aid* last 8 hours, so they were Utility spells: casting one spent the slot and did nothing. A level-1 wizard stayed at AC 12, and since [monster AI](monster-ai.md) shipped, tactical monsters go after spellcasters.

## Decisions
- **Saved on the character.** A `LastingEffects` column (migration 37) holds `"Mage Armor=0;Aid=5"`: the spell's name, and an amount where one matters. The design in [effects-system.md](effects-system.md) planned a `CharacterEffects` table. One column is enough for two spells, the same way exhaustion and temporary hit points are saved.
- **A long rest ends them.** The app doesn't track hours, and 8 hours is a day's adventuring, so the next long rest stands in for the duration. You recast in the morning.
- **A new resolution, `Lasting`.** The spell names its targets like a buff, but the effect goes on the `Character`, not into the fight's effect list (`dnd/lasting_effects.hpp`).
- **Castable in combat and from the Spellbook.** The Spellbook lists the caster and the living characters in the same campaign, with a checkbox for each. Without a campaign, only the caster is listed.

## The rules
- **Mage Armor** (1st level, 1 creature): unarmored AC becomes 13 + DEX. It's one more unarmored base, so a monk's or barbarian's Unarmored Defense still wins when it's higher. It can't go on someone wearing armor, and **equipping armor ends it**. A shield is fine.
- **Aid** (2nd level, up to 3 creatures): max and current HP rise by 5, plus 5 per slot level above 2nd. The bonus is added to `max_hp` directly, the same way Life Drain's loss is taken off. A long rest takes it back off. Two Aids don't stack: a stronger one replaces the weaker (only the difference is added), and one that isn't stronger is refused before the slot is spent.
- Neither can go on the dead, on a druid in beast form, or on NPC allies, since NPCs aren't characters with a saved sheet.

## Where it shows
- **Combat:** the spell hint says it lasts past the fight. The log reads "Zanna: Mage Armor until a long rest (AC 15)." or "Aria: Aid: +5 max HP. Now 17/17 HP." The combatant's AC and HP change at once, and the character is saved.
- **Spellbook:** a target list under the spell table, for *Mage Armor* and *Aid*. Anyone the spell can't affect is greyed out with the reason.
- **Character sheet:** a "Spells in effect" row. The Armor Class breakdown names Mage Armor in place of "Unarmored".
- **Inventory:** equipping armor says "Mage Armor ends."
- **Export:** a `lasting_effects` field, which is optional when importing.

## Later
- Other 8-hour or 1-hour spells (*Longstrider*, *Darkvision*, *Heroism* outside a fight) can use the same column and resolution.
- Real time, if the game ever tracks hours: short rests and travel would then count against the 8 hours.
