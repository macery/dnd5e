#include "dnd/turn.hpp"

namespace dnd {

std::optional<std::string> spell_casting_blocked(const TurnState& turn, const Spell& spell) {
    switch (spell.casting_time) {
        case CastingTime::Reaction:
            return spell.name + " is cast as a reaction, and reactions aren't supported yet.";
        case CastingTime::BonusAction:
            if (turn.bonus_action_used) return "The bonus action has already been used this turn.";
            if (turn.levelled_action_spell_cast) {
                return "A bonus-action spell can only be paired with a cantrip, and a levelled spell was already "
                       "cast this turn.";
            }
            return std::nullopt;
        case CastingTime::Action:
            if (turn.actions_left <= 0) return "No action is left this turn.";
            if (turn.bonus_action_spell_cast && spell.level > 0) {
                return "After casting a bonus-action spell, only a cantrip can be cast this turn.";
            }
            return std::nullopt;
    }
    return std::nullopt;
}

void record_spell_cast(TurnState& turn, const Spell& spell) {
    if (spell.casting_time == CastingTime::BonusAction) {
        turn.bonus_action_used = true;
        turn.bonus_action_spell_cast = true;
        return;
    }
    turn.actions_left -= 1;
    turn.attacks_left = 0;
    if (spell.level > 0) turn.levelled_action_spell_cast = true;
}

}  // namespace dnd
