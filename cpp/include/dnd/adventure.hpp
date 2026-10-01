#pragma once

#include <string>
#include <vector>

namespace dnd {

// One monster group in an adventure scene. monster_name must match a seeded
// Monster.name -- resolved against the live Monster list at runtime, not
// stored by id, since this content never touches the database.
struct AdventureMonsterGroup {
    std::string monster_name;
    int count = 1;
};

// An item reward. item_name must match a seeded Item.name, resolved at
// runtime the same way as AdventureMonsterGroup.
struct AdventureLoot {
    std::string item_name;
    int quantity = 1;
};

// One fight in an adventure (docs/adventures-and-currency.md): narration,
// the monsters (scaled for the party), and what it pays.
struct AdventureScene {
    std::string title;
    std::string narration;
    std::vector<AdventureMonsterGroup> monsters;
    std::vector<AdventureLoot> loot;  // to the first character still standing
    // Coins, split among the survivors -- for a level-1 party; scale_adventure
    // multiplies it by the party's average level.
    int coins_cp = 0;
    bool camp_after = false;  // the party can take a long rest after this fight
    bool boss = false;        // scaled toward Hard rather than Medium
};

// A small, pre-built, homebrew adventure for solo play: a hook, one or more
// scenes in order, and a payoff line (victory_text) after the last one.
struct AdventureTemplate {
    std::string title;
    std::string hook;
    std::string victory_text;
    int suggested_level = 1;
    std::vector<AdventureScene> scenes;
};

const std::vector<AdventureTemplate>& adventure_templates();
// nullptr if there's no adventure with this title.
const AdventureTemplate* find_adventure(const std::string& title);

}  // namespace dnd
