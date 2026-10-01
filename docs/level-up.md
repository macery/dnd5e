# Level-up design

Status: **phases 1 (rules) and 2 (UI) done.** Decisions were settled on 2026-10-01; see [Decisions](#decisions).

**Phase 2 as built:**
- **`LevelUpDialog`:** a wizard with Hit Points, Ability Score Improvement, Fighting Style, New Spells, and Summary pages. Pages that don't apply are skipped.
  - **Hit Points:** take the average, or roll once and keep the result.
  - **Ability Score Improvement:** two "+1 to" picks, where the same ability twice gives +2. It shows each new score and modifier.
  - **New Spells:** lists only the new picks, after the ASI (prepared counts follow the casting ability). You can also pick them later in the Spellbook.
  - **Summary:** scrolls, and marks features that aren't automated yet.
  - **Finish** applies `apply_level_up` and saves the character and any new spells.
- **Main window:** a **Level Up** button runs the dialog once per pending level. The Level column shows "3 → 5" in bold, with a tooltip, when levels are waiting.
- **Character sheet:** the Experience row says "ready to level up".
- **Editor:** XP is editable, and Level is read-only.

**Phase 1 as built (`dnd/level_up`):**
- **`pending_levels`:** levels the XP has earned but the character hasn't taken.
- **`has_ability_score_improvement`:** the PHB ASI levels, including the Fighter's and the Rogue's extras.
- **`class_feature_names` / `class_features_at`:** every class's PHB ch. 3 features for levels 1–20, each flagged as automated or shown only.
- **`level_up_plan`:** the hit die and average, the ASI, a due Fighting Style and its options, the features gained, the spell allowance before and after, and `scaling_changes`. The changes cover proficiency, max HP, hit dice, attacks, feature uses, Rage damage, Sneak Attack, the Martial Arts and Bardic Inspiration dice, spells, and slots.
- **`validate_level_up`, `level_up_hp_increase`, `apply_level_up`:** roll or average, plus CON, minimum 1, with retroactive CON. The ASI is +2 to one ability or +1 to two, capped at 20.
- **Fighting Styles per class:** Paladins (Defense, Dueling, Great Weapon Fighting) and Rangers (Archery, Defense, Dueling) from level 2.
- **`award_experience`** only adds XP now and reports newly available levels. The victory log says "has enough XP for level N! Use Level Up on the main window."

**Until phase 2 lands,** characters can't level up through play: XP piles up and the level stays put. Rules follow the 2014 Player's Handbook: ch. 1 ("Beyond 1st Level") and ch. 3 (class tables).

## Where things stand

- **Levelling is automatic.** When a victory's XP crosses a threshold, `award_experience` raises the level on the spot and adds the fixed-average HP (hit die / 2 + 1 + CON, minimum 1) to both max and current HP. The log says "reaches level N!".
- **Nothing is chosen.** There are no Ability Score Improvements, so a level-8 character still has their level-1 scores. HP can't be rolled. The Paladin's and Ranger's level-2 Fighting Style can't be picked.
- **New spells are a hint.** The combat log says the character "may be able to learn new spells" and points at the Spellbook, which already enforces the new allowance.
- **New class features just appear.** Extra Attack, Ki, and Action Surge switch on by level with no mention.
- **The editor's Level field is free-form.** It can disagree with XP, which is a README TODO.

## Goals

- **A Level Up step** that walks through each new level in order, one level at a time when XP jumps several:
  1. **Hit points:** roll the hit die or take the average, plus CON.
  2. **Ability Score Improvement** at the PHB levels:
     - all classes at 4, 8, 12, 16, and 19;
     - Fighters also at 6 and 14;
     - Rogues also at 10.

     Either +2 to one ability or +1 to two, with a cap of 20.
  3. **Fighting Style** for Paladins and Rangers at level 2.
  4. **New spells:** cantrips and spells to learn, reusing the Spellbook's rules.
  5. **Summary:** what the new level brings (proficiency bonus, features, slots, Rage uses, Sneak Attack dice, ...), then confirm.
- **CON increases are retroactive:** +1 to the CON modifier means +1 max HP for every level already gained (PHB ch. 1).
- **Level always matches XP.** A character whose XP has passed their next threshold is shown as ready to level up everywhere.

## Non-goals for now

- **Feats.** They're an optional rule in the 2014 PHB and need feat data and effects of their own. (Question 3.)
- **Subclasses** (Champion at Fighter 3, Life Domain at Cleric 1, ...) and their features.
- **Multiclassing.**
- **Automating features past level 5.** They're listed by name only (question 4).
- **Swapping a known spell** on level-up (Bard, Ranger, Sorcerer, Warlock). The Spellbook's Forget already covers it loosely.
- **Level-down and undo.** A level-up is final, like the PHB; the editor stays the escape hatch.

## Rules (pure functions in `dndcore`, with tests)

**`dnd/level_up`:**
- `pending_levels(character)`: `level_for_xp(experience) - level`. It needs no new column, because the level is simply behind the XP.
- `asi_levels(class_name)` and `has_asi_at(class_name, level)`.
- `LevelUpPlan level_up_plan(character, class_info)`: what reaching `level + 1` brings. Fields:
  - the hit die and the average HP;
  - whether there's an ASI;
  - whether a Fighting Style is chosen, and the allowed styles;
  - the new spell allowance and new slot levels;
  - the features gained by name, with a flag for the ones the app automates;
  - old → new proficiency bonus and scaling values (Rage uses, Sneak Attack dice, Martial Arts die, Ki, Lay on Hands pool).
- `LevelUpChoices`: the HP method (average, or a rolled value) and the ASI (+2 to one ability, or +1 to two).
- `validate_level_up(character, plan, choices)`: rejects:
  - an ASI above 20;
  - an ASI given when none is due, or missing when one is;
  - a style this class can't take.
- `apply_level_up(character, plan, choices)`: bumps the level, adds HP (current HP too), applies the ASI, and adds retroactive CON HP. It also sets the Fighting Style.
- **Fighting Styles per class:**
  - **Fighter:** Archery, Defense, Dueling, Great Weapon Fighting.
  - **Paladin:** Defense, Dueling, Great Weapon Fighting.
  - **Ranger:** Archery, Defense, Dueling.
  - Protection and Two-Weapon Fighting stay out, as in phase 3 of class features.
  - `fighting_style()` starts honouring Paladins and Rangers from level 2.

**`award_experience` changes:** it only adds XP and reports levels now *available*. It no longer applies them. The victory log says "Aria has enough XP for level 3: use Level Up on the main window."

**Class feature names by level:** a table of each class's PHB ch. 3 feature names for levels 1–20, for the summary. Size depends on question 4.

## UI

- **Main window:**
  - The character table shows "3 → 4" (or a "Level up!" tag) when levels are pending.
  - A **Level Up** button, enabled for those characters, opens the Level Up dialog.
- **Level Up dialog:** a small wizard, one run per pending level, with these pages:
  1. **Hit Points:** "Take the average (5)" or "Roll 1d8", then + CON. It shows the new max.
  2. **Ability Score Improvement**, only at ASI levels: +2 to one, or +1 to two. Scores already at 20 are greyed out. It previews modifier and HP changes (CON).
  3. **Fighting Style**, only when one is due.
  4. **Spells**, only for casters whose allowance grew: the Spellbook's pick lists, limited to the new picks.
  5. **Summary:** what's gained, then **Level Up**.

  With several levels pending, finishing one offers the next.
- **Character sheet:** a "Ready to level up (level N)" line under Experience.
- **Editor:** answer to question 5.

## Build phases

1. **Rules and data:** `dnd/level_up` with the plan, validation, and apply.
   - ASI levels, retroactive CON, and Paladin and Ranger Fighting Styles.
   - `award_experience` stops auto-levelling.
   - The feature-name table.
   - Tests.
2. **UI:** the Level Up dialog, the main-window button and table tag, the sheet line, the victory log text, and the editor change. Plus an offscreen walkthrough.

## Existing characters

Clean starts are fine, so nothing is migrated. A character whose level is already above the ASI levels keeps their scores; the editor can fix them by hand. Level and XP already agree for anyone who levelled through play.

## Decisions

Settled 2026-10-01; every recommendation was taken:

1. **XP makes a level available**, and the player levels up from the main window with the Level Up dialog. Victories no longer level anyone automatically.
2. **HP each level:** the player chooses roll or average every time.
3. **Feats:** ASI only for now. Feats stay a non-goal.
4. **The summary lists every PHB ch. 3 feature name** for levels 1–20, marked "not automated yet" where the app doesn't do it.
5. **The editor edits XP;** Level is read-only and changes only through Level Up.
6. **New spells are picked in the Level Up dialog.**

## Open questions

1. **When does levelling happen?**
   - **(a) Recommended:** XP makes a level *available*, and the player levels up from the main window with the dialog. This is where the choices happen.
   - **(b)** Keep auto-levelling on victory, applying average HP and asking for the rest later. This splits a level-up in two.
2. **HP each level:**
   - **(a) Recommended:** the player chooses roll or average every time, as the PHB allows.
   - **(b)** A per-character setting.
   - **(c)** Always the average, as today.
3. **Feats:** ASI only for now (recommended), or offer "take a feat instead" with a handful of PHB feats?
4. **Features past level 5 in the summary:**
   - **(a) Recommended:** list every PHB ch. 3 feature name for levels 1–20, marked "not automated yet" where it isn't. The summary stays honest.
   - **(b)** Only the automated features.
5. **Editor Level field:**
   - **(a) Recommended:** the editor edits XP, and Level is read-only and follows level-up. A raised XP shows up as a pending level-up.
   - **(b)** Keep Level editable, but set XP to that level's threshold when it changes.
   - **(c)** Leave it as is.
6. **Spells in the dialog:**
   - **(a) Recommended:** pick new spells in the Level Up dialog.
   - **(b)** Keep pointing at the Spellbook.
