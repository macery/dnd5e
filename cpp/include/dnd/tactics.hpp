#pragma once

#include <string>

namespace dnd {

struct Combatant;

// How monsters (and NPC allies) choose whom to attack (docs/monster-ai.md).
// Each kind of mind weights the targets it can reach; a roll still decides,
// so smarter creatures are likelier, not certain, to pick well.

enum class Mind {
    Simple,    // beasts, constructs, and anything with INT 5 or less: any target will do
    Cunning,   // INT 6-11: finishes off the wounded
    Tactical,  // INT 12+: also goes after spellcasters, and anyone concentrating
};
const char* mind_name(Mind mind);
// "Fights cunningly: goes after the wounded.", for the status tooltip.
const char* mind_description(Mind mind);

// From the stat block: Beast, Construct, Ooze, and Plant creatures are Simple
// whatever their Intelligence; otherwise INT decides.
Mind creature_mind(const Combatant& creature);

// A character of a spellcasting class, or a creature with spellcasting.
bool is_spellcaster(const Combatant& combatant);

// How much a creature with this mind wants `target`: 1 for every target with
// a Simple mind. Otherwise 2, +3 when the target is at half its hit points or
// less, +2 more at a quarter; Tactical minds add +4 for concentration and +2
// for a spellcaster.
int target_weight(Mind mind, const Combatant& target);

}  // namespace dnd
