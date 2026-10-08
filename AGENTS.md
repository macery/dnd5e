# AGENTS.md

Guidance for coding agents working in this repo. `README.md` covers the features, architecture, and build details; this file covers how work gets done here.

## What this is
A desktop companion and solo-play engine for D&D 5e: C++20, Qt 6 Widgets, SQLite. It's a learning and practice project with no fixed spec, so features grow in experimental phases. All rules follow the **2014** Player's Handbook and DMG, not the 2024 revision. `DND5EProject/` is the retired C# version: don't change it.

## Build and test
```sh
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build --parallel
ctest --test-dir cpp/build --output-on-failure
```
- Core tests: `cpp/build/tests/dnd_tests "[tag]"` runs one area.
- UI tests need the offscreen platform: `QT_QPA_PLATFORM=offscreen cpp/build/tests/ui/dnd_ui_tests`.
- Tests that roll real dice (anything using `SystemRandomSource`, such as the Scene dialog) should be run several times before you trust a pass.
- A new test file must be added to `cpp/tests/CMakeLists.txt` (or `cpp/tests/ui/CMakeLists.txt`).
- CI builds on Linux and Windows (MSVC) with Qt 6.7, so keep code portable and don't use Qt APIs newer than 6.7.

## Where code goes
- **Rules live in `dndcore`** (`cpp/include/dnd`, `cpp/src`): pure functions, no Qt, and every rule gets a Catch2 test. Dice come from an injected `IRandomSource`, and tests script the rolls (see `ScriptedRandomSource` in the tests). Tests use `Repository(":memory:")`.
- **`cpp/app` is only the view.** Dialogs call `dndcore` and hold no rules logic. The combat dialog, for example, is a view over `dnd::Encounter`.
- **Content is code:** adventures, scenes, and their NPCs live in `cpp/src/adventure.cpp`, built with the small helpers at the top of that file. Seeded rules data (monsters, items, spells) lives in migrations.

## Database changes
- Append a new entry to `kMigrations` in `cpp/src/migrations.cpp`, and bump the array size. Never edit a migration that has shipped.
- Then update the counts the tests pin: the schema version and migration count in `cpp/tests/test_migrations.cpp`, and the monster count in `test_migrations.cpp` and `test_repository.cpp` if you seed monsters.
- A new column also needs its `Repository` select, insert, and update bindings.

## Docs
- Each feature has a design doc in `docs/` (for example `docs/scenes-and-npcs.md`): the goals, the decisions taken, a **Status** line, and per-phase notes on what was actually built and any departures from the plan. Update them as you build.
- `README.md`: keep the Features list and the TODO checklist current.
- Write docs, UI text, and commit messages in plain, short, direct sentences, the way the existing ones read. Comments explain why, not what.

## Releases
The owner playtests from releases, so a finished feature is committed, pushed to `main`, and tagged without asking first.
- Tags are `vX.Y.Z`: a minor bump for features, a patch for fixes.
- Pushing a tag makes CI (`.github/workflows/appimage.yml`) build the AppImage and attach it to a GitHub release, with notes taken from the commit messages since the last tag.
- To check packaging changes locally, build in the same container CI uses:
  `podman run --rm -v "$PWD":/src:Z -w /src docker.io/library/ubuntu:24.04 cpp/packaging/build-appimage.sh`

## Commits
- One feature or fix per commit.
- The subject says what changed for the player ("Scenes and NPCs, phase 3: NPC allies in fights"), and the body lists the visible changes.
