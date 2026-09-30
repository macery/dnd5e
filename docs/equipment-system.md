# Equipment design: weapons, armor, and AC

Status: **approved, ready for phase 1.** Nothing here is implemented yet. The open questions were settled on 2026-09-30; see [Decisions](#decisions).

This covers the TODO items **Equipping & weapons in the inventory**, **Armor & shields**, and **Weapon proficiencies**. It also lays the groundwork for *Mage Armor*, magic items, and encumbrance. Rules follow the **2014 PHB**: chapter 5 (Equipment) for the armor and weapon tables, and chapter 3 for class proficiencies and starting equipment.

## Where things stand

- **AC is a stored number**, `Character::armor_class`. The creation wizard sets it once to 10 + DEX, or to Unarmored Defense for Barbarians and Monks. The editor lets you type anything. Combat and the sheet read it as-is.
- **Weapons are a separate table.** There are 12 weapons, with only damage dice, finesse, and ranged. A character holds exactly one, through `Character::weapon_id`. "Unarmed Strike" is a fake 1d1 weapon row.
- **Everyone is proficient with every weapon**: attack bonuses always add the proficiency bonus.
- **Nothing is equipped.** The inventory is a list of stacks, with no notion of wearing or wielding.

## Goals

- **Weapons, armor, and shields are ordinary inventory items**, and characters equip them from the Inventory dialog.
- **AC is always calculated** from the worn armor, DEX, a shield, and class features, then shown on the sheet with a breakdown.
- **Weapon attacks use the wielded weapon**, adding proficiency only when the character is proficient with it. Versatile weapons are stronger in two hands.
- **Wearing armor you aren't proficient with has the PHB's cost.**
- **New characters start with their class's equipment.**

## Non-goals for now

- **Two-weapon fighting** (an off-hand attack is a bonus action), **thrown weapons**, **ammunition**, and **range** (there's no map).
- **Magic armor and weapons** (+1): the data model leaves room for them, but they're the *Magic items* TODO.
- **Encumbrance**, and **stealth**, where some armor gives disadvantage on Stealth. The data is recorded now, but nothing uses it yet.
- **Donning and doffing times** (1 to 10 minutes). Equipping is instant outside combat and not allowed during it.

## Data model

Weapons and armor become **items**, so they share the inventory, weight, cost, and loot. Their extra rules data goes in side tables keyed by `ItemID`, rather than a dozen mostly-empty columns on `Items`:

| Table | Columns |
|---|---|
| `Items` | adds the categories `Weapon`, `Armor`, and `Shield` (the column already exists) |
| `WeaponStats` | `ItemID`, `WeaponCategory` (Simple/Martial), `DamageDiceCount`, `DamageDiceSides`, `DamageType`, `VersatileDiceSides` (0 if not versatile), properties as flags: `Finesse`, `Light`, `Heavy`, `TwoHanded`, `Ranged`, `Thrown`, `Reach`, `Loading`, `Ammunition` |
| `ArmorStats` | `ItemID`, `ArmorType` (Light/Medium/Heavy/Shield), `BaseAC` (+2 for a shield), `MaxDexBonus` (-1 for no cap), `StrengthRequirement`, `StealthDisadvantage` |
| `CharacterInventory` | adds `Equipped` (0/1) |

**Migration:**
- Seed the PHB armor table and the shield, and move the 11 real weapons from `Weapons` into `Items` + `WeaponStats`, with their full PHB properties.
- Add a few weapons the class kits need, such as a Scimitar and Javelins.
- Stop using `Characters.WeaponID`, and turn each existing character's weapon into an equipped inventory item. That's one SQL statement; existing AC values are simply ignored from then on.
- The old `Weapons` table stays but goes unused. SQLite can't drop it cleanly inside the same migration history.

**Unarmed strike** stops being a fake weapon. It's the built-in attack when nothing is wielded: 1 + STR modifier bludgeoning damage, always proficient (PHB ch. 9).

### Armor (PHB ch. 5)

| Armor | Type | AC | STR | Stealth | Weight | Cost |
|---|---|---|---|---|---|---|
| Padded | Light | 11 + DEX | | disadvantage | 8 lb | 5 gp |
| Leather | Light | 11 + DEX | | | 10 lb | 10 gp |
| Studded leather | Light | 12 + DEX | | | 13 lb | 45 gp |
| Hide | Medium | 12 + DEX (max 2) | | | 12 lb | 10 gp |
| Chain shirt | Medium | 13 + DEX (max 2) | | | 20 lb | 50 gp |
| Scale mail | Medium | 14 + DEX (max 2) | | disadvantage | 45 lb | 50 gp |
| Breastplate | Medium | 14 + DEX (max 2) | | | 20 lb | 400 gp |
| Half plate | Medium | 15 + DEX (max 2) | | disadvantage | 40 lb | 750 gp |
| Ring mail | Heavy | 14 | | disadvantage | 40 lb | 30 gp |
| Chain mail | Heavy | 16 | 13 | disadvantage | 55 lb | 75 gp |
| Splint | Heavy | 17 | 15 | disadvantage | 60 lb | 200 gp |
| Plate | Heavy | 18 | 15 | disadvantage | 65 lb | 1,500 gp |
| Shield | Shield | +2 | | | 6 lb | 10 gp |

### Class proficiencies (PHB ch. 3), added to `ClassInfo`

| Class | Armor | Weapons |
|---|---|---|
| Barbarian | light, medium, shields | simple, martial |
| Bard | light | simple, hand crossbows, longswords, rapiers, shortswords |
| Cleric | light, medium, shields | simple |
| Druid | light, medium, shields (no metal) | clubs, daggers, darts, javelins, maces, quarterstaffs, scimitars, sickles, slings, spears |
| Fighter | all armor, shields | simple, martial |
| Monk | none | simple, shortswords |
| Paladin | all armor, shields | simple, martial |
| Ranger | light, medium, shields | simple, martial |
| Rogue | light | simple, hand crossbows, longswords, rapiers, shortswords |
| Sorcerer | none | daggers, darts, slings, quarterstaffs, light crossbows |
| Warlock | light | simple |
| Wizard | none | daggers, darts, slings, quarterstaffs, light crossbows |

Racial weapon training: the base Dwarf is proficient with battleaxes, handaxes, light hammers, and warhammers. The Elf's weapon training belongs to the High and Wood Elf subraces, which aren't modeled yet.

## Rules (pure functions in `dndcore`, with tests)

- **`calculate_armor_class(character, class, worn armor, shield) → ArmorClassBreakdown`:**
  - **No armor:** 10 + DEX. A Barbarian may use 10 + DEX + CON instead, and a Monk 10 + DEX + WIS; the higher total wins. A Monk loses Unarmored Defense while holding a shield.
  - **Light armor:** base + DEX. **Medium armor:** base + DEX, up to +2. **Heavy armor:** base only.
  - **Shield:** +2.
  - **Breakdown:** returns the parts, e.g. "Chain mail 16 + Shield 2 = 18", for the sheet and tooltips. Combat still adds effect bonuses like *Shield of Faith* on top.
- **`is_proficient(character, class, race, weapon)`:** checks the weapon's category or name against the lists above.
- **`weapon_attack_bonus` / damage:** adds proficiency only when proficient. Finesse and ranged weapons use DEX as they do today. A versatile weapon uses its larger die when no shield is worn.
- **Armor you aren't proficient with:** disadvantage on attack rolls, ability checks, and saves that use STR or DEX, and no spellcasting (PHB ch. 5). This goes through the effects system as a new modifier, so the log explains it.
- **Heavy armor below its STR requirement:** speed −10 ft. It's shown on the sheet, because speed isn't tracked in combat.
- **Equip rules:**
  - One armor, one shield, and one main weapon at a time.
  - A two-handed weapon can't be wielded with a shield.
  - No equipping during combat, since donning armor takes minutes; swapping weapons mid-fight can come later.

## UI

- **Inventory dialog:** Equip/Unequip buttons, an "Equipped" column, and a warning when the character isn't proficient with the item or doesn't meet a STR requirement.
- **Character sheet:**
  - AC with its breakdown as a tooltip.
  - The wielded weapon with its bonus, which now honours proficiency.
  - Speed, from the race, with the heavy-armor penalty if it applies.
- **Creation wizard:** the Equipment step offers the class's starting kit (see below), shows the resulting AC, and still lets you swap the weapon.
- **Character editor:** the AC and weapon fields become read-only "see Inventory" summaries (see question 1).
- **Combat:** unchanged on screen. The AC and attack numbers come from the equipment instead of stored values.

### Starting kits (PHB ch. 3, one fixed default per class)

| Class | Armor | Weapons |
|---|---|---|
| Barbarian | none (Unarmored Defense) | Greataxe, 2 Handaxes |
| Bard | Leather | Rapier, Dagger |
| Cleric | Scale mail, Shield | Mace, Light crossbow |
| Druid | Leather, Shield (wooden) | Quarterstaff |
| Fighter | Chain mail, Shield | Longsword, Light crossbow |
| Monk | none (Unarmored Defense) | Shortsword |
| Paladin | Chain mail, Shield | Longsword |
| Ranger | Scale mail | 2 Shortswords, Longbow |
| Rogue | Leather | Rapier, Shortbow, 2 Daggers |
| Sorcerer | none | Light crossbow, 2 Daggers |
| Warlock | Leather | Light crossbow, 2 Daggers |
| Wizard | none | Quarterstaff |

The PHB's "(a) or (b)" choices (e.g. a Fighter's leather armor and longbow instead) could come later as options in the wizard.

## Build phases

1. **Core and data:**
   - The migration, `WeaponStats` and `ArmorStats`, and the seeds.
   - `Equipped` on the inventory, with repository methods to equip and unequip.
   - Class and race proficiencies, `calculate_armor_class`, proficiency-aware attack bonuses, and the rules for armor you aren't proficient with.
   - Tests throughout.
2. **Integration and UI:**
   - `Encounter` reads the equipped weapon and armor; `Character::weapon_id` and the stored AC are retired.
   - Equip/Unequip in the Inventory dialog, AC breakdown and speed on the sheet, starting kits in the wizard, and read-only fields in the editor.
3. **Later:** two-weapon fighting, thrown weapons and ammunition, the PHB's kit choices, magic armor and weapons, encumbrance, and Stealth disadvantage.

## Decisions

Settled 2026-09-30:

1. **AC is purely calculated** from armor, DEX, a shield, and class features. The character editor shows it read-only, with no manual override. A "misc bonus" field can come later if something the app doesn't model needs one.
2. **Armor you aren't proficient with:** the Inventory dialog warns when you equip it, *and* combat enforces the PHB penalties: disadvantage on attack rolls, ability checks, and saves that use STR or DEX, and no spellcasting.
3. **Starting kits:** one fixed default per class (the table above), with the weapon still swappable in the wizard. The PHB's "(a) or (b)" choices come later.
4. **Weapon proficiency is part of this work.** Attack bonuses add proficiency only with weapons the class (or a base-race trait, like the Dwarf's) is trained in. This closes the *Weapon proficiencies* TODO.
5. **Existing characters:** migration converts each character's weapon into an equipped inventory item. Their AC becomes calculated from then on, so it may change.
