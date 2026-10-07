#pragma once

#include <optional>
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

// The party, ready to roll checks: each conscious member with class and pack.
std::vector<CheckRoller> party_rollers(const Repository& repo, const std::vector<int>& party);

// Applies an outcome: damage (to `roller_id`, or everyone), exhaustion,
// inspiration, loot and coins, an NPC's attitude (saved to the database),
// and what it changes in the run (flags, the next fight). `scene_npc` is the
// key of the scene's NPC, for outcomes that change "their" attitude.
// Returns lines for the player. The caller records the scene and saves the
// run.
std::vector<std::string> apply_outcome(Repository& repo, AdventureRun& run, const SceneOutcome& outcome,
                                       int roller_id, const Dice& dice, const std::string& scene_npc = "");

// --- Social scenes and NPCs -----------------------------------------------------

// The NPC a run's campaign knows by `key`: an adventure's (met for the
// first time from its template), or "merchant" / "priest". Nothing for a key
// the adventure doesn't define.
std::optional<Npc> find_run_npc(Repository& repo, const AdventureRun& run, const std::string& key);

// The skills a Social option can be tried with: its own list, or
// Persuasion, Deception, and Intimidation.
std::vector<std::string> social_approaches(const SceneOption& option);

// An NPC in a fight, with the stat block they fight with.
struct FightNpc {
    Npc npc;
    Monster stat_block;
};
// The NPCs who fight beside the party in the run's next fight: those an
// earlier outcome brought along who are alive, not Hostile, and have a stat
// block among `monsters`.
std::vector<FightNpc> next_fight_allies(Repository& repo, const AdventureRun& run,
                                         const std::vector<Monster>& monsters);
// The NPCs who have turned on the party and fight among the monsters next:
// those still alive, with a stat block among `monsters`.
std::vector<FightNpc> next_fight_foes(Repository& repo, const AdventureRun& run,
                                       const std::vector<Monster>& monsters);

// Marks the NPCs (by id) as dead, in whatever campaign they're in.
void mark_npcs_dead(Repository& repo, int campaign_id, const std::vector<int>& npc_ids);

// Reading an NPC (Insight, DMG ch. 8): on a success the party learns what
// they want and fear, which lowers later DCs with them (saved).
CheckResult read_npc(Repository& repo, Npc& npc, const CheckRoller& roller, int dc, const Dice& dice,
                     bool helped = false, bool inspired = false);

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
// returns the surprise. The run keeps all of it until record_fight, so a
// fight left unfinished is shaped the same way on resume. `skip` reports
// whether the fight is skipped altogether.
Surprise shape_next_fight(const AdventureRun& run, AdventureScene& scene, bool& skip);

}  // namespace dnd
