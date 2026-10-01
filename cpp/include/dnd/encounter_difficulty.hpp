#pragma once

#include <array>
#include <string>
#include <vector>

#include "dnd/adventure.hpp"
#include "dnd/monster.hpp"

namespace dnd {

// Encounter difficulty from the 2014 encounter-building rules (Dungeon
// Master's Guide ch. 3, also in the free Basic Rules); see
// docs/monsters-and-encounters.md.

enum class Difficulty { Trivial, Easy, Medium, Hard, Deadly };
const char* difficulty_name(Difficulty difficulty);

// One character's Easy / Medium / Hard / Deadly XP thresholds. Levels are
// clamped to [1, 20].
std::array<int, 4> xp_thresholds(int level);

// The XP multiplier for the number of monsters: x1 for 1, x1.5 for 2, x2 for
// 3-6, x2.5 for 7-10, x3 for 11-14, x4 for 15+. One step higher for a party
// of fewer than 3, one step lower for 6 or more (x0.5 to x5).
double encounter_multiplier(int monster_count, int party_size);

struct EncounterRating {
    int base_xp = 0;      // the monsters' XP (what's awarded)
    double multiplier = 1.0;
    int adjusted_xp = 0;  // base x multiplier: what's compared with the thresholds
    std::array<int, 4> party_thresholds{};  // summed over the party
    Difficulty difficulty = Difficulty::Trivial;  // Trivial: below Easy

    // "Hard: 1,100 adjusted XP (Easy 300, Medium 600, Hard 900, Deadly 1,400)".
    std::string text() const;
};

// `monster_xp` is each monster's XP (one entry per monster).
EncounterRating rate_encounter(const std::vector<int>& party_levels, const std::vector<int>& monster_xp);

struct ScaledAdventure {
    AdventureTemplate adventure;  // with monster counts adjusted
    EncounterRating rating;
    bool scaled = false;          // counts differ from the template's
};

// Adjusts an adventure's monster counts toward Medium-Hard for this party:
// adds monsters (the cheapest group first) while it's below Medium and the
// next one wouldn't make it Deadly, and removes them (the most expensive
// group with more than one first) while it's Deadly. Every group keeps at
// least one monster, so a party can still face a Deadly fight. Unknown
// monster names count as 0 XP; an empty party leaves it unchanged.
ScaledAdventure scale_adventure(const AdventureTemplate& adventure, const std::vector<int>& party_levels,
                                const std::vector<Monster>& monsters);

}  // namespace dnd
