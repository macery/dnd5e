# Reactions

Status: **done** (2026-10-02). As with the last few features, the questions were settled with the recommended answers without a review round, and they're open to revisiting.

## The problem

- **Reactions happen on other creatures' turns:** an attack hits you, and you cast *Shield*.
- **Monsters' turns resolve automatically.** There's no point at which to ask the player, and stopping a monster's turn halfway would mean restructuring the turn engine around waiting for input.

## Decision: automatic reactions

Each character's reaction is taken for them when it clearly helps, with one switch to turn it off:

- ***Shield*** (Sorcerer, Wizard; 1st level):
  - When an attack hits and +5 AC would make it miss, the character casts *Shield* with their lowest slot.
  - The +5 AC lasts until the start of their next turn, against every attack.
  - It's never cast on a critical hit, or when +5 wouldn't be enough, so no slot is wasted.
- **Uncanny Dodge** (Rogue 5): halves the damage of an attack that hits.
- ***Hellish Rebuke*** (Warlock; 1st level): when an attack damages the warlock, the attacker makes a DEX save against 2d10 fire (+1d10 per slot level above 1st), taking half on a success.

Every character gets one reaction per round, and it comes back at the start of their turn. All of this only applies to monsters' attacks.

The combat screen's **Automatic reactions** checkbox (on by default) turns them off for the fight, to save slots.

## Not yet

- **Opportunity attacks:** there's no map, so no one ever leaves anyone's reach.
- ***Counterspell***: monsters don't cast spells.
- ***Feather Fall***: there's no falling.
- **Choosing reactions by hand.** That would need monster turns to pause for input.

## As built

- `Combatant::reaction_used`, reset in `start_turn`.
- `Encounter::set_auto_reactions` and `auto_reactions`.
- In `weapon_attack`, when a monster attacks a character: *Shield* before damage, Uncanny Dodge after resistances, and *Hellish Rebuke* after the damage lands. These use the helpers `reaction_spell`, `spend_lowest_slot`, and `hellish_rebuke`.
- Migration 27 seeds *Shield* (Utility; Reaction) and *Hellish Rebuke* (a DEX save, 2d10 fire; Reaction). Reaction spells can't be cast on your own turn, or from the Spellbook.
- Tests: `tests/test_reactions.cpp`.
