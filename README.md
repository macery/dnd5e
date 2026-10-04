# dnd5e

A desktop companion and solo-play engine for **Dungeons & Dragons 5th Edition**, written in C++20 with a Qt 6 UI and a SQLite save file.

It manages players and characters, computes a full character sheet from the 5e rules, and runs turn-based combat. You play your own characters; the monsters take their turns automatically. Short pre-built solo adventures give you something to jump straight into, and characters earn XP, level up, and rest between fights.

> This is a learning/practice project. There's no fixed spec, so expect features to grow in experimental steps.

---

## Features

- **Players & characters**: create, edit, and delete players and their characters.
  - The main window lists every character in play, meaning the active campaign's (or everyone's, with no campaign), with a Player column.
  - The player list filters it to one player, and **All players** is the default.
- **Character creation wizard**: a step-by-step guide that builds a rules-valid level-1 character:
  - **Class:** sets the hit die, saving throws, and skill choices.
  - **Race:** adds ability bonuses, with a choice of +1s for Half-Elves.
  - **Ability scores:** standard array, roll 4d6 drop the lowest, or 27-point buy.
  - **Background:** grants two skill proficiencies.
  - **Skills:** class picks and free picks, checked against the rules.
  - **Name and alignment.**
  - **Starting equipment:** the class's PHB kit (e.g. a Fighter's chain mail, shield, longsword, and light crossbow), a choice of weapon to wield, and the Explorer's Pack.
  - **Review:** the finished sheet, before saving. HP, AC, and the weapon attack are calculated for you.

  Editing an existing character still uses the free-form editor.
- **Character sheet**: every derived stat is computed from the 5e rules, never stored: ability modifiers, proficiency bonus, saving throws, skills, initiative, passive Perception, and weapon attack/damage. Saves and skills can be rolled directly, with advantage or disadvantage.
- **Dice roller**: any number of dice with any number of sides, plus d20 rolls with advantage/disadvantage.
- **HP quick-adjust**: Damage and Heal buttons for the selected character.
- **Initiative tracker**: a standalone turn-order tracker.
- **Monsters**: a seeded list of low-CR enemies (Kobold, Goblin, Skeleton, Wolf, Zombie, Orc) with stat blocks and attacks.
- **Weapons, armor & AC** (2014 PHB ch. 5): 16 weapons, 12 armors, and a shield, all ordinary inventory items.
  - **Equipping:** equip them from the Inventory: one armor, one shield, and one weapon at a time; two-handed weapons and shields don't mix.
  - **AC is always calculated:** from armor (light adds DEX, medium up to +2, heavy none), a shield, and Barbarian/Monk Unarmored Defense. The sheet shows the breakdown.
  - **Attacks** use the wielded weapon (finesse, ranged, versatile, unarmed strike), adding proficiency only with weapons your class or race is trained in.
  - **Untrained armor** gives disadvantage on STR/DEX rolls and blocks spellcasting.
  - **Heavy armor** without enough STR costs 10 ft of speed.
- **Combat encounters**:
  - Real initiative order.
  - Monster turns resolve automatically.
  - On each of your turns you pick Attack, Dash, Dodge, Drink Potion, Cast a Spell, or Stabilize.
  - **Conditions and effects:** all 14 PHB conditions, plus spell effects like *Bless*, *Guiding Bolt*, and *Charm Person*. They change advantage and disadvantage, saves, AC, and who can act. Hover a creature's status for what each one does and how long it lasts.
  - **Death saving throws:** characters at 0 HP are dying and roll each turn. Stabilize them, heal them, or lose them. The dead stay dead until a temple raises them (see *Shop*).
  - **Concentration:** one spell at a time, and damage can break it.
  - **Actions and class features:** one action and one bonus action per turn, plus class features:
    - **Barbarian:** Rage, Reckless Attack.
    - **Fighter:** Fighting Style (Archery, Defense, Dueling, Great Weapon Fighting), Second Wind, Action Surge.
    - **Rogue:** Sneak Attack, Cunning Action.
    - **Bard:** Bardic Inspiration.
    - **Monk:** Martial Arts, Ki (Flurry of Blows, Patient Defense, Step of the Wind).
    - **Paladin:** Lay on Hands, Divine Smite.
    - **Level 5:** Extra Attack.

    Bonus-action spells follow the PHB rule.
  - Natural 20s crit and natural 1s miss.
  - Dodge imposes disadvantage on attacks against you.
  - Drink Potion heals from your inventory.
  - The encounter ends with a victory or a defeat.
- **Campaigns** ([docs/campaigns.md](docs/campaigns.md)): a table that plays together, with characters from any players.
  - Choose the active campaign at the top of the main window, and build rosters with **Campaigns…**.
  - While one is active, fights, adventure parties, trading, and the Temple use only its characters, and its adventures resume separately.
  - Imports can join the active campaign. "No campaign" opens everything, as before.
- **Magic items** ([docs/magic-items.md](docs/magic-items.md), 2014 DMG):
  - **+1/+2/+3 weapons, armor, and shields:** bonuses to attack and damage, or to AC.
  - **Healing potions** from Healing to Supreme.
  - **Wondrous items:** Cloak and Ring of Protection, Bracers of Defense, Gauntlets of Ogre Power, and Amulet of Health.
  - **Attunement:** one item per short rest (chosen in the Short Rest dialog), three at most.
  - **Spell scrolls:** a **Read Scroll** action in combat. Only classes with the spell on their list can read one, a spell above your highest slot needs a check, and the scroll's own DC and attack bonus apply.
  - **Where they come from:** boss fights drop them, and the Shop sells the common and uncommon ones.
- **Shop** ([docs/shops.md](docs/shops.md)): buy gear at PHB list price (with change made automatically) and sell it for half; equipped gear must be unequipped first. Give coins, or items from the Inventory, to any other living character. A temple raises the dead for 550 gp; they return with 1 HP and −4 to attack rolls, saving throws, and ability checks, easing by 1 each long rest.
- **Solo adventures** ([docs/adventures-and-currency.md](docs/adventures-and-currency.md)): twelve pre-built adventures. Nine are single fights; three are multi-fight runs (The Goblin Warren, Crypt of the Restless, The Hill Giant's Toll).
  - **Party:** you pick who goes.
  - **Fights:** each comes with narration, is scaled for the party (the final fight toward Hard), and pays coins and loot.
  - **Loot:** coins are split evenly. With two or more characters standing, a **Divide the Loot** panel on the victory screen picks who takes each item: healing potions start with the most hurt, and the rest is spread around. Closing the window hands it out as shown.
  - **Between fights:**
    - press on;
    - take a short rest, with hit dice chosen per character;
    - take a long rest at the adventure's camps, with Level Up for anyone ready;
    - or retreat, keeping what you've earned.
  - **Saving:** progress is saved between fights, and Solo Adventure offers to resume.
- **Export, import, and backup** ([docs/save-load.md](docs/save-load.md)):
  - **Export…** saves a player's characters, or just one, to a readable `.dnd5e.json` file: stats, XP, proficiencies, spent resources, inventory, and spells, all by name.
  - **Import…** previews the file before anything changes: which player the characters go to, a choice for any name clash (import as a copy, replace, or skip), and anything this version doesn't have.
  - **Back Up Database…** saves a complete copy of everything.
- **Spellcasting** (2014 PHB rules):
  - **Spells:** 42 seeded spells. 18 are resolved automatically: spell attacks, saving throws (half or no damage on a success), always-hit spells, and heals. 24 are utility spells with no automated effect yet.
  - **Creation:** Bards, Clerics, Druids, Sorcerers, Warlocks, and Wizards choose their cantrips and spells in the character creation wizard.
  - **Character sheet:** shows the spellcasting ability, spell attack bonus, save DC, and remaining slots.
  - **Spellbook:** learn and forget spells within your class's limits (this is also how you pick new spells after leveling up), and cast healing or utility spells on yourself outside combat.
  - **Combat:** a **Cast a Spell** action with slot choice for upcasting, and multiple targets for area spells and Magic Missile.
  - **Rests:** spell slots come back on a long rest; Warlock pact slots also come back on a short rest.
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
| `encounter` | The combat engine: initiative and turns, monster AI, the Attack/Dash/Dodge/Drink Potion/Cast a Spell actions, victory and defeat, XP and loot. It reports log lines and what to persist as events, and never touches the database. |
| `inventory` | The item model (`item.hpp`), healing consumables, weight and cost helpers. |
| `character_creation` | Class, race, and background rules data; ability score generation; skill plans; validation; and assembling a level-1 character. |
| `spellcasting` | Spell model (`spell.hpp`), class spellcasting stats, spell counts, slot tables, and `cast_spell`. |
| `progression` | XP tables, leveling, class hit dice, and short/long rests. |
| `adventure` | Built-in solo adventure templates. |
| `repository` | SQLite persistence for players, characters, inventories, and the lookup tables. |
| `migrations` | Versioned, append-only schema migrations. They run automatically when the database is opened. |
| `app/` | Qt dialogs and widgets. They sit on top of `dndcore` and hold no rules logic. The combat dialog, for example, is a view over `dnd::Encounter`. |

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

`ctest` runs two programs:
- **`dnd_tests`:** the core rules, with Catch2.
- **`dnd_ui_tests`:** offscreen UI tests, built when Qt is available. They open the app's windows against an in-memory database on Qt's offscreen platform and click through them. They check that every window fits an 800×600 screen, and they cover Level Up, the creation wizard, the editor, the character sheet's columns, the import preview, rests, the combat controls, and solo adventures. To run them alone: `QT_QPA_PLATFORM=offscreen ./cpp/build/tests/ui/dnd_ui_tests`.

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
- **Save data:** each machine has its own database. To move characters, use **Export…** on one machine and **Import…** on the other (see Features).

### Where your data lives

The app stores its SQLite database in the per-user data directory that Qt reports for this app, for example:

- Linux: `~/.local/share/dnd5e/DnD Engine/dnd.db`
- Windows: `%APPDATA%\dnd5e\DnD Engine\dnd.db`

Any pending schema migrations run on launch, so older save files are upgraded in place.

## Quick start (playing)

1. Type a player name and click **Add Player**.
2. Click **Add Character** and follow the wizard: class, race, ability scores, background, skills, name, and equipment.
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
- [x] **Equipment design**: see [docs/equipment-system.md](docs/equipment-system.md). Phases 1 (data and rules) and 2 (combat, Inventory, sheet, wizard) are done. It covers the next two items and *Weapon proficiencies*, with its decisions settled: calculated AC, warnings plus combat penalties for armor you aren't proficient with, fixed starting kits per class, weapon proficiency included, and existing weapons converted to inventory items.
- [x] **Equipping & weapons in the inventory**: weapons are inventory items, and the Inventory dialog equips one armor, one shield, and one weapon at a time. The old `WeaponID` column is retired.
- [x] **Armor & shields**: the PHB armor table and the shield are seeded. AC is calculated from the worn armor, DEX (capped for medium), a shield, and Unarmored Defense, and is never entered by hand.
- [x] **Currency** ([docs/adventures-and-currency.md](docs/adventures-and-currency.md)):
  - Characters carry cp, sp, ep, gp, and pp, and start with their background's purse.
  - Adventures pay coins scaled to the party's level, split among the survivors.
  - Coins show on the sheet and in the Inventory (with what the items are worth), and travel in exports.
- [x] **Shops and spending** ([docs/shops.md](docs/shops.md)): buy gear at list price and sell it for half, give coins between a player's characters, and pay a temple 550 gp to *raise dead*. The Inventory's **Add (free)** stays for loot and DM gifts.
- [ ] **Coins from Combat Encounter fights** (open question): fights built with the main window's **Combat Encounter** award XP only; coins and loot come only from Solo Adventure fights. One option is the 2014 DMG's *Individual Treasure* table: each defeated monster rolls coins by challenge rating (CR 0–4: d100 for 5d6 cp, 4d6 sp, 3d6 ep, 3d6 gp, or 1d6 pp), logged and split among the survivors. Undecided: whether free-form fights should pay at all, and whether to use the DMG tables or a simpler scale.
- [x] **Divide loot after a victory**: a **Divide the Loot** panel on the combat screen's victory, with a "goes to" choice per item (per unit for small stacks). Potions start with the most hurt, and everything else goes to whoever has received least. A lone survivor takes it all at once. Later moves use the Inventory's **Give…**.
- [ ] **Encumbrance** (optional): item weights compared with carrying capacity (STR × 15).
- [ ] **Item uses**: a Healer's Kit has 10 uses in the PHB, but they aren't tracked, so stabilizing with one never uses it up. The same applies to other limited-use gear later (torches burning out, rations eaten).

### Magic system

Phases 1 (core and data) and 2 (UI) are done; see [docs/magic-system.md](docs/magic-system.md). Most of phase 3 was built as part of the [effects system](docs/effects-system.md): spell side effects, *Bless*-style buffs, concentration, and *Spare the Dying*. The unchecked items below are what's left.

- [x] **Design the approach**: see [docs/magic-system.md](docs/magic-system.md). It covers what's data and what's code, the `Spells` schema, class spellcasting rules, the starter spell list, three build phases, and the settled decisions (2014 PHB rules, "choose up to N targets" for area spells, spells cast as actions, no monster casters yet).
- [x] **Spellcasting stats**: each class's spellcasting ability, caster type, cantrip and spell counts, spell save DC, and spell attack bonus (`dnd/spellcasting.hpp`), shown on the sheet and in the Spellbook.
- [x] **Spell slots**: full, half, and pact slot tables for levels 1–20, spent by casting. Long Rest restores every slot; Short Rest restores Warlock pact slots.
- [x] **Known/prepared spells**: the wizard's "Choose Spells" step, the Spellbook dialog (learn, forget, cast out of combat), and a Spellcasting section on the character sheet. Prepared casters keep the spells they picked; re-preparing after a long rest is phase 3.
- [x] **Starter spell list**: 18 spells with automated effects (migration 11), plus 24 *Utility* spells with no automated effect yet (migration 12), so every caster can fill its PHB level-1 picks. *Shield* came with reactions (below).
- [x] **2nd- and 3rd-level spells** ([docs/spells-2-3.md](docs/spells-2-3.md)):
  - Automated: *Scorching Ray*, *Shatter*, *Hold Person* and *Blindness/Deafness* (a save at the end of each turn), *Fireball*, *Lightning Bolt*, *Mass Healing Word*, and *Revivify* (someone who died in this fight; uses up Diamonds (300 gp), sold in the Shop).
  - 28 more as Utility, so every class has picks through level 5.
  - Scrolls of the new automated spells.
- [ ] **More spells (4th level and up), and the map-bound 2nd–3rd ones**: *Spiritual Weapon*, *Spirit Guardians*, *Moonbeam*, *Call Lightning*, *Haste*, *Hypnotic Pattern*, and *Aid* are Utility for now.
- [x] **Cast a Spell in combat**: pick a spell, a slot level, and targets. Monsters make saving throws with their ability modifiers, and everything is logged. Conditions, Dodge, and *Bless* apply to spell attacks and saves.
- [x] **Spell side effects**: *Bless*, *Shield of Faith*, *Divine Favor*, *Guiding Bolt*, *Ray of Frost*, *Chill Touch*, *Vicious Mockery*, *Faerie Fire*, *Charm Person*, and *Spare the Dying* do what the PHB says (`SpellEffects`, effects phase 3).
- [x] **Concentration**: one spell at a time (casting another ends the first); damage forces a CON save against DC max(10, half the damage); dropping to 0 or dying ends it, along with the spell's effects. *Bless*, *Shield of Faith*, *Divine Favor*, and *Faerie Fire* use it.
- [x] **Scaling**: cantrips get extra dice (or extra beams, for Eldritch Blast) at levels 5, 11, and 17, and casting with a higher-level slot adds dice or darts.
- [x] **Reactions** ([docs/reactions.md](docs/reactions.md)): automatic on monsters' turns. *Shield* when it turns a hit into a miss, a Rogue's Uncanny Dodge, and *Hellish Rebuke*. A combat-screen checkbox turns them off.
- [ ] **More reactions**: opportunity attacks (they need a map), *Counterspell* (monsters don't cast yet), and choosing reactions by hand.
- [x] **Bonus actions**: each turn has one action and one bonus action, shown live in combat. *Healing Word*, *Shield of Faith*, *Divine Favor*, and *Sanctuary* use the bonus action, and the PHB rule applies: after a bonus-action spell, only an action cantrip can follow. Extra Attack gives two attacks per Attack action from level 5. Class-feature bonus actions are next; see [docs/class-features.md](docs/class-features.md).
- [x] **Re-preparing spells** ([docs/preparing-spells.md](docs/preparing-spells.md)):
  - Clerics, Druids, Paladins, and Wizards change prepared spells after a long rest, until their next fight. The Spellbook says when it's open.
  - Wizards keep a real spellbook (6 spells at level 1, plus 2 per level) and prepare INT + level spells from it. Combat offers only prepared spells.
- [ ] **Copying spells into a spellbook**: from scrolls or another wizard's book (PHB: 50 gp and 2 hours per spell level).
- [ ] **Effects that outlast combat**: *Mage Armor* (8 hours; AC 13 + DEX while unarmored) and *Aid*. They need effects saved on the character; today every effect ends with the fight. (Exhaustion and temporary hit points are saved now; see subclasses.)
- [ ] **More utility spells with real effects**: 18 utility spells (*Light*, *Detect Magic*, *Command*, *Sanctuary*, ...) still have no automated effect.
- [x] **Magic items** ([docs/magic-items.md](docs/magic-items.md)): +1/+2/+3 weapons, armor, and shields; four healing potions; five wondrous items with attunement; spell scrolls with the DMG's reading rules; boss loot and the Shop.
- [ ] **Charged magic items**: wands and staffs (a Wand of Magic Missiles), with charges saved per item and recharged each dawn.
- [ ] **Scrolls out of combat**: reading a *Cure Wounds* scroll from the Inventory, like Use for potions. Scrolls are combat-only for now.

### Characters

- [x] **Campaigns** ([docs/campaigns.md](docs/campaigns.md)): rosters from any players, one active campaign that scopes fights, parties, trading, the Temple, and adventure runs, and imports that join it. The original idea: a level above players, so the structure becomes *campaign → players → characters*. Players build characters on their own, then import them into a campaign and choose who plays in each session. Party choice, Give Coins, Give Items, and the Temple's payers would then work within the campaign. Until then, any character can trade with or pay for any other, across players.
- [x] **Character creation wizard**: a step-by-step `QWizard` that builds a complete, rules-valid level-1 character, instead of the current single form where everything is typed by hand. Several items below are steps of it.
  1. **Class:** sets the hit die and saving-throw proficiencies, and which skills you can choose from and how many (e.g. a Rogue picks 4).
  2. **Race:** ability score increases, speed, and traits.
  3. **Ability scores:** roll 4d6 drop the lowest, use point buy, or take the standard array, then assign each value to an ability, with racial bonuses shown.
  4. **Background:** two fixed skill proficiencies.
  5. **Skills:** choose from the class list; background skills are pre-selected and locked.
  6. **Details:** name and alignment.
  7. **Equipment:** a starting weapon, plus a starting gear pack that goes straight into the inventory.
  8. **Review:** a preview of the finished character sheet (HP from hit die + CON, AC, all bonuses) before saving.

  The per-class rules data it added (saving throws, skill choices, hit die, and since the magic system, spellcasting) lives in `dnd/character_creation.hpp`. Still to do: using this data in the existing editor to check proficiencies.
- [x] **Ability score generation**: 4d6 drop the lowest, point buy, and the standard array, in the wizard.
- [ ] **Race traits**: ability score increases and racial skills are applied. Speed and traits (darkvision, Lucky, Relentless Endurance, etc.) are shown but not simulated. Subraces (Hill Dwarf, High Elf, ...) aren't modeled, and speed isn't stored on the character yet.
- [ ] **Class features** ([docs/class-features.md](docs/class-features.md)):
  - **Done:**
    - Rage, Reckless Attack, Sneak Attack, Cunning Action, Second Wind, and Action Surge.
    - Fighting Styles, chosen in the wizard.
    - Bardic Inspiration, Martial Arts, Ki, Lay on Hands, Divine Smite, Arcane Recovery, and Extra Attack.

    Uses are saved on the character and restored by rests.
  - **Later:** Paladin and Ranger Fighting Styles at level 2, two-weapon fighting, subclasses, Warlock invocations, Sorcery Points, reactions, and Divine Smite's extra die against undead and fiends.
- [x] **Level-up choices** ([docs/level-up.md](docs/level-up.md)):
  - XP makes a level available, and **Level Up** on the main window walks through it: roll or average HP, Ability Score Improvements (with retroactive CON), Paladin and Ranger Fighting Styles, and new spells.
  - A summary shows every PHB feature gained, marked where it isn't automated yet.
  - Feats and multiclassing are still to do.
- [x] **Subclasses** ([docs/subclasses.md](docs/subclasses.md)):
  - The twelve SRD subclasses, chosen at the class's level (in Level Up, after creation, or with **Choose Subclass…**), with their features through level 5.
  - Champion crits, Frenzy (with exhaustion), Cutting Words, Colossus Slayer, Open Hand Technique, Fast Hands, Disciple of Life, Dark One's Blessing, Draconic Resilience, and Natural Recovery.
  - Channel Divinity (Turn Undead, Destroy Undead, Preserve Life, Sacred Weapon, Turn the Unholy), and always-prepared domain, oath, and circle spells.
- [ ] **More subclasses and higher-level subclass features**: the PHB's other 28 subclasses (not in the SRD), and subclass features above level 5.
- [x] **Starting HP**: level-1 max HP is the class hit die plus CON, calculated by the wizard.
- [x] **Keep level and XP in sync**: the editor edits XP, and Level is read-only; levels change only through Level Up.
- [x] **Weapon proficiencies**: class (PHB ch. 3) and base-Dwarf weapon training. Proficiency is added only with trained weapons, and the wizard and Inventory warn about untrained ones. The Elf's training waits for subraces.
- [x] **Backgrounds**: 13 PHB backgrounds, each granting two skill proficiencies, stored on the character and shown on the sheet. Background features and tool proficiencies aren't modeled.

### Combat

- [x] **Effects system**: designed in [docs/effects-system.md](docs/effects-system.md). It covers the next two items, concentration, and spell side effects.
  - **Done:** step zero (combat logic in a tested `dnd::Encounter`), phase 2 (the core effects engine), phase 3 (spells and monsters applying effects, plus the Stabilize action), and phase 4 (tooltips, death save pips, and the dead shown and kept out of play).
  - **Later:** tracked as their own items. Effects that outlast combat and reactions are under [Magic system](#magic-system); damage types, temporary HP, and creature types are below.
- [x] **Conditions**: all 14 PHB conditions are wired into advantage, disadvantage, auto-fails, auto-crits, and skipped turns (`dnd/effects`). They're applied in play by *Charm Person*, the Wolf's knock-prone, and dropping to 0 HP. Hovering a status in combat explains each one. More sources will come as monsters and spells are added.
- [x] **Death saving throws**: characters at 0 HP fall unconscious and roll death saves each turn. A natural 1 counts as two failures, and a natural 20 brings them back with 1 HP. Massive damage kills outright. Healing revives them, and the fight's end stabilizes them. Monsters ignore the dying. A **Stabilize** action (Medicine check, or automatic with a Healer's Kit) and *Spare the Dying* stabilize the dying. Death save pips show in combat, and the dead are marked and kept out of play.
- [ ] **Revival**: a temple's *raise dead* is done (the Shop's Temple tab, with the −4 penalty that eases each long rest). Still to come: characters casting *Revivify*, *Raise Dead*, *Reincarnate*, *Resurrection*, and *True Resurrection* themselves, which needs the spells, their material components, and time tracking. The penalty doesn't touch skill checks yet; initiative is the only ability check in combat.
- [ ] **More actions**: Disengage, Help, Hide, and Ready. (Attack, Dash, Dodge, Drink Potion, Cast a Spell, and Stabilize exist.) Most need positions or reactions to mean much without a map; Help (advantage on an ally's next attack) works without one.
- [x] **Monsters and encounter balance** ([docs/monsters-and-encounters.md](docs/monsters-and-encounters.md)):
  - Damage and creature types.
  - Multiattack and 13 SRD monsters from CR ½ to 5, with monster traits.
  - Five new adventures.
  - Encounter difficulty and adventure scaling.

  Follow-ups are the open items below.
- [x] **Monsters with several attacks**: Multiattack (the Bandit Captain, Owlbear, Ettin, Hill Giant, and Troll), with later attacks moving to someone else when a target drops.
- [ ] **Smarter monster AI**: monsters already ignore dying characters and won't attack whoever charmed them. Beyond that, they pick a random conscious character. Better choices might be the lowest HP, the biggest threat, or the spellcaster who's concentrating.
- [x] **Damage types**:
  - Every hit is typed (weapon, spell, monster attack, and radiant for Divine Favor and Divine Smite).
  - Resistance halves, vulnerability doubles, and immunity cancels. A skeleton takes double from a mace, and a zombie shrugs off Poison Spray.
  - Rage is resistance to bludgeoning, piercing, and slashing.
  - Monsters can be immune to conditions.
- [ ] **Temporary hit points from spells**: *Heroism*, *Armor of Agathys*, and similar. Temporary HP itself exists now (Dark One's Blessing): it soaks damage first and goes on a long rest.
- [x] **Creature types**: on every monster. *Charm Person* targets humanoids only, and Divine Smite adds 1d8 against undead and fiends. (*Protection from Evil and Good* is still a utility spell.)
- [ ] **Save log**: save the combat log or an encounter summary when the encounter ends.

### Content

- [ ] **More monsters**: 19 SRD stat blocks from CR 1/8 to 5 so far. Still to come: higher CRs, monster spellcasters, and the traits that need a map.
- [ ] **Monster editor**: add and edit custom monsters in the UI.
- [x] **Multi-encounter adventures** ([docs/adventures-and-currency.md](docs/adventures-and-currency.md)): adventures made of scenes, party choice, short and long rests between fights (long rests at camps), retreat, progress saved between fights (Resume), and three multi-fight adventures.
- [ ] **More adventure content**: branching choices, skill checks, and traps between fights. Scenes are narration plus a fight for now.
- [x] **Encounter difficulty**:
  - Fights are rated Trivial, Easy, Medium, Hard, or Deadly with the 2014 encounter-building rules: XP thresholds by level, plus a multiplier for the number of monsters.
  - Combat Encounter shows the rating live.
  - The Solo Adventure picker rates every adventure for the party and adjusts monster counts toward Medium–Hard (at most 12 monsters; every group keeps at least one, so a lone Troll can still be Deadly).
- [x] **Loot**: adventure rewards that drop items into inventory (depends on the inventory system).

### Technical / housekeeping

- [x] **Stale comment in `app/main.cpp`**: the comment and the database-error message now describe migrations: older save files upgrade in place, so a failure means a damaged file or a failed upgrade.
- [x] **Resizable windows and dialogs**:
  - Every window opens at a size that fits the screen (at most 90% of it) and remembers the size you leave it at.
  - **Combat Encounter** puts the fight and "Your Turn" side by side in a splitter when the window is wide, and stacks them when it's narrow. The participants and log share a splitter of their own, and the turn box scrolls.
  - **The character sheet** shows two columns when wide and one scrolling column when narrow, and long values wrap.
  - **The main window's** fight buttons have their own row.
  - **Scrolling pages:** the Level Up summary and the creation Review page scroll.
- [x] **Qt UI tests**: `dnd_ui_tests` (in `cpp/tests/ui`) runs in CI on Linux, Windows, and the AppImage build, checking that every window fits a small screen and covering the dialogs' behavior. New windows should get a line in `test_window_sizes.cpp`.
- [x] **Linux AppImage**: a local build script (`cpp/packaging/build-appimage.sh`).
- [x] **AppImage in CI**: built on every push and attached to releases for `v*` tags.
- [ ] **More packaging**: a Windows installer, and maybe Flatpak.
- [ ] **Wayland plugin**: bundle Qt's Wayland platform plugin so the app runs natively on Wayland instead of through XWayland.
- [ ] **Code style**: add `clang-format` / `clang-tidy` configs and check them in CI.
- [ ] **Legacy code**: decide whether to archive or remove the legacy `DND5EProject/` C# code.
- [x] **Per-player save/load** ([docs/save-load.md](docs/save-load.md)): Export… (a player or one character), Import… (with a preview and per-character conflict choices), and Back Up Database…. Files are versioned JSON that names everything, so they load into any database.

## License

MIT. See [LICENSE](LICENSE).
