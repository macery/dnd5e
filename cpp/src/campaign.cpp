#include "dnd/campaign.hpp"

namespace dnd {

std::vector<Character> characters_in_play(const Repository& repo) {
    int active = repo.active_campaign_id();
    auto characters = repo.get_characters();
    if (active == 0) return characters;
    std::vector<Character> members;
    for (auto& character : characters) {
        if (character.campaign_id == active) members.push_back(std::move(character));
    }
    return members;
}

bool in_play(const Repository& repo, const Character& character) {
    int active = repo.active_campaign_id();
    return active == 0 || character.campaign_id == active;
}

std::vector<AdventureRun> runs_in_play(const Repository& repo, bool in_progress_only) {
    int active = repo.active_campaign_id();
    std::vector<AdventureRun> runs;
    for (auto& run : repo.get_adventure_runs(in_progress_only)) {
        if (run.campaign_id == active) runs.push_back(std::move(run));
    }
    return runs;
}

std::string active_campaign_name(const Repository& repo) {
    for (const auto& campaign : repo.get_campaigns()) {
        if (campaign.active) return campaign.name;
    }
    return "";
}

}  // namespace dnd
