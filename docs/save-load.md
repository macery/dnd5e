# Save/load (export and import characters) design

Status: **design settled 2026-10-01 (see [Decisions](#decisions)); phase 1 next.**

## Where things stand

- **One save file per machine:** everything lives in one SQLite database (`~/.local/share/dnd5e/DnD Engine/dnd.db` on Linux). Characters can't be backed up on their own, shared with a friend, or moved to another machine except by copying the whole file by hand.
- **Characters now carry real progress:** levels, Ability Score Improvements, Fighting Styles, gear, known spells, spent slots and feature uses. Losing or being stuck with one database matters more than it did.
- **Database IDs aren't portable:** classes, races, alignments, items, and spells are stored by ID. IDs come from seed order and migrations, so they can differ between databases (and versions).

## Goals

- **Export** a player and all of their characters, or a single character, to a readable file.
- **Import** that file into another database (or the same one), with every character exactly as it was:
  - stats, HP, XP and level;
  - proficiencies, background, and Fighting Style;
  - spent slots, hit dice, and feature uses;
  - the dead flag;
  - inventory (quantities and what's equipped) and known spells.
- **Portable by name:** files refer to classes, races, alignments, items, spells, skills, and abilities by their names, never by database IDs.
- **Versioned:** every file says which format version it is. Older files keep loading after the app changes; newer files are refused with a clear message.
- **Safe imports:** nothing is overwritten without asking. Anything that can't be matched (an item this version doesn't have) is reported, not silently dropped.

## Non-goals for now

- **Syncing** between machines, or cloud storage.
- **Exporting monsters, items, spells, or adventures.** They're seeded data, the same in every database of the same version.
- **Settings** (window sizes) and in-progress fights.
- **Merging two copies** of the same character.

## File format (question 2)

JSON, one file per export, extension `.dnd5e.json`:

```json
{
  "format": "dnd5e-characters",
  "version": 1,
  "exported_at": "2026-10-01T19:30:00Z",
  "app_version": "0.8.1",
  "player": { "name": "Adam" },
  "characters": [
    {
      "name": "Aria",
      "class": "Fighter",
      "race": "Human",
      "alignment": "Lawful Good",
      "background": "Soldier",
      "level": 5,
      "experience": 6500,
      "abilities": { "Strength": 17, "Dexterity": 13, "Constitution": 15,
                     "Intelligence": 9, "Wisdom": 13, "Charisma": 11 },
      "hp": { "current": 44, "max": 44 },
      "hit_dice_used": 0,
      "dead": false,
      "saving_throws": ["Strength", "Constitution"],
      "skills": ["Athletics", "Intimidation", "Perception", "Survival"],
      "fighting_style": "Defense",
      "spell_slots_used": [0, 0, 0, 0, 0, 0, 0, 0, 0],
      "feature_uses_spent": { "SecondWind": 1 },
      "inventory": [
        { "item": "Chain Mail", "quantity": 1, "equipped": true },
        { "item": "Longsword", "quantity": 1, "equipped": true },
        { "item": "Potion of Healing", "quantity": 2, "equipped": false }
      ],
      "spells": []
    }
  ]
}
```

- **A single-character export** is the same format with one entry in `characters`.
- **Skill and saving-throw proficiencies** are lists of names rather than the database's bitmasks, so reordering the enums can never scramble them.

## Rules and code (in `dndcore`, with tests)

**`dnd/character_io`:**
- `CharacterExport` / `ExportedCharacter`: the file's contents as plain structs, independent of any database.
- `std::string to_json(const CharacterExport&)` and `ParseResult parse_character_file(const std::string&)`. Parsing returns the export, or a clear error ("not a character file", "made by a newer version of the app (format 3); update to import it", "line 12: …").
- **Versions:** `upgrade(json, from_version)` steps old files forward one format version at a time, like the database migrations. Version 1 needs none yet.
- `export_player(repo, player_id)` and `export_character(repo, character_id)` gather everything by name.
- `ImportPlan plan_import(repo, const CharacterExport&)` resolves names against the database. For each character it reports:
  - whether it can be imported (its class, race, and alignment exist);
  - name conflicts with existing characters;
  - unmatched items and spells, which are skipped with a warning.
- `ImportResult apply_import(repo, plan, choices)` adds or replaces characters, inventory, and spells in one transaction, so a failure leaves the database unchanged.

**JSON library:**
- [nlohmann/json](https://github.com/nlohmann/json), fetched by CMake like Catch2 (header-only, MIT).
- `dndcore` stays free of Qt, so the format is fully testable with Catch2 (question 2).

## UI

- **Main window:** **Export…** and **Import…** buttons on the player row.
  - **Export** asks: this player and all their characters, or just the selected character. Then a save dialog.
  - **Import** opens a file, then a preview:
    - the characters, with class and level;
    - conflicts, each with a choice (question 3);
    - warnings for anything that won't come across (an unknown item).

    **Import** applies it and reloads.
- **Which player imports land in:** question 4.
- **Whole-database backup:** question 1.

## Build phases

1. **Format and core:**
   - `dnd/character_io`: structs, JSON in and out, versioning, export from a repository, import planning and applying (transactional).
   - Round-trip tests: export a fully built character, import it into a fresh in-memory database, and compare everything.
   - Plus tests for conflicts, unknown names, bad files, and newer versions.
2. **UI and docs:** Export and Import in the main window, the import preview with conflict choices, backup if chosen, an offscreen walkthrough, and README updates.

## Decisions

Settled 2026-10-01; every recommendation was taken:

1. **Scope:** export a player with all their characters, or a single character (the same format), plus a **Back up database** button that copies the whole SQLite file.
2. **Format:** JSON through nlohmann/json in `dndcore`, tested with Catch2.
3. **Conflicts:** ask per character. **Import as a copy** is the default; **Replace** and **Skip** are the alternatives.
4. **Target player:** the file's player, matched by name or created. The preview can switch it to the selected player.
5. **What's exported:** the complete current state.
6. **Unknown items and spells** are skipped and listed in the preview.

## Open questions

1. **Scope:**
   - **(a) Recommended:** export a player with all their characters, or a single character (same file format), plus a separate **Back up database** button that copies the whole SQLite file.
   - **(b)** Per-player only.
   - **(c)** Whole-database only.
2. **Format:** JSON through nlohmann/json in `dndcore` (recommended), or Qt's JSON classes in the app layer (no new dependency, but the format then can't be tested by the core tests)?
3. **Name conflicts on import:**
   - **(a) Recommended:** ask per character, with **Import as a copy** as the default ("Aria (2)"), **Replace** (overwrite the existing one), or **Skip**.
   - **(b)** Always import as a copy.
4. **Target player:**
   - **(a) Recommended:** the player named in the file; matched by name, or created if missing. The preview can switch it to the currently selected player instead.
   - **(b)** Always into the selected player.
5. **What to export:** the complete current state (HP, spent slots and feature uses, the dead flag) (recommended), or a "fresh" copy rested to full?
6. **Unknown items or spells on import:** skip them and list them in the preview (recommended), or refuse the whole character?
