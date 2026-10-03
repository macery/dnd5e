#pragma once

#include <string>
#include <vector>

#include "dnd/adventure_run.hpp"
#include "dnd/character.hpp"
#include "dnd/repository.hpp"

namespace dnd {

// Campaigns scope play (docs/campaigns.md): with one active, fights,
// parties, trading, the Temple, and adventure runs use only its characters.
// With none active, everything is open, as before campaigns.

// The active campaign's characters, or every character when none is active.
std::vector<Character> characters_in_play(const Repository& repo);

// Whether a character is in play: in the active campaign, or any character
// when none is active.
bool in_play(const Repository& repo, const Character& character);

// Adventure runs of the active campaign (or those with no campaign, when
// none is active), newest first.
std::vector<AdventureRun> runs_in_play(const Repository& repo, bool in_progress_only);

// "The Lost Mine" or "" when no campaign is active.
std::string active_campaign_name(const Repository& repo);

}  // namespace dnd
