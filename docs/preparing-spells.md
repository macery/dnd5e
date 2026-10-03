# Preparing spells and the Wizard's spellbook

Status: **done.** As before, the questions were settled with the recommended answers without a review round (2026-10-02). They're open to revisiting.

**As built:**
- **Data and rules:**
  - Migration 26 adds `CharacterSpells.Prepared` and `Characters.MayPrepare`. `Spell::prepared` comes from `get_character_spells`.
  - `spell_allowance` gives a Wizard its spellbook's size.
  - New: `prepares_spells`, `preparation_refusal`, `prepared_leveled_count`, and `starts_prepared`.
  - `take_long_rest` opens the window, and `Encounter::roll_initiative` closes it. `castable_spells` skips unprepared spells.
- **`dnd/spellbook`:** `add_known_spell` (creation, Level Up, the Spellbook, and import all use it) and `set_prepared`.
- **Exports** carry an optional `unprepared_spells`.
- **Spellbook dialog:**
  - A Prepared column for Wizards ("always" for cantrips), with **Prepare** and **Unprepare** buttons. The counts read "Spellbook: 3 / 6 · Prepared: 2 / 2".
  - For Clerics, Druids, and Paladins, Learn and Forget read **Prepare** and **Unprepare** and are disabled outside the window.
  - A line says whether prepared spells can change now. Casting outside combat needs a prepared spell.
- **Tests:** `tests/test_preparing.cpp` and `tests/ui/test_spellbook_ui.cpp`.

## Where things stand

- **One list per character** (`CharacterSpells`) does every job:
  - for Bards, Rangers, Sorcerers, and Warlocks, it's their spells known;
  - for Clerics, Druids, and Paladins, it's their prepared spells;
  - for Wizards, it's their prepared spells too: there's no spellbook.
- **Learn and Forget in the Spellbook work any time** within the allowance. That lets prepared casters re-prepare in the middle of the day, which the PHB doesn't allow.

## Rules (2014 PHB ch. 10 and 3)

- **Clerics, Druids, and Paladins** prepare from their whole class list: level (half level for Paladins) + casting modifier, minimum 1.
- **Wizards** keep a spellbook: six 1st-level spells at level 1, plus two per level after. They prepare INT modifier + level spells from it.
- **Any of them** can change their prepared spells when they finish a long rest. Cantrips are always ready.

## Decisions (settled as recommended; open to revisiting)

1. **When preparing is allowed:** after a long rest, until the character's next fight. New characters can prepare at once. Starting a fight (rolling initiative) closes the window until the next long rest. The Spellbook says why when it's closed.
2. **Prepared casters (Cleric, Druid, Paladin):** their list stays the prepared list. Learn and Forget in the Spellbook become **Prepare** and **Unprepare**, and they're allowed only in that window.
3. **Wizards:**
   - `CharacterSpells` is the spellbook, and a new `Prepared` flag marks the prepared ones.
   - The allowance for new spells is the spellbook's size, so creation picks 6 and Level Up adds 2 per level. That's PHB-exact.
   - Prepared count is INT + level. New spellbook spells are prepared automatically while there's room. **Prepare** and **Unprepare** toggle the rest, in the window.
   - Copying spells from scrolls into the book is a follow-up.
4. **Known casters** (Bard, Ranger, Sorcerer, Warlock) are unchanged.
5. **Combat** offers only prepared spells; cantrips are always prepared.
6. **Existing characters:** every spell they have is prepared, and Wizards start with their current list as both spellbook and prepared. They can Learn up to the spellbook's size.

## Data model

Migration 26:
- `CharacterSpells.Prepared`, default 1.
- `Characters.MayPrepare`, default 1.
