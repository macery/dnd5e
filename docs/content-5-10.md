# Content for levels 5–10

Status: **both phases done.** Every recommendation was taken on 2026-10-05.

**Phase 2 as built:**
- **Migration 32** seeds the twelve SRD stat blocks. The Veteran's crossbow and the Stone Giant's rock are their preferred attacks. The Wight's Multiattack is Life Drain plus a longsword.
- **Adventures:**
  - Three multi-fight runs: **The Barrow-King** (5), **Moonrise at Ashgrove** (6), and **The White Wyrm's Lair** (7, four fights).
  - Four single fights: **The Bulette Field** (5), **The Golem's Workshop** (6), **The Gorgon's Pasture** (6), and **The Green Wyrm of Thornwood** (8).
  - Bosses drop rare and very rare items: Longsword +2, Ring of Protection, Warhammer +2, Bracers of Defense, Amulet of Health, Plate Armor +1, Shield +2, Longbow +2, Chain Mail +2, and Superior and Supreme Healing.
- **Tests:** a smoke test fights every new monster to the end against a level-8 party with three seeds. The existing checks confirm that every adventure's monsters and loot exist and that its scenes scale.

**Phase 1 as built:**
- **Migration 31:**
  - New `Monsters` columns for nonmagical-weapon resistance (`NonmagicalWeapons`), the breath weapon (`Breath…`), and spellcasting (`CasterLevel`, `SpellSaveDC`, `SpellAttackBonus`, `SpellAbilityModifier`, `SpellSlots`, `Spells`).
  - New `MonsterAttacks` columns `Preferred` and `LifeDrainDC`, and `Characters.MaxHpReduction`.
- **Nonmagical weapons:**
  - `DamageTraits::nonmagical_weapons` ("resistant" or "immune"), and `damage_modifier(..., magical)` with the log noting "nonmagical".
  - `WeaponAttack::magical` covers +N weapons, a warlock's pact weapon, and a monk's unarmed strikes from level 6. Monsters' and beasts' attacks are nonmagical; spells always count as magical.
- **Preferred attacks:** a monster with any preferred attack uses only those (archers, rock-throwers).
- **Life Drain:** a CON save, or max HP drops by the damage. It's saved on the character (`max_hp_reduction`), and a long rest gives it back. Max HP reaching 0 kills.
- **Breath weapons:**
  - Used whenever ready. They recharge on a d6 at the start of the monster's turn and catch up to N random characters, with a save for half.
  - Petrifying breath restrains through a `SaveToEnd` with `petrify_on_failure`. Petrified characters are out of the fight: monsters ignore them, and they count as down for defeat.
- **Monster spellcasting:**
  - Each turn, in this order: an area damage spell at two or more characters; a control spell (Hold Person) at someone not held, while not concentrating; otherwise its strongest damage spell, falling back to cantrips. Spells are cast through the same `resolve_spell` as characters' spells.
  - A caster monster with Shield uses it as a reaction against weapon hits.
  - Player characters count as Humanoid for spells like Hold Person.
- **Tests:** `tests/test_monster_abilities.cpp`.

Monsters come from the SRD 5.1, with mechanics as in the 2014 Monster Manual and DMG.

## Where things stand

- **24 monsters**, CR 1/8 to 5: the hardest are the Troll and the Hill Giant (CR 5). Five are beasts added for Wild Shape.
- **Monsters only melee.**
  - Each uses its first attack, or its Multiattack.
  - None casts spells, breathes, or drains life.
  - None resists nonmagical weapons, so magic weapons only add their bonus.
- **The hardest adventure** is The Hill Giant's Toll, aimed at level 5.
- **Characters now reach level 5 fully kitted:** subclasses, Extra Attack, 3rd-level spells, Metamagic, invocations, Wild Shape, magic items, and reactions. There's nothing new to fight beyond that.

## Goals

- **About a dozen new monsters, CR 2–8,** each bringing something the fights don't have yet (question 1):

  | Monster | CR | New here |
  |---|---|---|
  | Cult Fanatic | 2 | A caster: *Sacred Flame*, *Inflict Wounds*, *Hold Person*, *Shield of Faith* |
  | Veteran | 3 | A heavy crossbow at range |
  | Wight | 3 | Resists nonmagical weapons; Life Drain lowers max HP; a longbow |
  | Werewolf | 3 | Immune to nonmagical weapons |
  | Wraith | 5 | Resists nonmagical weapons; Life Drain |
  | Bulette | 5 | A heavy melee brute (its burrowing leap is shown only) |
  | Flesh Golem | 5 | Immune to nonmagical weapons; goes berserk when hurt |
  | Gorgon | 5 | A recharging breath weapon that petrifies |
  | Mage | 6 | A caster: *Fire Bolt*, *Magic Missile*, *Shield* (as a reaction), *Fireball* |
  | Young White Dragon | 6 | Cold breath (recharge 5–6), Multiattack |
  | Stone Giant | 7 | Rocks thrown at range |
  | Young Green Dragon | 8 | Poison breath, a boss |

- **New monster abilities:**
  - **Recharge abilities:** a breath weapon is used when it's ready and comes back on a d6 roll of 5–6 at the start of the monster's turn. It hits several characters with a save for half (question 6).
  - **Ranged attacks:** monsters that prefer range (archers, rock-throwers) use their ranged attack (question 8). This also brings the Monk's Deflect Missiles into play.
  - **Resistance or immunity to nonmagical weapons:** weapon attacks without magic don't get through (question 2).
  - **Life Drain:** a hit lowers the target's max HP until a long rest. A character whose max HP hits 0 dies (question 5).
  - **Monster spellcasting:** casters have slots and a short list of spells the app already handles. They pick the best one each turn: a big area spell when several characters stand, *Hold Person* on a fighter, and so on (question 4).
- **Three multi-fight adventures for levels 5–8** (question 7). Each ends in a boss that drops rare magic items: +2 weapons, +1 armor, the Amulet of Health, Bracers of Defense, the Ring of Protection.
- **Tests:** each ability, and the new adventures scaling and running end to end.

## Non-goals for now

- **Lair actions, legendary actions, and flying or burrowing movement** (there's no map).
- **Petrification as a lasting condition outside combat.** A petrified character counts as down for the rest of the fight, and recovers after it (question 6).
- **Silvered weapons.**
- **Monsters above CR 8.**

## Build phases

1. **Monster abilities:** ranged preference, damage from nonmagical weapons, Life Drain, recharge breath weapons, and spellcasting, each with tests.
2. **The monsters and adventures:** stat blocks (a migration), the three adventures and their loot, scaling checks, UI tests, docs, and the release.

## Open questions

1. **The monster list:**
   - **(a) Recommended:** the twelve above.
   - **(b)** A smaller first batch of six (Veteran, Wight, Werewolf, Mage, Young White Dragon, Stone Giant).
2. **What counts as magical** against nonmagical resistance or immunity:
   - **(a) Recommended:** spells; +N weapons; a warlock's pact weapon; a Monk's unarmed strikes from level 6 (Ki-Empowered Strikes, listed for later); and Divine Smite's and Divine Favor's radiant damage. Everything else is nonmagical.
   - **(b)** Only spells and +N weapons.
3. **Werewolf:** the Monster Manual makes it immune to nonmagical weapons unless they're silvered.
   - **(a) Recommended:** immune to nonmagical weapons, with silvering left out. Spells and magic weapons are the answer.
   - **(b)** Add silvered weapons to the Shop (100 gp to silver one).
4. **Monster spellcasting:**
   - **(a) Recommended:** casters get per-day slots and a short list of automated spells.
     - Each turn the monster picks: an area spell (*Fireball*) against two or more characters, *Hold Person* against a character without it, a damage spell otherwise, and its weapon when out of slots.
     - *Shield* as a reaction, like the characters'.
   - **(b)** Casters only use their damage spells (no control spells or *Shield*).
5. **Life Drain** (Wight, Wraith): a hit's necrotic damage also lowers the target's max HP by that much until a long rest.
   - **(a) Recommended:** implement it, with a saved max HP reduction cleared by a long rest. A CON save (DC 13 Wight, 14 Wraith) avoids the reduction.
   - **(b)** Only the necrotic damage.
6. **Breath weapons and petrification:**
   - **(a) Recommended:**
     - Breath weapons hit up to four characters (a cone, without a map), with a save for half.
     - The Gorgon's breath works like the PHB's petrification: a failed CON save restrains, and a second failed save petrifies. A petrified character counts as down for the rest of the fight and recovers when it ends (*greater restoration* isn't in the app).
   - **(b)** Leave the Gorgon out, and keep breath weapons to damage only.
7. **The adventures:**
   - **(a) Recommended:**
     - **The Barrow-King** (level 5): wights, then a wraith in the old king's tomb.
     - **Moonrise at Ashgrove** (level 6): werewolves and a cult fanatic, in a village that hides them.
     - **The White Wyrm's Lair** (level 7): a mage and a stone giant in the dragon's service, then the Young White Dragon.

     The Young Green Dragon is kept for a level-8 single fight.
   - **(b)** Suggest other themes.
8. **Ranged monsters:**
   - **(a) Recommended:** a per-attack "preferred" flag. Archers and rock-throwers use their ranged attack every turn, since there's no map to make them close in.
   - **(b)** Ranged attacks only while their target is a ranged attacker too.
