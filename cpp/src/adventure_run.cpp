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

void record_scene(AdventureRun& run, const AdventureTemplate& adventure) {
    if (run.status != AdventureStatus::InProgress) return;
    ++run.next_scene;
    run.long_rest_taken = false;
    if (run.next_scene >= static_cast<int>(adventure.scenes.size())) run.status = AdventureStatus::Finished;
}

std::string format_next_fight(const AdventureRun& run) {
    std::string text;
    if (run.skip_next_fight) text += "skip;";
    if (run.fewer_monsters) text += "fewer=" + std::to_string(run.fewer_monsters) + ";";
    if (run.more_monsters) text += "more=" + std::to_string(run.more_monsters) + ";";
    if (run.surprise == Surprise::PartySurprises) text += "surprise=party;";
    if (run.surprise == Surprise::PartySurprised) text += "surprise=monsters;";
    return text;
}

void parse_next_fight(AdventureRun& run, const std::string& text) {
    run.skip_next_fight = text.find("skip;") != std::string::npos;
    auto number = [&](const std::string& key) {
        size_t at = text.find(key + "=");
        return at == std::string::npos ? 0 : std::stoi(text.substr(at + key.size() + 1));
    };
    run.fewer_monsters = number("fewer");
    run.more_monsters = number("more");
    run.surprise = text.find("surprise=party;") != std::string::npos      ? Surprise::PartySurprises
                   : text.find("surprise=monsters;") != std::string::npos ? Surprise::PartySurprised
                                                                          : Surprise::None;
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
