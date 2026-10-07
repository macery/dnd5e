#pragma once

#include <string>
#include <vector>

#include "dnd/adventure.hpp"

namespace dnd {

// One party's way through a multi-scene adventure (docs/adventures-and-currency.md):
// which scene is next, who's in the party, and how it ended. Character state
// (HP, spent resources) lives on the characters; this is only the place in
// the story, saved between scenes (Repository: AdventureRuns).

enum class AdventureStatus { InProgress, Finished, Retreated, Defeated };
const char* adventure_status_name(AdventureStatus status);
// Inverse of adventure_status_name; InProgress for unknown text.
AdventureStatus adventure_status_from_name(const std::string& name);

struct AdventureRun {
    int run_id = 0;            // 0 until saved
    std::string title;         // an AdventureTemplate's title
    int next_scene = 0;        // index of the next scene to fight
    std::vector<int> party;    // character ids
    AdventureStatus status = AdventureStatus::InProgress;
    bool long_rest_taken = false;  // at the current camp
    std::string started_at;
    int campaign_id = 0;       // the campaign active when it started (0 = none)
    // What earlier scenes changed (docs/scenes-and-npcs.md): unlocked flags,
    // and how the next fight goes.
    std::vector<std::string> flags;
    bool skip_next_fight = false;
    int fewer_monsters = 0;
    int more_monsters = 0;
    Surprise surprise = Surprise::None;
    std::vector<std::string> allies;  // NPC keys who fight beside the party next time
};

// "skip;fewer=1;surprise=party;allies=snig,dagna;" and back, for the database.
std::string format_next_fight(const AdventureRun& run);
void parse_next_fight(AdventureRun& run, const std::string& text);

// The scene to fight next; nullptr once the run is over.
const AdventureScene* next_scene(const AdventureRun& run, const AdventureTemplate& adventure);
bool is_last_scene(const AdventureRun& run, const AdventureTemplate& adventure);
// Between scenes: at least one fight won and the run still going.
bool between_scenes(const AdventureRun& run);

// After a non-fight scene: on to the next one (Finished after the last).
void record_scene(AdventureRun& run, const AdventureTemplate& adventure);

// After a fight: a victory moves on to the next scene (Finished after the
// last one); a defeat ends the run. Either way, what earlier scenes did to
// the fight (skip, fewer/more monsters, surprise, allies) is used up. Anything else (the fight left
// unfinished) changes nothing, so the scene is fought again on resume.
void record_fight(AdventureRun& run, const AdventureTemplate& adventure, bool victory);

// A long rest is possible right after a camp scene, once per camp.
bool can_long_rest(const AdventureRun& run, const AdventureTemplate& adventure);
void record_long_rest(AdventureRun& run);

// Ends the run between scenes, keeping everything earned so far.
void retreat(AdventureRun& run);

// "Scene 2 of 3" -style progress, for the picker's Resume entry.
std::string progress_text(const AdventureRun& run, const AdventureTemplate& adventure);

}  // namespace dnd
