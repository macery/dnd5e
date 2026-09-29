#pragma once

#include <string>
#include <vector>

namespace dnd {

// One monster group in an AdventureTemplate. monster_name must match a
// seeded Monster.name -- resolved against the live Monster list at runtime,
// not stored by id, since this content never touches the database.
struct AdventureMonsterGroup {
    std::string monster_name;
    int count = 1;
};

// An item reward in an AdventureTemplate. item_name must match a seeded
// Item.name, resolved at runtime the same way as AdventureMonsterGroup.
struct AdventureLoot {
    std::string item_name;
    int quantity = 1;
};

// A small, pre-built, homebrew mini-encounter: a reason to fight (hook), the
// monsters involved, and a payoff line (victory_text) -- built for solo play
// so there's something to click into and play through, rather than having
// to hand-assemble every encounter.
struct AdventureTemplate {
    std::string title;
    std::string hook;
    std::string victory_text;
    std::vector<AdventureMonsterGroup> monster_groups;
    std::vector<AdventureLoot> loot;  // handed out on victory
};

const std::vector<AdventureTemplate>& adventure_templates();

}  // namespace dnd
