# dnd5e

A desktop companion and solo-play engine for **Dungeons & Dragons 5th Edition**, written in C++20 with a Qt 6 UI and a SQLite save file.

It manages players and characters, computes a full character sheet from the 5e rules, and runs turn-based combat. You play your own characters; the monsters take their turns automatically. Short pre-built solo adventures give you something to jump straight into, and characters earn XP, level up, and rest between fights.

> This is a learning/practice project. There's no fixed spec, so expect features to grow in experimental steps.

---

## Features

- **Players & characters**: create, edit, and delete players and their characters (class, race, alignment, level, ability scores, HP, AC, proficiencies, equipped weapon).
- **Character sheet**: every derived stat is computed from the 5e rules, never stored: ability modifiers, proficiency bonus, saving throws, skills, initiative, passive Perception, and weapon attack/damage. Saves and skills can be rolled directly, with advantage or disadvantage.
- **Dice roller**: any number of dice with any number of sides, plus d20 rolls with advantage/disadvantage.
- **HP quick-adjust**: Damage and Heal buttons for the selected character.
- **Initiative tracker**: a standalone turn-order tracker.
- **Monsters**: a seeded list of low-CR enemies (Kobold, Goblin, Skeleton, Wolf, Zombie, Orc) with stat blocks and attacks.
- **Weapons**: a seeded weapon list with finesse/ranged handling (finesse weapons use the better of STR or DEX).
- **Combat encounters**:
  - Real initiative order.
  - Monster turns resolve automatically.
  - On each of your turns you pick Attack, Dash, Dodge, or Drink Potion.
  - Natural 20s crit and natural 1s miss.
  - Dodge imposes disadvantage on attacks against you.
  - Drink Potion heals from your inventory.
  - The encounter ends with a victory or a defeat.
- **Solo adventures**: pre-built mini-encounters, each with a hook, its monsters, a victory payoff, and loot.
- **Inventory**:
  - **Carrying items:** each character carries stacks of seeded items: adventuring gear, rations, and Potions of Healing (2d4+2). The Inventory screen shows total weight and value.
  - **Healing:** potions can be used from the Inventory screen, or drunk in combat with the **Drink Potion** action.
  - **Loot:** adventure rewards go into the inventory of the first character still standing.
- **Progression**:
  - **XP:** each monster's XP comes from its challenge rating. On a victory the total is split evenly across the party.
  - **Leveling up:** crossing an XP threshold levels the character up automatically. Each level adds fixed-average HP.
  - **Short rest:** spend a hit die to heal.
  - **Long rest:** restore all HP and recover up to half the spent hit dice.
- **In-app rules help**: tooltips explain abilities, skills, and combat terms for new players.

## Repository layout

```
dnd5e/
├── cpp/                  # The current application (C++20 / Qt 6 / SQLite)
│   ├── include/dnd/      # dndcore public headers (UI-agnostic rules & data)
│   ├── src/              # dndcore implementation
│   ├── app/              # Qt Widgets desktop app (dnd_app)
│   ├── tests/            # Catch2 unit tests for dndcore
│   └── CMakeLists.txt
├── DND5EProject/         # Legacy C# / .NET (WPF, MvvmCross, MAUI) version, no longer developed
└── .github/workflows/    # CI: builds and tests cpp/ on Linux and Windows
```

### Architecture (`cpp/`)

| Module | Purpose |
|---|---|
| `dice` / `rng` | The dice engine. Randomness comes from an injected `IRandomSource`, so tests are deterministic. |
| `rules` | Pure 5e rules math: modifiers, proficiency, skills, saves, weapon attack bonuses. |
| `combat` | Attack resolution (hit/miss, crits, advantage/disadvantage). Knows nothing about characters or monsters. |
| `inventory` | The item model (`item.hpp`), healing consumables, weight and cost helpers. |
| `progression` | XP tables, leveling, class hit dice, and short/long rests. |
| `adventure` | Built-in solo adventure templates. |
| `repository` | SQLite persistence for players, characters, inventories, and the lookup tables. |
| `migrations` | Versioned, append-only schema migrations. They run automatically when the database is opened. |
| `app/` | Qt dialogs and widgets. They sit on top of `dndcore` and hold no rules logic. |

`dndcore` has no Qt dependency. The library and its tests build even on a machine without Qt; in that case the UI target is skipped.

## Building

### Requirements

- CMake ≥ 3.16
- A C++20 compiler (GCC, Clang, or MSVC)
- SQLite3 development headers
- Qt 6 Widgets, needed only for the desktop app
- An internet connection on first configure, because Catch2 is fetched automatically

On Fedora:

```sh
sudo dnf install cmake gcc-c++ sqlite-devel qt6-qtbase-devel
```

On Ubuntu/Debian:

```sh
sudo apt install cmake g++ libsqlite3-dev qt6-base-dev
```

### Build, test, run

```sh
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build --parallel
ctest --test-dir cpp/build --output-on-failure
./cpp/build/app/dnd_app
```

On Windows, CI installs SQLite through vcpkg; see `.github/workflows/cpp-ci.yml` for the exact configure command.

### Portable Linux build (AppImage)

`cpp/packaging/build-appimage.sh` builds `DnD_Engine-x86_64.AppImage`, a single file that bundles Qt and runs without installing anything. It builds inside an Ubuntu 24.04 container so the result runs on Ubuntu 24.04 and newer distros. An AppImage only runs where glibc is at least as new as on the build system, and Ubuntu 24.04 also ships an older Qt than Fedora does. From the repo root:

```sh
podman run --rm -v "$PWD":/src:Z -w /src docker.io/library/ubuntu:24.04 cpp/packaging/build-appimage.sh
```

The script runs the tests, then writes the AppImage to `cpp/packaging/out/`. To use it on another machine:

```sh
scp cpp/packaging/out/DnD_Engine-x86_64.AppImage other-machine:~/
# on the other machine:
chmod +x DnD_Engine-x86_64.AppImage && ./DnD_Engine-x86_64.AppImage
```

**From CI:** the `AppImage` workflow (`.github/workflows/appimage.yml`) runs the same script on every push to `main`, on pull requests that touch `cpp/`, and on demand from the Actions tab.
- **Every run:** the AppImage is uploaded as a workflow artifact. Download it from the run's page. GitHub delivers artifacts zipped, so unzip it and `chmod +x` it before running.
- **Releases:** pushing a version tag attaches the AppImage to a GitHub release:
  ```sh
  git tag v0.1.0 && git push origin v0.1.0
  ```
  The release notes are generated by `cpp/packaging/release-notes.sh`: the commit messages since the previous `v*` tag, plus install instructions. Run it locally to preview them, and edit them on GitHub afterwards if you like.

Things to know:
- **FUSE:** AppImages need FUSE 2. If it won't start on Ubuntu, either `sudo apt install libfuse2t64`, or run it with `APPIMAGE_EXTRACT_AND_RUN=1 ./DnD_Engine-x86_64.AppImage`.
- **OpenGL:** it expects the desktop's graphics libraries. On a minimal install, `sudo apt install libopengl0` provides the one most likely to be missing.
- **Display:** only the X11 platform plugin is bundled. On a Wayland session it runs through XWayland.
- **Save data:** each machine has its own database. Characters don't move between machines yet; that's the save/load TODO.

### Where your data lives

The app stores its SQLite database in the per-user data directory that Qt reports for this app, for example:

- Linux: `~/.local/share/dnd5e/DnD Engine/dnd.db`
- Windows: `%APPDATA%\dnd5e\DnD Engine\dnd.db`

Any pending schema migrations run on launch, so older save files are upgraded in place.

## Quick start (playing)

1. Type a player name and click **Add Player**.
2. Click **Add Character** and fill in their class, stats, HP, AC, and weapon.
3. Double-click a character to open their **character sheet**.
4. Click **Solo Adventure**, pick one, then **Roll Initiative** and fight. Use **Next Turn** to hand play back to the monsters.
5. Win to earn XP and loot. Use **Inventory** to see or use items. Afterwards, use **Short Rest** or **Long Rest** to recover before the next fight.

## Development notes

- **Schema changes:** append a new entry to `kMigrations` in `cpp/src/migrations.cpp`. Never edit an existing migration, because it has already run on real databases.
- **Rules logic:** put it in `dndcore`, not in `app/`, and cover it with a Catch2 test. Pass a fake `IRandomSource` to get deterministic dice.
- **Tests:** tests use `Repository(":memory:")` for a throwaway database.

---

## TODO

### Inventory & items

- [x] **Inventory system**: an `Items` table plus a per-character `CharacterInventory` table (item, quantity), with the matching repository methods and a migration.
- [x] **Basic items**: seed common adventuring gear (rations, rope, torches, bedroll, waterskin, thieves' tools) and consumables (Potion of Healing: 2d4+2).
- [x] **Inventory UI**: an Inventory dialog (opened from the main window) to add, drop, and use items.
- [x] **Use items in combat**: drinking a potion as an action (or a bonus action, depending on the rules variant).
- [ ] **Equipping & weapons in the inventory**: add an equipped flag to inventory entries, and make the equipped weapon one of the character's inventory items instead of the single `WeaponID` column.
- [ ] **Armor & shields**: seed armor (light/medium/heavy) and calculate AC from the equipped armor, DEX (capped for medium armor), and a shield, instead of entering AC by hand.
- [ ] **Currency**: track CP/SP/EP/GP/PP and have adventures award gold alongside XP.
- [ ] **Encumbrance** (optional): item weights compared with carrying capacity (STR × 15).

### Magic system

Planned after the character creation wizard. The wizard adds the per-class rules data this builds on, and it will gain a "Choose spells" step once spells exist.

- [ ] **Design the approach**: decide what needs to be data (a `Spells` table: level, school, casting time, range, components, duration, damage/healing dice, save ability, attack or save) and what needs code (special effects).
- [ ] **Spellcasting stats**: each class's spellcasting ability, spell save DC (8 + proficiency + modifier), and spell attack bonus, added to `rules`.
- [ ] **Spell slots**: slots per level by class and level, spent when casting and restored on a long rest (Warlock pact slots on a short rest).
- [ ] **Known/prepared spells**: a per-character spell list, plus a spellbook UI on the character sheet.
- [ ] **Starter spell list**: cantrips (Fire Bolt, Sacred Flame, Eldritch Blast) and level-1 spells (Magic Missile, Cure Wounds, Healing Word, Shield, Burning Hands).
- [ ] **Cast a Spell in combat**: a new action, covering spell attack rolls and saving throws (half damage on a successful save).
- [ ] **Concentration**: at most one concentration spell at a time; taking damage forces a CON save to keep it.
- [ ] **Scaling**: cantrip damage grows at levels 5, 11, and 17; casting with a higher-level slot increases the effect.
- [ ] **Magic items**: +1 weapons and armor, wands, scrolls, and so on, built on top of the inventory system.

### Characters

- [ ] **Character creation wizard** (next up): a step-by-step `QWizard` that builds a complete, rules-valid level-1 character, instead of the current single form where everything is typed by hand. Several items below are steps of it.
  1. **Class:** sets the hit die and saving-throw proficiencies, and which skills you can choose from and how many (e.g. a Rogue picks 4).
  2. **Race:** ability score increases, speed, and traits.
  3. **Ability scores:** roll 4d6 drop the lowest, use point buy, or take the standard array, then assign each value to an ability, with racial bonuses shown.
  4. **Background:** two fixed skill proficiencies.
  5. **Skills:** choose from the class list; background skills are pre-selected and locked.
  6. **Details:** name and alignment.
  7. **Equipment:** a starting weapon, plus a starting gear pack that goes straight into the inventory.
  8. **Review:** a preview of the finished character sheet (HP from hit die + CON, AC, all bonuses) before saving.

  It needs new per-class rules data: saving throws, skill choices, hit die, and later the spellcasting ability. The existing editor can also use that data to check proficiencies.
- [ ] **Ability score generation**: 4d6 drop the lowest, point buy, and the standard array in the character editor.
- [ ] **Race traits**: ability score increases, speed, darkvision, and racial features.
- [ ] **Class features**: a few defining ones, e.g. Fighter's Second Wind and Action Surge, Rogue's Sneak Attack, Barbarian's Rage.
- [ ] **Level-up choices**: Ability Score Improvements at levels 4/8/12/16/19, and an option to roll HP instead of taking the average.
- [ ] **Starting HP**: calculate level-1 max HP from the class hit die plus CON instead of entering it by hand.
- [ ] **Keep level and XP in sync**: editing the level by hand in the editor can leave it out of step with XP.
- [ ] **Weapon proficiencies**: attack bonuses currently assume the character is proficient with every weapon.
- [ ] **Backgrounds**: skill proficiencies plus flavor.

### Combat

- [ ] **Conditions**: prone, poisoned, frightened, restrained, stunned, etc., wired into advantage and disadvantage.
- [ ] **Death saving throws**: characters at 0 HP roll death saves, get stabilized, or die, instead of simply being "Down".
- [ ] **More actions**: Disengage, Help, Hide, Ready, and Use an Object (for potions).
- [ ] **Monsters with several attacks**: Multiattack and more than one attack per stat block.
- [ ] **Smarter monster AI**: target selection better than a random living character (lowest HP, closest, etc.).
- [ ] **Damage types**: resistances, vulnerabilities, and immunities (e.g. skeletons are vulnerable to bludgeoning).
- [ ] **Save log**: save the combat log or an encounter summary when the encounter ends.

### Content

- [ ] **More monsters**: more stat blocks, including CR 1–5 enemies for higher-level parties.
- [ ] **Monster editor**: add and edit custom monsters in the UI.
- [ ] **Multi-encounter adventures**: several fights in a row, with rests in between.
- [ ] **Encounter difficulty**: rate encounters Easy/Medium/Hard/Deadly from party XP budgets, and scale adventures to the party's level.
- [x] **Loot**: adventure rewards that drop items into inventory (depends on the inventory system).

### Technical / housekeeping

- [ ] **Stale comment in `app/main.cpp`**: it still says there's "no schema migration system yet", which is no longer true.
- [ ] **Qt UI tests**: add some tests for the dialogs, or move more of their logic into `dndcore` where it can be tested.
- [x] **Linux AppImage**: a local build script (`cpp/packaging/build-appimage.sh`).
- [x] **AppImage in CI**: built on every push and attached to releases for `v*` tags.
- [ ] **More packaging**: a Windows installer, and maybe Flatpak.
- [ ] **Wayland plugin**: bundle Qt's Wayland platform plugin so the app runs natively on Wayland instead of through XWayland.
- [ ] **Code style**: add `clang-format` / `clang-tidy` configs and check them in CI.
- [ ] **Legacy code**: decide whether to archive or remove the legacy `DND5EProject/` C# code.
- [ ] **Per-player save/load**: export a player's characters (stats, XP, inventory) to a file and import them into another database, for backups, sharing, and moving between machines. It starts per-player, but the scope still needs working through. Open questions:
  - **Scope:** per player, per character, or the whole database?
  - **Format:** JSON (readable, diff-friendly), or a copy of the SQLite file (simpler, but all-or-nothing)?
  - **Conflicts:** what happens on import when a name already exists? Rename, overwrite, or skip?
  - **Stable IDs:** how to map class, race, item and weapon IDs between databases, for example by exporting names instead of IDs.
  - **Versioning:** how to version the file format so older exports still load after schema changes.

## License

MIT. See [LICENSE](LICENSE).
