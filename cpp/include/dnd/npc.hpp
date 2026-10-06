#pragma once

#include <optional>
#include <string>

namespace dnd {

class Repository;

// Non-player characters (docs/scenes-and-npcs.md): people the party talks
// to in Social scenes, and the faces behind the Shop counter and the Temple
// altar. NPCs belong to a campaign (or to none), so an innkeeper the party
// helped stays friendly in that campaign's later adventures.

// How an NPC feels about the party (DMG ch. 8, "Social Interaction").
enum class Attitude { Hostile, Indifferent, Friendly };
const char* attitude_name(Attitude attitude);
// Inverse of attitude_name; Indifferent for unknown text.
Attitude attitude_from_name(const std::string& name);
// Friendlier (+) or less friendly (-) by `steps`, stopping at Hostile and Friendly.
Attitude shift_attitude(Attitude attitude, int steps);

struct Npc {
    int npc_id = 0;       // 0 until saved
    int campaign_id = 0;  // 0 = no campaign
    // Who this is in the content: an adventure's NPC ("snig"), or the Shop's
    // "merchant" and the Temple's "priest". "" for NPCs made by hand.
    std::string key;
    std::string name;
    std::string role;  // "innkeeper", "cult priest"
    Attitude attitude = Attitude::Indifferent;
    std::string description;
    // What they want and fear. Once the party knows (an Insight check, or
    // the DM ticks it), bringing them up lowers conversation DCs.
    std::string wants;
    std::string fears;
    bool known = false;
    // A Monster name for their combat stats ("Veteran", "Mage"), or "".
    // NPCs join fights in phase 3.
    std::string stat_block;
};

// What the party asks of an NPC, from least to most (DMG ch. 8, the
// Conversation Reaction table).
enum class SocialRequest {
    NoHarm,           // just stop opposing the party
    NoRisk,           // help, at no risk to themselves
    MinorRisk,        // help, at a minor risk or sacrifice
    SignificantRisk,  // help, at a significant risk or sacrifice
};
// "help at no risk", for the Scene dialog.
const char* social_request_text(SocialRequest request);

// The Persuasion, Deception, or Intimidation DC for a request, following the
// DMG's table: 0 means no check is needed, and nothing means they won't do
// it at this attitude, whatever is said.
//   Friendly:    no risk 0, minor risk 10, significant risk 20.
//   Indifferent: no harm 0, no risk 10, minor risk 20.
//   Hostile:     no harm 10, no risk 20.
std::optional<int> social_dc(Attitude attitude, SocialRequest request);

// Knowing what an NPC wants and fears lowers a conversation's DC by this much.
constexpr int kKnownMotivesBonus = 5;

// The DC once the party's knowledge is counted (never below 0); nothing if
// the request is out of reach.
std::optional<int> conversation_dc(const Npc& npc, SocialRequest request);

// The campaign's NPC with this key -- created from `first_meeting` (its
// key, campaign, and id are set here) the first time, and as the campaign
// left them every time after.
Npc meet_npc(Repository& repo, int campaign_id, const Npc& first_meeting);

// The Shop's merchant and the Temple's priest in a campaign (0: no
// campaign), with names and descriptions the first time.
Npc shop_merchant(Repository& repo, int campaign_id);
Npc temple_priest(Repository& repo, int campaign_id);

}  // namespace dnd
