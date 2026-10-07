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
    // What earlier scenes did to this fight is used up.
    run.skip_next_fight = false;
    run.fewer_monsters = 0;
    run.more_monsters = 0;
    run.surprise = Surprise::None;
    run.allies.clear();
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
    if (!run.allies.empty()) {
        text += "allies=";
        for (size_t i = 0; i < run.allies.size(); ++i) text += (i ? "," : "") + run.allies[i];
        text += ";";
    }
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
    run.allies.clear();
    if (size_t at = text.find("allies="); at != std::string::npos) {
        std::string list = text.substr(at + 7, text.find(';', at) - at - 7);
        size_t start = 0;
        while (start <= list.size()) {
            size_t comma = list.find(',', start);
            std::string key = list.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
            if (!key.empty()) run.allies.push_back(key);
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
    }
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
