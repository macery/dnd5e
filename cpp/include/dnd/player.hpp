#pragma once

#include <string>

namespace dnd {

struct Player {
    int player_id = 0;
    std::string player_name;
};

// A table that plays together: a roster of characters from any players
// (Character::campaign_id). At most one campaign is active; see
// dnd/campaign.hpp and docs/campaigns.md.
struct Campaign {
    int campaign_id = 0;
    std::string name;
    std::string notes;
    bool active = false;
    std::string created_at;
};

}  // namespace dnd
