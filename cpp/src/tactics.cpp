#include "dnd/tactics.hpp"

#include <algorithm>
#include <array>

#include "dnd/encounter.hpp"

namespace dnd {

const char* mind_name(Mind mind) {
    switch (mind) {
        case Mind::Simple: return "Simple";
        case Mind::Cunning: return "Cunning";
        case Mind::Tactical: return "Tactical";
    }
    return "Simple";
}

const char* mind_description(Mind mind) {
    switch (mind) {
        case Mind::Simple: return "Fights by instinct: any target will do.";
        case Mind::Cunning: return "Fights cunningly: goes after the wounded.";
        case Mind::Tactical: return "Fights tactically: goes after the wounded, spellcasters, and anyone concentrating.";
    }
    return "";
}

Mind creature_mind(const Combatant& creature) {
    static const std::array<const char*, 4> kInstinct = {"Beast", "Construct", "Ooze", "Plant"};
    if (std::find(kInstinct.begin(), kInstinct.end(), creature.creature_type) != kInstinct.end()) return Mind::Simple;
    int intelligence = creature.ability_scores[3];
    if (intelligence <= 5) return Mind::Simple;
    if (intelligence <= 11) return Mind::Cunning;
    return Mind::Tactical;
}

bool is_spellcaster(const Combatant& combatant) {
    static const std::array<const char*, 6> kCasters = {"Bard", "Cleric", "Druid", "Sorcerer", "Warlock", "Wizard"};
    if (combatant.spellcasting) return true;
    return combatant.is_character &&
           std::find(kCasters.begin(), kCasters.end(), combatant.class_name) != kCasters.end();
}

int target_weight(Mind mind, const Combatant& target) {
    if (mind == Mind::Simple) return 1;
    int weight = 2;
    if (target.max_hp > 0) {
        if (target.current_hp * 2 <= target.max_hp) weight += 3;  // bloodied
        if (target.current_hp * 4 <= target.max_hp) weight += 2;  // nearly down
    }
    if (mind == Mind::Tactical) {
        if (!target.concentration.empty()) weight += 4;
        if (is_spellcaster(target)) weight += 2;
    }
    return weight;
}

}  // namespace dnd
