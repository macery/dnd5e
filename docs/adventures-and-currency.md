# Multi-encounter adventures and currency design

Status: **all phases done:** currency, multi-scene adventures, and progress that survives restarts. Phases 2 and 3 were built together. Decisions were settled on 2026-10-01; see [Decisions](#decisions).

**Phases 2 and 3 as built:**
- **Data:**
  - `AdventureScene` (title, narration, monsters, loot, coins, `camp_after`, `boss`) and `AdventureTemplate::scenes` plus `suggested_level`.
  - The nine existing adventures are one-scene adventures, built with a `single(...)` helper.
  - New: **The Goblin Warren** (level 1+, three fights), **Crypt of the Restless** (level 2+, three), and **The Hill Giant's Toll** (level 5+, four), each with a camp partway and a boss at the end.
- **Scaling:** `scale_scene` aims for Medium–Hard, or for Hard on a boss scene. Coins scale with the party's average level. `scale_adventure` scales every scene and reports the hardest.
- **`dnd/adventure_run`:** `AdventureRun` (title, next scene, party, status, long rest taken) with `next_scene`, `record_fight`, `can_long_rest` (once per camp), `retreat`, and `progress_text`.
- **Saving:**
  - Migration 21 adds `AdventureRuns`, with `Repository::add_adventure_run`, `update_adventure_run`, and `get_adventure_runs`.
  - Progress is saved after every fight and rest.
  - Leaving mid-fight saves the place before that fight; the damage taken stays.
- **`short_rest(character, class, dice, hit_dice)`:** the automatic part, Arcane Recovery, and up to N hit dice, stopping at full HP.
- **UI:**
  - **Picker:** "▶ Resume" entries first. Each adventure shows its fight count, suggested level, and hardest rating; the details list every fight's monsters and rating, camps, and total rewards.
  - **`PartyDialog`:** a checklist of living characters, defaulting to the current player's.
  - **Combat Encounter:** shows the scene's title and narration, and a **Continue** (or **End Adventure**) button once the fight is over.
  - **`BetweenScenesDialog`:**
    - the party's HP, hit dice, and remaining features and slots;
    - **Press On**, **Short Rest…**, **Long Rest** (camps only, then Level Up for anyone ready), and **Retreat**;
    - closing it stops for now.
  - **`ShortRestDialog`:** hit dice per character, defaulting to enough to heal up on average. It's also used by the main window's Short Rest button.

**Phase 1 as built:**
- **Migration 20:** `Characters.Coins` ("cp:12,gp:15"), and `Character::coins` (cp, sp, ep, gp, pp).
- **`dnd/currency`:** coin values, `total_cp`, `coins_from_cp` (gold, silver, and copper for payouts), `format_coins`, storage text, `split_cp` (leftover copper to the first), and `background_purse_gp` (the PHB purses, 5–25 gp).
- **Creation:** new characters get their background's purse, shown on the Review page.
- **Rewards:**
  - `AdventureTemplate::coins_cp` is set for a level-1 party, and `scale_adventure` multiplies it by the party's average level.
  - `Encounter::set_victory_reward` takes the coins and splits them among the survivors: "The party finds 24 gp", or each share when they differ.
- **Exports** carry a `coins` object. Older files without it import with no coins, so the format stays version 1.
- **UI:** a Coins row on the sheet, and a coin line in the Inventory with what the carried items are worth. The editor keeps coins when saving. Rules follow the 2014 Player's Handbook (ch. 5 coins and starting equipment, ch. 8 resting).

## Where things stand

- **An adventure is one fight:**
  - A hook, monster groups (scaled toward Medium–Hard for the party), victory text, and item loot for the first character still standing.
  - XP is split across the survivors.
  - Then you're back at the main window.
- **Resting is a main-window chore:**
  - **Short Rest** spends one hit die for the selected character per click, and restores pact slots, short-rest features, and Arcane Recovery.
  - **Long Rest** restores everything.

  Nothing stops a long rest between every fight, so per-day resources (Rage, Lay on Hands, spell slots) rarely matter.
- **Who fights:** every living character in the database, across all players.
- **No money:** items have a price (`Item.cost_cp`), but characters have no coins and adventures pay none.

## Goals

- **Adventures as a short series of scenes:**
  - two to four fights, with a bit of narration before each and a choice between them;
  - the last fight can be a tougher "boss" scene;
  - rewards per scene and at the end.
- **Resting inside an adventure, by the PHB:**
  - Between scenes the party can **press on** or take a **short rest** (1 hour). Each character chooses how many hit dice to spend, then short-rest features and Arcane Recovery apply.
  - A **long rest** happens only where the adventure has a camp (question 3), or not at all.
- **Choosing the party** at the start, rather than everyone in the database.
- **Retreating** between scenes, keeping the XP and rewards earned so far.
- **Currency:**
  - Characters carry coins: copper, silver, electrum, gold, and platinum (1 pp = 10 gp = 20 ep = 100 sp = 1,000 cp).
  - Adventures pay coins, split evenly among the survivors.
  - New characters start with their background's purse (question 6).
  - Coins show on the sheet and in the Inventory, and come along in exports.
- **Progress survives closing the app** between scenes (question 2).

## Non-goals for now

- **Shops and buying, selling, and paying for services** (temple revival). Coins first; spending is its own TODO.
- **Branching stories, skill checks, traps, and exploration** between fights. Scenes are narration plus a fight.
- **Saving mid-fight.**
- **Random treasure tables** (question 5).
- **Encumbrance**, including coin weight (50 coins = 1 lb). It stays its own TODO.

## Data model

**Adventures stay data in code** (`dnd/adventure`), extended:

```cpp
struct AdventureScene {
    std::string title;        // "The Warren Entrance"
    std::string narration;    // shown before the fight
    std::vector<AdventureMonsterGroup> monsters;
    Difficulty target = Difficulty::Medium;  // scale toward this (the boss: Hard)
    std::vector<AdventureLoot> loot;          // items after this fight
    int coins_cp = 0;                         // coins after this fight (question 5)
    bool camp_after = false;                  // a long rest is possible after it (question 3)
};
struct AdventureTemplate {
    std::string title, hook, victory_text;
    int suggested_level = 1;                  // shown in the picker
    std::vector<AdventureScene> scenes;       // one scene = today's single-fight adventures
    std::vector<AdventureLoot> final_loot;
    int final_coins_cp = 0;
};
```

The nine existing adventures become one-scene adventures, unchanged in play.

**Coins** (migration):
- **`Characters.Coins`:** text such as `"cp:12,sp:3,gp:15"`, stored like `FeatureUses`. `Character::coins` is `std::array<int, 5>` (cp, sp, ep, gp, pp).
- **Rules (`dnd/currency`):**
  - `total_cp(coins)`, `add_coins`, and `format_coins` ("15 gp, 3 sp, 12 cp").
  - `split_coins(total_cp, n)`: share evenly, largest denominations first. The remainder goes to the first characters, one copper at a time.
  - `background_purse(name)`: the PHB starting gold.

**Adventure progress** (question 2), a migration adding `AdventureRuns`:
- `AdventureRunId`, `Title`, `SceneIndex` (the next scene), `PartyCharacterIds` ("3,5,9"), `StartedAt`, and a `Status`: InProgress, Finished, Retreated, or Defeated.
- Character state already lives in `Characters`, so only the place in the story is stored.

**The export format** gains an optional `coins` object per character. Version 1 readers ignore it and files without it import with no coins, so the format version stays 1.

## Rules (pure functions, with tests)

- **`dnd/adventure_run`:** a small state machine over a template and a party:
  - `current_scene()`, `scene_monsters(scaled)`;
  - `finish_scene(victory)`, then `rest_options()`: Press on, Short rest, and Long rest only at a camp;
  - `retreat()`, `is_over()`, and `final_rewards()`.

  It needs no database: the app saves its state to `AdventureRuns` after each scene.
- **Scaling** reuses `scale_adventure` per scene, with a target band: Medium–Hard for most scenes, and Hard (never Deadly unless a lone boss already is) for a boss.
- **Short rest with a choice of hit dice:** `spend_hit_dice(character, dice, hit_die, count)`. The main window's Short Rest button gains the same choice.

## UI

- **Solo Adventure picker:**
  - Each adventure shows its scenes ("3 fights, a camp after the 2nd") and its suggested level.
  - **Resume** for an adventure in progress.
- **Party selection:** a checklist of living characters, defaulting to the current player's.
- **The adventure window,** the existing Combat Encounter dialog one scene at a time:
  - **Before each fight:** the scene title and narration, and the difficulty for the party.
  - **After a victory:** that scene's rewards, then a **between scenes** panel:
    - each character's HP, hit dice, and spent resources;
    - buttons: **Press on**, **Short rest** (per-character hit dice choice), **Long rest** (camp only), and **Retreat**.
  - **The end:** victory text, final rewards, and the total earned.
  - **Defeat:** the adventure is over, and the dead stay dead as today.
- **Coins:**
  - **Sheet:** a Coins row.
  - **Inventory:** a coin line, plus the gold value of what's carried, from `cost_cp`.
  - **Wizard:** the background's purse added at creation, shown on the Review page.

## Build phases

1. **Currency:** the migration, `dnd/currency`, the background purses, the sheet, Inventory, and wizard, the export field, and coins in today's adventure rewards (split among survivors). Tests.
2. **Multi-scene adventures:**
   - the scene data model, `dnd/adventure_run`, per-scene scaling, and choosing hit dice on a short rest;
   - the adventure window flow (narration, between-scenes panel, retreat, final rewards) and party selection;
   - two or three new multi-scene adventures. Tests and a walkthrough.
3. **Progress that survives restarts** (if question 2 is yes): `AdventureRuns`, and Resume in the picker.

## Decisions

Settled 2026-10-01; every recommendation was taken:

1. **Choose the party** at the start, defaulting to the current player's living characters.
2. **Progress is saved between scenes,** and the picker offers Resume.
3. **Long rests only at camps** the adventure marks.
4. **Level Up is offered during a camp's long rest.**
5. **Coins are fixed per scene and at the end,** scaled by the party's average level.
6. **New characters start with their background's purse.**
7. **Retreat is allowed between scenes,** keeping what was earned.

## Open questions

1. **Party:**
   - **(a) Recommended:** choose the party at the start (a checklist, defaulting to the current player's living characters).
   - **(b)** Keep "everyone alive in the database".
2. **Closing the app mid-adventure:**
   - **(a) Recommended:** progress is saved between scenes, and the picker offers Resume.
   - **(b)** An adventure must be finished in one sitting; closing abandons it, keeping the XP and rewards earned so far.
3. **Long rests:**
   - **(a) Recommended:** only at a camp the adventure marks (for example after the second of four fights).
   - **(b)** Never inside an adventure.
   - **(c)** Any time between scenes.
4. **Levelling up mid-adventure:**
   - **(a) Recommended:** allowed during a long rest at a camp (it offers Level Up for anyone ready).
   - **(b)** Only after the adventure ends.
5. **How much coin:**
   - **(a) Recommended:** a fixed amount per scene and at the end, written into each adventure and scaled by the party's average level.
   - **(b)** Roll the 2014 individual-treasure tables per monster. That's more random, and those tables are in the DMG, which you don't have.
6. **Starting coins:**
   - **(a) Recommended:** the background's purse from the PHB (for example 10 gp for a Soldier, 15 for an Acolyte, 25 for a Noble), on top of the class kit.
   - **(b)** Start with none.
7. **Retreat:**
   - **(a) Recommended:** allowed between scenes. You keep what you earned, and the adventure ends as "Retreated".
   - **(b)** No retreating.
