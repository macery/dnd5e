#pragma once

#include <optional>
#include <string>

#include "dnd/spell.hpp"

namespace dnd {

// What a creature has left on its turn (2014 PHB ch. 9): one action and one
// bonus action, plus extras from features (Action Surge adds an action;
// Extra Attack makes one Attack action several attacks).
struct TurnState {
    int actions_left = 1;
    // Attacks still to make in an Attack action already started (Extra
    // Attack). Taking any other action gives them up.
    int attacks_left = 0;
    bool bonus_action_used = false;
    // For the bonus-action spell rule (PHB ch. 10).
    bool bonus_action_spell_cast = false;
    bool levelled_action_spell_cast = false;
    // Class features.
    bool attack_action_taken = false;     // took the Attack action this turn
    bool attack_action_martial = false;   // ...with an unarmed strike or monk weapon (Martial Arts)
    bool sneak_attack_used = false;       // once per turn
    bool action_surge_used = false;       // once per turn
    bool reckless = false;                // Reckless Attack: advantage on STR melee attacks this turn
    bool martial_advantage_used = false;  // a Hobgoblin's once-per-turn extra damage
    bool colossus_slayer_used = false;    // a Hunter's once-per-turn extra damage
};

// Why `spell` can't be cast right now, or nullopt if it can. The spell's own
// casting time decides whether it takes the action or the bonus action.
// PHB ch. 10: after casting a spell as a bonus action, the only other spell
// you can cast that turn is a cantrip with a casting time of 1 action -- so a
// bonus-action spell also can't follow a levelled spell cast with an action.
// Reactions aren't supported yet.
std::optional<std::string> spell_casting_blocked(const TurnState& turn, const Spell& spell);

// Records that `spell` was cast (spending the action or bonus action).
void record_spell_cast(TurnState& turn, const Spell& spell);

}  // namespace dnd
