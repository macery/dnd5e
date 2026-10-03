# Magic items design

Status: **phases 1 (rules and data) and 2 (scrolls and UI) done.** Decisions were settled on 2026-10-02; see [Decisions](#decisions).

**Phase 2 as built:**
- **Combat:** a **Read Scroll** action shares the spell controls: the pack's scrolls, each marked "DC 11 check" or "can't read", the spell's effect with the scroll's DC and attack bonus, and targets. `Encounter::read_scroll` checks, rolls any spellcasting check, uses up the scroll (ItemUsed), and casts through the same path as Cast a Spell (`resolve_spell`).
- **Short Rest dialog:** an **Attunement** column per character: attune to one item, or end attunement to one.
- **Inventory:** rarity after magic item names, an **Attuned** column ("needs attunement" until then), an "Attuned: 1 of 3" count, and Drop and Give disabled for attuned items.
- **Character sheet:** scores raised by items ("19 (+4) — Gauntlets of Ogre Power; 16 without it"), with saves and skills to match.
- **Shop:** lists only what shops sell, with a **Magic items** filter and rarity after names.
- **Boss loot:** the Goblin Warren's boss drops a Cloak of Protection; the Crypt's a Mace +1, a Potion of Greater Healing, and a Cure Wounds scroll; the Hill Giant's two Greater Healing potions, a Shield +1, and Gauntlets of Ogre Power.
- **UI tests** (`tests/ui/test_magic_items_ui.cpp`): reading a scroll in combat, attuning on a short rest (and what that locks in the Inventory), the Shop's filter, and the sheet.
- **Left for later** (README TODO): charged items, and reading scrolls outside combat.

**Phase 1 as built:**
- **Migration 23:**
  - `Items` gains `Rarity`, `BaseItem`, `MagicBonus`, `RequiresAttunement`, `AcBonus`, `SaveBonus`, `UnarmoredAcBonus`, `SetsAbility`, `SetsAbilityTo`, and `ScrollSpellID`. `CharacterInventory` gains `Attuned`.
  - Seeded: 87 +N weapons, armor, and shields (generated from the mundane ones, stats copied, named "Longsword +1"); Greater, Superior, and Supreme Healing; the five wondrous items; and 24 spell scrolls. Only spells of levels 0–1 are automated so far, so those are the scroll levels.
- **New categories:** `Wondrous` and `Scroll`. Wondrous items are worn by attuning; Equip refuses them with a pointer to the short rest.
- **`dnd/magic_items`:**
  - `item_active`, `with_item_scores` (STR/CON raised), `item_hp_bonus` and `without_item_hp` (Level Up uses it), `item_save_bonus`, and a `saving_throw_bonus` overload with the inventory.
  - Attunement: `attuned_count`, `attunement_refusal`, `attune`, and `end_attunement` (with the Amulet's HP).
  - Scrolls: `scroll_save_dc`, `scroll_attack_bonus`, and `read_scroll` (class list, check above the highest slot).
- **Applied in the rules:**
  - `weapon_attack` adds the bonus to attack and damage, and uses raised STR.
  - `calculate_armor_class` adds magic armor and shields ("Chain Mail +1 17"), worn AC items, and the Bracers.
  - Proficiency follows `BaseItem`, so a Bard's +1 rapier is still a rapier. Encounter saves include items.
- **Shop:** `sold_in_shops` (mundane, common, uncommon); buying anything rarer is refused. Attuned items can't be sold or given.
- **Exports** carry an optional `attuned` per item.

Rules follow the 2014 Dungeon Master's Guide (ch. 7, "Treasure"): the same edition as the Player's Handbook used everywhere else.

## Where things stand

- **Items are mundane.** There are 32 seeded items: PHB gear, weapons, armor, a shield, and one magic item, the Potion of Healing (2d4 + 2).
- **Equipping:** one armor, one shield, and one weapon at a time. They set AC and the attack (`dnd/equipment`).
- **Consumables:** only healing potions do anything: Drink Potion in combat, or Use in the Inventory.
- **Coins have few uses.** The Shop sells PHB gear, and the Temple raises the dead for 550 gp. Adventures pay a few gp per fight, and boss fights drop a potion at best.
- **Nothing is per-item state.** An inventory entry is an item, a quantity, and an equipped flag. Charges (a wand's) would be new.

## Goals

- **Magic weapons and armor:** +1, +2, and +3 versions. A weapon adds its bonus to attack and damage rolls; armor or a shield adds it to AC. They're equipped like their mundane versions and show on the sheet ("Longsword +1").
- **More healing potions:** Greater (4d4 + 4, uncommon), Superior (8d4 + 8, rare), and Supreme (10d4 + 20, very rare), with the existing Drink Potion and Use.
- **A few worn wondrous items** whose effects the app can already apply:
  - **Cloak of Protection** and **Ring of Protection:** +1 to AC and saving throws.
  - **Bracers of Defense:** +2 AC while wearing no armor and no shield.
  - **Gauntlets of Ogre Power:** STR 19. **Amulet of Health:** CON 19 (with the HP that follows from it).
- **Attunement:** the DMG's limit of three attuned items per character (question 2).
- **Spell scrolls** of spells the app already automates: *Cure Wounds*, *Magic Missile*, *Bless*, and so on. Reading one casts the spell without a slot, then the scroll is gone (question 3).
- **Where they come from:** boss-scene loot in the multi-fight adventures, and the Shop for the lesser ones (question 4).
- **Prices** from the DMG's rarity table (question 5). Selling pays half, like everything else.
- **Tests:** rules tests for each bonus, and UI tests for equipping, the sheet, and reading a scroll.

## Non-goals for now

- **Cursed items, sentient items, and identifying items.** Items are known by name when found.
- **Wands, staffs, and other charged items** (question 6). They need charges saved per item.
- **Resistance to nonmagical attacks.** No seeded monster has it yet; it comes with the monsters that need it.
- **Magic items the app can't apply yet:** flying, invisibility, extra spells known, and anything that needs a map or an exploration mode.
- **Random treasure tables** (DMG Treasure Hoards). Loot is chosen per adventure.

## Data model

**Items gain magic fields** (migration 23, on `Items`):
- `Rarity`: `Common`, `Uncommon`, `Rare`, `VeryRare`, `Legendary`; empty for mundane items.
- `MagicBonus`: +1/+2/+3 for a weapon (attack and damage) or armor and shield (AC).
- `RequiresAttunement`.
- `Wondrous` effects as small typed columns rather than free text:
  - `AcBonus` and `SaveBonus` (Cloak and Ring of Protection);
  - `UnarmoredAcBonus` (Bracers of Defense);
  - `SetsAbility` and `SetsAbilityTo` (Gauntlets: STR 19; Amulet: CON 19).
- `ScrollSpellID`: the spell a scroll casts.

**A new category, `Wondrous`,** for worn items. Wearing one is the existing equipped flag. Rings allow two at once; other wondrous items allow one of each name. Magic weapons, armor, potions, and scrolls keep their existing categories.

**Characters don't change.** Attunement is "equipped, and requires attunement", so it needs no column (question 2).

## Rules (pure functions in `dndcore`, with tests)

- **`weapon_attack`:** adds a wielded weapon's `MagicBonus` to attack and damage. The name shows it ("Longsword +1").
- **`calculate_armor_class`:** adds armor and shield bonuses, plus worn wondrous AC bonuses, as parts of the breakdown ("Cloak of Protection 1"). Bracers count only with no armor and no shield.
- **`saving_throw_bonus`:** gains worn save bonuses. This needs the inventory, so the encounter and sheet pass it.
- **`effective_scores(character, inventory)`:** STR/CON set by worn items, when higher than the character's own. Every derived number uses it. A higher CON also raises max HP while the amulet is worn (DMG: as if the score were 19).
- **`can_attune` / `attuned_items`:** at most three equipped items that require attunement. Equipping a fourth is refused with a message.
- **Spell scrolls:** a reader whose class has the spell casts it as an action, using their own spell attack bonus and save DC, with no slot spent. Anyone else can't use it (the DMG's ability check for higher-level scrolls doesn't apply: question 3).
- **Encounter:** a **Read Scroll** action, next to Drink Potion. It reports ItemUsed like a potion.

## UI

- **Inventory:**
  - Magic items show their rarity and bonus.
  - Equip handles wondrous items; a fourth attuned item is refused with "Aria is already attuned to 3 items."
  - An **Attuned** count sits next to the equipped summary.
- **Character sheet:** AC, attack, and save breakdowns name the items. The STR and CON rows show "19 (Gauntlets of Ogre Power)".
- **Combat:** **Read Scroll** in the action list when the character carries a scroll they can use: pick the scroll, then targets as for Cast a Spell.
- **Shop:** a **Magic** category on the Buy tab (question 4).
- **Loot:** boss scenes name their magic items like any loot, so they go through the Divide the Loot panel.

## Build phases

1. **Rules and data:**
   - Migration 23: the columns, plus the seeded items.
   - The bonuses in `weapon_attack`, AC, saves, and effective scores.
   - Attunement.
   - Tests.
2. **Scrolls and the UI:**
   - Read Scroll in combat and the Inventory.
   - The Inventory, sheet, and Shop changes.
   - Boss loot.
   - UI tests, docs, and the release.

## Existing characters

Clean starts are fine. Migration 23 only adds columns and items, so existing characters and inventories carry over unchanged.

## Decisions

Settled 2026-10-02:

1. **The full first set (1a):** +1/+2/+3 versions of every weapon, armor, and the shield; Greater, Superior, and Supreme Healing; Cloak and Ring of Protection, Bracers of Defense, Gauntlets of Ogre Power, and Amulet of Health; scrolls of the automated spells of levels 0–3.
2. **Attunement the DMG way (2b):**
   - Attuning to an item takes a short rest focused on it: **one item per short rest**, chosen in the Short Rest dialog. Ending attunement voluntarily also takes a short rest.
   - At most **three** attuned items, and one attuned copy of any item.
   - The five wondrous items all require attunement, and an attuned wondrous item counts as worn: there's no separate Equip for them. +N weapons and armor don't require attunement (DMG); they're equipped as usual.
   - Attuned items can't be given, sold, or dropped until attunement ends, like equipped gear.
   - **Amulet of Health:** attuning raises max HP (and current HP) by what CON 19 adds over the character's own CON, and ending it takes that back off (current HP capped). Level Up works around the amulet, so the bonus always matches the current level.
3. **Spell scrolls the full DMG way (3b):**
   - Only a reader whose class has the spell on its list can read it, and the scroll casts at its own level.
   - A spell above the reader's highest spell slot level needs a check with their spellcasting ability, DC 10 + the spell's level. On a failure the scroll is wasted.
   - Saves and attacks use the scroll's own numbers (DMG "Spell Scroll" table): DC 13 and +5 for cantrips through 2nd level, DC 15 and +7 for 3rd.
4. **The Shop sells common and uncommon magic items (4a).** Rare and up come only from boss loot.
5. **A fixed price per item inside its rarity range (5a).** See [Prices](#prices).
6. **No charged items yet (6a).** Wands and staffs are a follow-up.

## Prices

Inside the DMG's rarity ranges, plus the mundane item's own price for weapons and armor:

| Item | Rarity | Price |
|---|---|---|
| Weapon +1 / +2 / +3 | uncommon / rare / very rare | 400 / 4,000 / 20,000 gp |
| Armor +1 / +2 / +3 | rare / very rare / legendary | 1,500 / 15,000 / 60,000 gp |
| Shield +1 / +2 / +3 | uncommon / rare / very rare | 300 / 3,000 / 15,000 gp |
| Potion of Healing / Greater / Superior / Supreme | common / uncommon / rare / very rare | 50 / 150 / 600 / 6,000 gp |
| Cloak of Protection, Gauntlets of Ogre Power | uncommon | 400, 500 gp |
| Ring of Protection, Bracers of Defense, Amulet of Health | rare | 3,500, 4,000, 4,000 gp |
| Spell scroll, cantrip / 1st / 2nd / 3rd | common / common / uncommon / uncommon | 50 / 75 / 150 / 300 gp |

## Open questions (settled)

1. **Which items in the first set?**
   - **(a) Recommended:**
     - weapons and armor +1/+2/+3, for each existing weapon, armor, and the shield (generated, not typed);
     - the three bigger healing potions;
     - Cloak and Ring of Protection, Bracers of Defense, Gauntlets of Ogre Power, and Amulet of Health;
     - scrolls of the automated level 1–3 spells.
   - **(b)** A smaller hand-picked list: a +1 longsword, +1 chain mail, a +1 shield, two potions, and the Cloak of Protection.
2. **Attunement:**
   - **(a) Recommended:** enforce the limit of three. Equipping an attunement item attunes it; unequipping ends it. There's no short-rest wait.
   - **(b)** The DMG way: attuning takes a short rest, tracked per item.
   - **(c)** Ignore attunement.
3. **Spell scrolls:**
   - **(a) Recommended:** only classes with the spell on their list can read it, using their own casting stats. Scroll level is the spell's level.
   - **(b)** The full DMG rule: also a DC 10 + spell level check for spells above the reader's highest slot, and the DMG's fixed scroll save DC and attack bonus (13/+5 for levels 1–2, 15/+7 for level 3).
   - **(c)** Leave scrolls out of the first set.
4. **Where magic items come from:**
   - **(a) Recommended:** the Shop sells common and uncommon items (potions, +1 gear, the Cloak, scrolls of levels 1–3). Rare items and up (+2/+3, the Amulet, the Gauntlets) come only from boss loot.
   - **(b)** Loot only: the DMG treats magic items as rarely for sale.
   - **(c)** Everything for sale.
5. **Prices:**
   - **(a) Recommended:** a fixed price per item inside the DMG's rarity range (common 50–100 gp, uncommon 101–500, rare 501–5,000, very rare 5,001–50,000). Potions follow their rarity too: Healing stays 50 gp (the PHB price), then Greater 150, Superior 600, and Supreme 6,000.
   - **(b)** One flat price per rarity: 100, 500, 5,000, and 50,000 gp.
6. **Charged items** (wands of *magic missiles*, staffs):
   - **(a) Recommended:** not in this round. They need charges saved per item and a recharge at dawn; that's a follow-up.
   - **(b)** Include the Wand of Magic Missiles now, with charges stored on the inventory entry and recharged on a long rest.
