# Scenes between fights, and NPCs

Status: **phases 1 and 2 built** (checks, Challenge/Trap/Choice scenes, surprise, inspiration; NPCs, Social scenes, the merchant and the priest); phase 3 next. Every recommendation was taken on 2026-10-05.

Phase 1 notes:
- The code is `dnd/checks` (checks, group checks, passive scores) and `dnd/scenes` (outcomes, traps, shaping the next fight); scenes are in `src/adventure.cpp`.
- Surprised creatures lose their first turn, but can still take reactions before it (the PHB says they can't). That is a small simplification.

Phase 2 notes:
- The code is `dnd/npc` (the NPC model, attitudes, `social_dc`, and `meet_npc`), Social scenes in `dnd/scenes`, and haggling and the priest in `dnd/shop`. Migration 34 adds the `Npcs` table and `Characters.Haggle`.
- **NPCs are saved per campaign** (0 = no campaign). An adventure defines its NPCs in code; the first time a campaign meets one, it's saved, and after that the campaign's copy is used, attitude and all. Deleting a campaign deletes its NPCs. Deleting an adventure NPC from the NPCs dialog means the next meeting starts fresh.
- **A Social scene** shows the NPC (name, role, attitude, description). Each request has a size (stop opposing, help at no risk, minor risk, significant risk), and the DC comes from the DMG table for the NPC's attitude: 0 needs no roll, and a request the attitude rules out is shown greyed ("not while they're Indifferent"). The player picks the approach: Persuasion, Deception, or Intimidation, or whichever the option allows.
- **Reading an NPC:** once per conversation, an Insight check against the scene's DC reveals what they want and fear. From then on every DC with them is 5 lower, in that campaign. The DMG leaves the adjustment to the DM; 5 is ours.
- **Outcomes can change attitudes:** the scene's NPC, the merchant, or the priest, one step at a time.
- **The merchant** (Hilda Brannock by default): once between long rests, each character can make a Persuasion check for 10% off everything they buy until their next long rest. The DC is the DMG's for a minor sacrifice: 10 if Friendly, 20 if Indifferent. A Hostile merchant won't haggle.
- **The priest** (Brother Aldous by default): a Friendly priest waives the 50 gp fee for *raise dead*, so it costs only the 500 gp diamond. That is our addition; the doc only asked for the attitude to change.
- **Content:** the Goblin Warren's captive is now a conversation (Snig). Crypt of the Restless opens with the gravedigger (Old Tobin), who can make the priest Friendly. The Hill Giant's Toll meets a caravan master (Dagna Ironfoot), who can make the merchant Friendly. In Moonrise at Ashgrove, the questioning is a conversation with the miller's wife (Wenna Hask).
- **The NPCs… dialog** (main window, next to Campaigns…) lists the active campaign's NPCs: name, role, attitude, description, wants, fears, whether the party knows them, and a stat block from the monster list (used in phase 3). The merchant and the priest can be renamed but not deleted.
- Haggling isn't exported with a character.

Rules follow the 2014 PHB (ch. 7, ability checks) and DMG (ch. 8: ability checks, social interaction, traps).

## Where things stand

- **Adventures are chains of fights.**
  - A scene is narration plus monsters. Between scenes the party can press on, rest, or retreat.
  - Nothing ever asks for an ability check.
- **Skills don't come into play.** Proficiencies, Expertise, Jack of All Trades, Lore bonus skills, and exhaustion's disadvantage on checks show on the sheet, and the dice roller can roll them by hand.
- **There are no NPCs.**
  - People exist only in narration ("the farmers want it cleared").
  - The Shop and the Temple have no faces.
  - Every creature in a fight is a monster against the party.

## Goals

### Scenes that aren't fights

Adventure scenes gain kinds besides Fight:

- **Challenge:** an obstacle with one or more ways through, each a skill check.
  - Example: the bridge is out. Climb down (Athletics, DC 15), find a ford (Survival, DC 12), or rig a rope (Sleight of Hand, DC 13).
  - Success and failure lead to different outcomes.
- **Trap:** a hidden danger.
  - The party's best passive Perception (10 + Perception bonus) spots it against a DC. Otherwise someone springs it: a save for half damage, or a condition.
  - A spotted trap can be disarmed (Thieves' Tools / Sleight of Hand) or avoided.
- **Social:** a conversation with an NPC. Pick an approach (Persuasion, Deception, Intimidation) against a DC set by the NPC's attitude, as in the DMG's conversation rules.
- **Choice:** two or three paths with different consequences, with no roll.
- **Group checks** (DMG): when the whole party has to do something (sneak past, cross the ice), everyone rolls. It succeeds if at least half succeed.
- **Help:** one character helps another's check, giving advantage (PHB). Only if the helper could do the task too.

### Outcomes that matter

Each option's success or failure can:
- **skip the next fight**, or remove some of its monsters;
- **surprise** (the party gets a free round, and monsters skip their first turn), or **be surprised** (the reverse);
- **give or cost:**
  - loot, coins, or HP (trap damage);
  - a level of exhaustion (a forced march);
  - inspiration (advantage on one roll in the next fight);
- **unlock a later option** (the password from the guard opens the vault without a fight);
- **change an NPC's attitude.**

### NPCs

**An NPC** has:
- a name, a role ("innkeeper", "cult priest");
- an **attitude** toward the party (Friendly, Indifferent, Hostile; DMG ch. 8);
- a short description, and what they **want** and **fear**. Bringing these up in conversation lowers the DC;
- optionally, **combat stats**, so they can fight or join a fight (questions 5–7).

NPCs appear in Social scenes, and can stand behind the Shop counter and the Temple altar (question 8).

## Rules (pure functions in `dndcore`, with tests)

- **`ability_check(character, skill_or_ability, dc, mode)`:**
  - the check bonus (proficiency, Expertise, Jack of All Trades);
  - advantage or disadvantage (Help, exhaustion 1+, armor without proficiency on STR and DEX checks, the *raise dead* penalty);
  - the d20 and the result. Initiative isn't affected.
- **`group_check`**, **`passive_score`** (10 + bonus, ±5 for advantage or disadvantage), and **`social_dc(attitude, request)`**, following the DMG's conversation table:
  - **Friendly NPCs** help with no risk to themselves without a check; DC 10 gets them to accept a minor risk, and DC 20 a significant one.
  - **Indifferent NPCs** need DC 10 to help at no risk, and DC 20 to accept a minor risk.
  - **Hostile NPCs** need DC 10 just to stop opposing the party, and DC 20 to help at no risk.
- **`resolve_scene(scene, choices, dice)`:** returns the outcome and every roll for the log. The adventure run records the outcome (skipped fights, surprise, unlocked options, attitudes).

## UI

**A Scene dialog** for non-fight scenes, opened by the adventure flow in place of the combat screen:

- the narration;
- the options (each with its skill and DC, or "a choice");
- **who attempts it** (question 1), and who helps;
- **Roll**, with the result and what happened in plain words;
- **Continue**.

## Build phases

1. **Checks and scenes:**
   - The rules (checks, group checks, passive scores, Help).
   - Challenge, Trap, and Choice scenes in adventure data, plus outcomes in the run.
   - The Scene dialog, and surprise in combat.
   - Two or three scenes added to each multi-fight adventure, and tests.
2. **NPCs and Social scenes:**
   - The NPC model and Social scenes with attitudes.
   - NPCs in the adventures, the Shop, and the Temple.
   - Building NPCs (question 5).
3. **NPCs in fights** (question 6): allies who fight beside the party, and named NPCs who turn hostile.
4. **A new adventure built around scenes and NPCs** (question 9).

## Open questions

1. **Who attempts a check:**
   - **(a) Recommended:** the player picks the character, and the dialog shows each character's bonus so the best choice is clear.
   - **(b)** The best-suited character rolls automatically.
2. **Failure on traps and challenges:**
   - **(a) Recommended:** failure costs something but never blocks the adventure: damage, a harder next fight, surprise against the party, a lost reward, or rarely a level of exhaustion.
   - **(b)** Failures can also end the adventure (the party is turned back).
3. **Surprise:**
   - **(a) Recommended:** both ways. Surprised combatants skip their first turn (PHB ch. 9), decided by the scene's outcome rather than by Stealth against passive Perception, since there's no map.
   - **(b)** Only the party surprising monsters.
4. **Inspiration as a reward** (PHB ch. 4): advantage on one attack, save, or check:
   - **(a) Recommended:** yes. Characters can hold one inspiration, and spend it from the combat screen or the Scene dialog.
   - **(b)** Not now.
5. **How NPCs are built:**
   - **(a) Recommended:** NPCs are saved in the database and edited in an **NPCs…** dialog: name, role, attitude, description, wants and fears. Combat stats come from a stat block picked from the monster list (Veteran, Mage, Cult Fanatic, Bandit Captain, ...), optionally renamed. Adventures define their own NPCs in code, the same way.
   - **(b)** NPCs built like characters (class, level, abilities) with the creation wizard, owned by the DM rather than a player. That's richer, but much heavier.
   - **(c)** Both: stat-block NPCs now, class-built NPCs later.
6. **NPC allies in fights:**
   - **(a) Recommended:** yes, in phase 3. An allied NPC fights on the party's side, controlled by the monster AI with its targets reversed. It takes turns, can be healed, and can fall. It gets no XP or loot.
   - **(b)** NPCs never fight beside the party.
7. **NPCs and campaigns:**
   - **(a) Recommended:** NPCs belong to a campaign (or to none). Their attitude is remembered: an innkeeper you helped stays friendly in later adventures of that campaign.
   - **(b)** NPCs exist only inside one adventure.
8. **The Shop and the Temple:**
   - **(a) Recommended:** each has a named NPC: a merchant and a priest with a line of description.
     - A Persuasion check against a Friendly or Indifferent merchant, once a day, gets 10% off.
     - Each one's attitude can change through adventures.
   - **(b)** Leave them faceless.
9. **The new adventure:**
   - **(a) Recommended:** **The Missing Heir** (level 3–5): an investigation in a town.
     - Social scenes with three NPCs who know different parts of the truth, a trap-filled cellar, and a choice: rescue the heir quietly or storm the hideout.
     - Fewer fights, but harder ones, depending on how the scenes go.
   - **(b)** Suggest another.
