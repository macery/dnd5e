# Campaigns design

Status: **phases 1 (core) and 2 (UI) done.** The questions below were settled with the recommended answers without a review round (2026-10-02). Any of them can be revisited.

**As built:**
- **Core:**
  - Migration 24 adds `Campaigns`, `Characters.CampaignID`, and `AdventureRuns.CampaignID`.
  - The repository gains campaign CRUD, `active_campaign_id`, `set_active_campaign`, and `set_character_campaign`. `update_character` never changes a character's campaign.
  - `dnd/campaign` provides `characters_in_play`, `in_play`, `runs_in_play`, and `active_campaign_name`.
  - `give_coins`, `give_item`, and `raise_dead` refuse characters outside the active campaign.
- **Main window:**
  - A **Campaign** selector at the top ("No campaign (everyone plays)" or a campaign), a **Campaigns…** button, and the campaign's name in the window title.
  - The character table lists everyone in play, across players, with Player and Campaign columns. The player list is a filter that defaults to **All players** (added 2026-10-03, after playtesting).
  - With All players selected, Add Character asks which player it's for, and Export uses the selected character's player. A character created while a campaign is active joins it.
- **`CampaignDialog`:** a list of campaigns (New and Delete), and for the selected one its name, notes, and a roster checklist of every player's characters. Characters in another campaign are marked "— in Dragon Heist", and checking them moves them.
- **Scoped play:**
  - Combat Encounter, the Initiative Tracker, and Solo Adventure use the campaign's living characters. The party is all of them by default, and difficulty ratings use the whole roster.
  - New runs belong to the campaign, and Resume shows only its runs.
  - Give Coins, Give Items, and the Temple list only its characters.
- **Import:** "Add the imported characters to the campaign …", checked by default when one is active.
- **Tests:** `tests/test_campaign.cpp` and `tests/ui/test_campaign_ui.cpp`. The Campaigns dialog is in the small-screen size test.

## Where things stand

- **Players own characters, and that's the only grouping.** The main window shows one player's characters at a time.
- **Everything that picks "who" uses every character in the database:**
  - Combat Encounter and the Initiative Tracker take every living character.
  - Solo Adventure's party list offers every living character, and checks the current player's by default.
  - Give Coins, Give Items, and the Temple's payers list everyone.
- **Adventure runs** belong to no one: Resume lists every run in progress.

That works for one table. With two groups, or characters a player is building but not playing yet, everything mixes together.

## Goals

- **A level above players: campaign → players → characters.** Players keep owning and building their characters. A campaign is a table: a name, notes, and a roster of characters from any players.
- **One campaign at a time is active.** It's chosen on the main window and remembered between runs. With no campaign active, everything works as it does today.
- **The active campaign scopes play:**
  - Combat Encounter, the Initiative Tracker, and Solo Adventure's party use only its characters, and the party defaults to all of them.
  - Give Coins, Give Items, and the Temple's lists stay within it.
  - New adventure runs belong to it, and Resume shows only its runs.
- **Joining a campaign:** a Campaigns dialog to create, rename, and delete campaigns and to choose each one's roster. Import can add the imported characters to the active campaign, which covers "build your character at home, bring it to the table".
- **Tests:** rules and repository tests, plus UI tests for the dialog and the scoping.

## Non-goals for now

- **Characters in several campaigns at once.** A character is in at most one campaign; joining another moves them. Their HP, items, and coins are one set, so being in two would mix the tables' stories.
- **Per-campaign houserules, calendars, session logs, or DM tools.**
- **Players belonging to campaigns.** Characters join, and the player comes along through them.

## Decisions (settled as recommended; open to revisiting)

1. **Membership:** a character is in at most one campaign (`Characters.CampaignID`; 0 = none).
2. **Active campaign:** stored in the database (`Campaigns.Active`), so it survives restarts and the core rules can see it. "No campaign" means everything is open, as today.
3. **Scoping:** fighting, parties, trading, Temple payers, and adventure runs follow the active campaign. The main window's character table still shows the selected player's characters, with a Campaign column.
4. **Rules enforcement:** `give_coins`, `give_item`, and `raise_dead` refuse characters outside the active campaign, as well as hiding them in the lists.
5. **Deleting a campaign** keeps its characters and their adventure runs. They just leave the campaign.
6. **Import:** an "Add to the active campaign" checkbox, on by default when a campaign is active.

## Data model

Migration 24:
- `Campaigns (CampaignID, Name UNIQUE, Notes, Active, CreatedAt)`.
- `Characters.CampaignID`, default 0.
- `AdventureRuns.CampaignID`, default 0. Runs started with no campaign active stay visible when none is active.

## Build phases

1. **Core:**
   - The migration.
   - The `Campaign` struct and repository CRUD, plus `set_character_campaign`, `active_campaign_id`, and `set_active_campaign`.
   - `dnd/campaign`: `characters_in_play` (the active campaign's, or everyone), `in_play`, and the scoped trading checks.
   - Runs carry their campaign.
   - Tests.
2. **UI:**
   - The main window's Campaign selector and **Campaigns…** dialog, and a Campaign column.
   - Scoped fights, parties, trading, and Resume.
   - The Import checkbox.
   - UI tests, docs, and the release.
