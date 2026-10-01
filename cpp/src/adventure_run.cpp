#include "dnd/adventure_run.hpp"

namespace dnd {

const char* adventure_status_name(AdventureStatus status) {
    switch (status) {
        case AdventureStatus::InProgress: return "InProgress";
        case AdventureStatus::Finished: return "Finished";
        case AdventureStatus::Retreated: return "Retreated";
        case AdventureStatus::Defeated: return "Defeated";
    }
    return "InProgress";
}

AdventureStatus adventure_status_from_name(const std::string& name) {
    for (auto status : {AdventureStatus::Finished, AdventureStatus::Retreated, AdventureStatus::Defeated}) {
        if (name == adventure_status_name(status)) return status;
    }
    return AdventureStatus::InProgress;
}

const AdventureScene* next_scene(const AdventureRun& run, const AdventureTemplate& adventure) {
    if (run.status != AdventureStatus::InProgress) return nullptr;
    if (run.next_scene < 0 || run.next_scene >= static_cast<int>(adventure.scenes.size())) return nullptr;
    return &adventure.scenes[static_cast<size_t>(run.next_scene)];
}

bool is_last_scene(const AdventureRun& run, const AdventureTemplate& adventure) {
    return run.next_scene == static_cast<int>(adventure.scenes.size()) - 1;
}

bool between_scenes(const AdventureRun& run) {
    return run.status == AdventureStatus::InProgress && run.next_scene > 0;
}

void record_fight(AdventureRun& run, const AdventureTemplate& adventure, bool victory) {
    if (run.status != AdventureStatus::InProgress) return;
    if (!victory) {
        run.status = AdventureStatus::Defeated;
        return;
    }
    ++run.next_scene;
    run.long_rest_taken = false;  // a new break between scenes
    if (run.next_scene >= static_cast<int>(adventure.scenes.size())) run.status = AdventureStatus::Finished;
}

bool can_long_rest(const AdventureRun& run, const AdventureTemplate& adventure) {
    if (!between_scenes(run) || run.long_rest_taken) return false;
    return adventure.scenes[static_cast<size_t>(run.next_scene - 1)].camp_after;
}

void record_long_rest(AdventureRun& run) {
    run.long_rest_taken = true;
}

void retreat(AdventureRun& run) {
    if (run.status == AdventureStatus::InProgress) run.status = AdventureStatus::Retreated;
}

std::string progress_text(const AdventureRun& run, const AdventureTemplate& adventure) {
    return "scene " + std::to_string(run.next_scene + 1) + " of " + std::to_string(adventure.scenes.size());
}

}  // namespace dnd
