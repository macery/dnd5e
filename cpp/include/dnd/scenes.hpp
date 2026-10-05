#pragma once

#include <string>
#include <vector>

#include "dnd/adventure.hpp"
#include "dnd/adventure_run.hpp"
#include "dnd/checks.hpp"
#include "dnd/dice.hpp"
#include "dnd/repository.hpp"

namespace dnd {

// Scenes between fights (docs/scenes-and-npcs.md): their options, what
// their outcomes do to the party and the run, traps, and how they change
// the next fight.

// The options a Challenge or Choice scene offers this run: those whose
// required flag (if any) the run has unlocked.
std::vector<const SceneOption*> available_options(const AdventureScene& scene, const AdventureRun& run);

// The party, ready to roll checks: each living member with class and pack.
std::vector<CheckRoller> party_rollers(const Repository& repo, const std::vector<int>& party);

// Applies an outcome: damage (to `roller_id`, or everyone), exhaustion,
// inspiration, loot and coins (saved to the database), and what it changes
// in the run (flags, the next fight). Returns lines for the player. The
// caller records the scene and saves the run.
std::vector<std::string> apply_outcome(Repository& repo, AdventureRun& run, const SceneOutcome& outcome,
                                       int roller_id, const Dice& dice);

// The party's best passive Perception: who has it, and the score.
struct BestPassive {
    int character_id = 0;
    std::string name;
    int score = 0;
};
BestPassive best_passive(const std::vector<CheckRoller>& party, const std::string& what);

// Springs a trap on `victim_id` (or everyone, for a trap that catches the
// party): each makes the save for half damage. Saves HP; returns the lines.
std::vector<std::string> spring_trap(Repository& repo, const SceneTrap& trap, const std::vector<int>& party,
                                     int victim_id, const Dice& dice);

// Before a fight: what earlier scenes did to it. Removes or adds monsters
// (never fewer than one in all; reinforcements join the first group) and
// returns the surprise, clearing all of it from the run. `skip` reports
// whether the fight is skipped altogether.
Surprise shape_next_fight(AdventureRun& run, AdventureScene& scene, bool& skip);

}  // namespace dnd
