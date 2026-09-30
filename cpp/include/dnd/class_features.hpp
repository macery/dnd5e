#pragma once

#include <string>

namespace dnd {

// Class features (2014 PHB ch. 3); see docs/class-features.md. Pure rules,
// no state -- the encounter and rests apply them.

// Attacks per Attack action: 2 from level 5 for Fighters, Barbarians,
// Paladins, Rangers, and Monks (Extra Attack); Fighters get 3 at level 11
// and 4 at level 20. Everyone else: 1.
int attacks_per_attack_action(const std::string& class_name, int level);

}  // namespace dnd
