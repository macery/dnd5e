# Core class features, levels 1–5

Status: **all three phases done.** Every recommendation was taken on 2026-10-05.

**Phase 3 as built (Wild Shape):**
- **Migration 30** seeds five SRD beasts that walk: Boar, Panther, Giant Badger, Black Bear, and Ape. They join the Wolf and Dire Wolf, and fight as monsters too.
- **Rules:** `Feature::WildShape` (two uses per short rest) and `wild_shape_max_cr` (1/4, 1/2 at 4, 1 at 8).
- **Engine:**
  - `set_wild_shape_beasts`, `wild_shape_forms`, `wild_shape`, and `revert_wild_shape`.
  - The beast's HP, AC, DEX, and attacks replace the druid's. The druid's own are kept in `Combatant::druid_form`.
  - The Attack action uses the beast's attacks, Multiattack included.
  - Physical saves use the beast's scores plus the druid's proficiencies.
  - No spellcasting in beast form. Character HP isn't synced while shaped.
  - At 0 HP the druid reverts and extra damage carries over. A bonus action reverts sooner, and the fight's end reverts too.
- **Combat UI:** a **Wild Shape** row (forms with CR, HP, and AC, and **Transform**). The participant table shows "Fern (Black Bear)".
- **Tests:** Wild Shape in `tests/test_class_features_1_5.cpp` and `tests/ui/test_class_options_ui.cpp`.

**Phase 2 as built:**
- **`dnd/class_options`:**
  - Every SRD Eldritch Invocation with its level and prerequisite (Eldritch Blast, or a pact). `invocations_known` gives the count.
  - Pact Boons and Expertise counts.
  - `class_choices_due` (Metamagic, Pact Boon, Invocations, Pact of the Tome cantrips, Expertise, and Lore bonus skills) and `apply_class_choice`.
- **Applied:**
  - Expertise doubles proficiency in `skill_bonus`.
  - Armor of Shadows gives unarmored AC 13 + DEX.
  - Pact of the Blade makes a warlock proficient with their melee weapon.
  - Thirsting Blade gives two attacks at level 5.
  - Agonizing Blast adds +CHA per beam.
  - Tome cantrips are always ready (`spells_ready`), and the Spellbook marks them.
- **UI:**
  - `ClassOptionsDialog` shows a checklist per due choice and checks the counts.
  - The main window gets a **Class Options…** button, and the dialog opens on its own after Level Up and creation. The Status column says "Choices due".
  - The sheet lists Metamagic, Pact Boon, Invocations, Book of Shadows, and Expertise.
- **Tests:** class choices in `tests/test_class_features_1_5.cpp`, and `tests/ui/test_class_options_ui.cpp`.

**Phase 1 as built:**
- **Migration 29:** `Characters.ClassOptions` stores the class choices ("Metamagic", "Invocations", "PactBoon", "Expertise", ...), and exports carry them.
- **Bard:**
  - Jack of All Trades: `jack_of_all_trades`, applied to initiative in combat, and to initiative and non-proficient skills on the sheet.
  - Song of Rest: `song_of_rest_die`. `short_rest` takes the die, and the Short Rest dialog uses the best living bard in the group.
- **Barbarian:** Danger Sense gives advantage on DEX saves unless blinded, deafened, or incapacitated (`Encounter::save_modifiers`).
- **Monk:**
  - Deflect Missiles is an automatic reaction against ranged monster attacks. No seeded monster uses a ranged attack first yet, so it's tested with a custom archer.
  - Stunning Strike is `AttackOptions::stunning_strike` plus a combat checkbox. It spends ki on a hit, and the target is stunned until the end of the monk's next turn.
- **Sorcerer:**
  - Font of Magic: sorcery points as a limited feature, `create_spell_slot` (created slots are negative "used" counts, gone after a long rest), and `convert_slot_to_points` (capped).
  - In combat, a bonus action creates a slot. The Spellbook converts either way.
  - Metamagic: `Encounter::metamagic_options` and `cast_spell(..., Metamagic)`. Quickened casts as a bonus action; Twinned gives a single-target spell a second target and projectile; Empowered rerolls low damage dice in the core rolls; Heightened gives the first target disadvantage on its save. Careful, Distant, Extended, and Subtle spend their points and are shown only. A Metamagic picker appears under Cast a Spell.
- **Tests:** `tests/test_class_features_1_5.cpp`.

Rules follow the 2014 Player's Handbook, limited to what the SRD 5.1 includes (as with subclasses).

## Where things stand

Done for levels 1–5:
- Rage, Reckless Attack, and Extra Attack;
- Bardic Inspiration;
- Second Wind, Action Surge, and Fighting Styles;
- Martial Arts, Ki (Flurry of Blows, Patient Defense, Step of the Wind), and Unarmored Defense;
- Lay on Hands and Divine Smite;
- Sneak Attack, Cunning Action, and Uncanny Dodge;
- Arcane Recovery, Channel Divinity, spellcasting and slots, and the subclasses.

Still missing or shown only, by class:

| Class | Feature (level) | Plan |
|---|---|---|
| Barbarian | Danger Sense (2) | Advantage on DEX saves unless blinded, deafened, or incapacitated |
| Bard | Jack of All Trades (2) | Half proficiency on ability checks without proficiency, which includes initiative |
| Bard | Song of Rest (2) | +1d6 healing (more at higher levels) for allies who spend hit dice on a short rest |
| Bard, Rogue | Expertise (3 / 1) | Double proficiency in two chosen skills (question 7) |
| Druid | **Wild Shape** (2) | Turn into a beast from the seeded stat blocks (question 5) |
| Monk | Deflect Missiles (3) | An automatic reaction against ranged weapon attacks: reduce the damage by 1d10 + DEX + monk level |
| Monk | **Stunning Strike** (5) | On a hit, 1 ki: the target makes a CON save or is stunned until the end of the monk's next turn |
| Sorcerer | **Font of Magic** (2) | Sorcery points (= sorcerer level), turning slots into points and points into slots |
| Sorcerer | **Metamagic** (3) | Two options to start, three at 10 (question 2) |
| Warlock | **Eldritch Invocations** (2) | Two at level 2, three at 5 (question 3) |
| Warlock | **Pact Boon** (3) | Blade, Chain, or Tome (question 4) |
| Ranger | Favored Enemy and Natural Explorer (1), Primeval Awareness (3) | Shown only: they're exploration and tracking |
| Paladin | Divine Sense (1), Divine Health (3) | Shown only (sensing, and immunity to disease) |
| Monk | Unarmored Movement (2), Slow Fall (4) | Shown only (movement and falling) |

## Goals

- **Automate everything in the "Plan" column** that combat, rests, or the sheet can apply.
- **New choices go in Level Up** (Metamagic, invocations, Pact Boon, Expertise) **and in character creation** (a Rogue's Expertise at level 1).
- **Characters already past these levels** choose with a **Choose Class Options…** step, the same way as Choose Subclass.
- **Tests** for every automated feature, plus UI tests.

## Non-goals for now

- **Features above level 5.**
- **Feats** (question 1).
- **Movement, falling, exploration, and senses.**

## Build phases

1. **Combat and rests:** Danger Sense, Jack of All Trades, Song of Rest, Deflect Missiles, Stunning Strike, Font of Magic, and Metamagic.
2. **Warlock and the choices:** invocations, Pact Boon, and Expertise, plus Level Up, creation, and Choose Class Options.
3. **Wild Shape**, if question 5 keeps it in.

## Open questions

1. **Feats.** The PHB offers a feat in place of an Ability Score Improvement, but the SRD has only one feat, Grappler.
   - **(a) Recommended:** skip feats, consistent with keeping to the SRD.
   - **(b)** Write a handful in our own words (Alert, Tough, Great Weapon Master, Sharpshooter, War Caster).
2. **Metamagic options:**
   - **(a) Recommended:** automate these, and show the rest (Distant, Extended, Subtle) by name only:
     - **Quickened** (2 points): an action spell as a bonus action;
     - **Twinned** (points = spell level): a single-target spell hits a second target;
     - **Empowered** (1 point): reroll up to CHA-modifier damage dice;
     - **Heightened** (3 points): one target has disadvantage on its first save;
     - **Careful** (1 point): allies automatically succeed on the save. Rarely matters here, because area spells only hit the targets you choose.
   - **(b)** Only Quickened and Twinned at first.
3. **Eldritch Invocations:**
   - **(a) Recommended:** offer every SRD invocation a level-5 warlock qualifies for. Automate:
     - **Agonizing Blast** (+CHA to Eldritch Blast damage);
     - **Armor of Shadows** (*Mage Armor* at will: unarmored AC 13 + DEX);
     - **Thirsting Blade** (Pact of the Blade: two attacks at level 5);
     - **Eldritch Sight** and **Devil's Sight**, shown only.

     The rest are shown only (Repelling Blast's push, Eldritch Spear's range, Beguiling Influence's skills, ...).
   - **(b)** Only Agonizing Blast and Armor of Shadows.
4. **Pact Boon:**
   - **(a) Recommended:**
     - **Pact of the Blade:** proficiency with the equipped melee weapon, and it counts as magical;
     - **Pact of the Tome:** three cantrips from any class's list;
     - **Pact of the Chain:** chosen and shown; the familiar isn't simulated.
   - **(b)** Blade only.
5. **Wild Shape:**
   - **(a) Recommended:** include it, simplified.
     - A druid becomes a beast of CR ≤ 1/4 (≤ 1/2 at level 4, no flying or swimming speeds), using seeded beast stat blocks: the Wolf, plus a few new SRD beasts such as the Boar, Panther, and Black Bear.
     - The beast's HP, AC, and attacks take over. Damage past 0 carries back to the druid.
     - Two uses per short rest. No spellcasting while shapechanged.
   - **(b)** Leave Wild Shape for its own design round. It touches combat more deeply than anything else here.
6. **Font of Magic conversions:**
   - **(a) Recommended:** converting a slot into points works outside combat (the Spellbook). Creating a slot from points is a bonus action in combat, or free outside it.
   - **(b)** Only in the Spellbook.
7. **Expertise (and the Lore Bard's bonus skills):**
   - **(a) Recommended:** chosen in creation (Rogue) and Level Up (Bard 3, Rogue 6), stored on the character, and shown on the sheet as double proficiency. Lore Bards' three bonus proficiencies get the same picker.
   - **(b)** Set by hand in Edit Character.
8. **Stunning Strike:**
   - **(a) Recommended:** a "Stunning Strike (1 ki)" checkbox before the attack, like Divine Smite. Ki is spent only on a hit.
   - **(b)** Automatic on every hit while ki lasts.
