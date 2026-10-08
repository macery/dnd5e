# Monster AI: choosing targets

Status: **built** (2026-10-08).

## Before
Monsters already used a breath weapon when it was ready, cast spells (area spells when two or more targets were standing, then control, then damage), ignored dying characters, and wouldn't attack whoever charmed them. Otherwise they attacked **a random conscious character**. So a goblin was as likely to swing at the fresh fighter as at the wizard on 2 HP, and a mage never went after the cleric holding *Bless*.

## The rule
Each creature has a **mind**, from its stat block (`dnd/tactics.hpp`):

| Mind | Who | Picks |
|---|---|---|
| **Simple** | Beasts, constructs, oozes, plants, and anything with INT 5 or less (wolves, zombies, ogres, hill giants) | Any target: a plain random pick, as before. |
| **Cunning** | INT 6–11 (goblins, orcs, gnolls, skeletons, ghouls, veterans, wights, werewolves, the young white dragon) | Favors the wounded. |
| **Tactical** | INT 12+ (bandit captains, mages, wraiths, spies, nobles, the young green dragon) | Favors the wounded, spellcasters, and anyone concentrating. |

**Smarter is likelier, not certain.** Every target the creature can reach gets a weight, and a roll picks among them. Fights stay unpredictable, and no one is focused down every time.
- Every target is worth 2.
- +3 at half its hit points or less, and +2 more at a quarter.
- Tactical only: +4 if it's concentrating, and +2 if it's a spellcaster (a Bard, Cleric, Druid, Sorcerer, Warlock, or Wizard, or a creature with spellcasting).
- A Simple mind weighs everyone at 1.

For example, a goblin facing a fresh fighter and a wizard on a quarter of her HP picks the wizard 7 times in 9.

When every weight is equal, the roll is the same plain d(n) as before. So fights where no one is hurt or concentrating play exactly as they used to.

**Where it applies:**
- **Attacks:** the target, and the next target when a multiattack drops the first one.
- **Single-target spells:** the target.
- **Area spells and control spells:** the order in which the caster considers targets, so a *Fireball* that can't catch everyone catches the ones it wants most, and *Hold Person* goes to its best target first.
- **Not breath weapons:** without a map there's no aiming a cone, so they still catch targets at random.

**NPC allies** use the same minds against the monsters, so Dagna (a Veteran, Cunning) finishes off wounded ogres.

**The status tooltip** says how each creature fights ("Fights cunningly: goes after the wounded.").

## Not done
- **Difficulty:** the encounter rating still assumes random targets. Smarter monsters make fights harder, Cunning ones mostly by finishing off characters who are already hurt. Watch it in playtests before adjusting the ratings or the scaling.
- **No positions:** with no map, there's no flanking, no protecting the back line, and no retreating.
- **The model-driven version** ([ai-tactics.md](ai-tactics.md)) is shelved. If it comes back, it can replace the weighted pick for Tactical minds.
