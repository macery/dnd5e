#pragma once

#include <string>

#include <vector>

#include "dnd/npc.hpp"
#include "dnd/rules.hpp"

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

// Scenes besides fights (docs/scenes-and-npcs.md).
enum class SceneKind { Fight, Challenge, Trap, Choice, Social };

// Who's surprised when the next fight starts: surprised combatants lose
// their first turn.
enum class Surprise { None, PartySurprises, PartySurprised };

// What happens after a choice or a check. Everything is optional.
struct SceneOutcome {
    std::string text;               // what happened, in a sentence or two
    int damage_dice = 0;            // e.g. 2d6 falling damage...
    int damage_sides = 6;
    std::string damage_type;
    bool damage_everyone = false;   // ...to everyone, or just whoever rolled
    int exhaustion = 0;             // levels, to everyone (a forced march)
    bool inspiration = false;       // to whoever rolled (everyone, after a group check)
    std::vector<AdventureLoot> loot;  // to whoever rolled
    int coins_cp = 0;               // split among the party
    bool skip_next_fight = false;   // sneaked past: no fight, and no rewards from it
    int fewer_monsters = 0;         // the next fight loses this many (never below one)
    int more_monsters = 0;          // ...or gains reinforcements
    Surprise surprise = Surprise::None;
    std::string unlock;             // a flag later options can require
    // The NPC's attitude moves this many steps (+ friendlier). `attitude_npc`
    // names who: an adventure NPC's key, "merchant" or "priest" (the Shop's
    // and the Temple's), or "" for the scene's own NPC.
    int attitude_change = 0;
    std::string attitude_npc;
    // An NPC (by key) who fights beside the party in the next fight, if
    // they're alive, not Hostile, and have a stat block.
    std::string ally_joins;
};

// One way through a Challenge or Choice scene.
struct SceneOption {
    std::string label;          // "Climb down the gorge"
    std::string check;          // a skill or ability ("Athletics"); "" for a choice with no roll
    int dc = 0;
    bool group = false;         // everyone rolls; it succeeds if half do
    std::string requires_flag;  // offered only once this flag is unlocked
    SceneOutcome success;       // a choice with no roll uses `success`
    SceneOutcome failure;
    // A request in a Social scene: the DC comes from the NPC's attitude and
    // what's asked (dnd/npc.hpp's social_dc), and the player picks the
    // approach -- Persuasion, Deception, or Intimidation, or those listed.
    bool social = false;
    SocialRequest request = SocialRequest::NoRisk;
    std::vector<std::string> approaches;
};

// A hidden danger: the party's best passive Perception spots it; otherwise
// it's sprung (a save for half damage).
struct SceneTrap {
    std::string name;            // "a scything blade"
    int spot_dc = 15;
    std::string disarm_check = "Sleight of Hand";
    int disarm_dc = 15;
    Ability save_ability = Ability::Dexterity;
    int save_dc = 13;
    int damage_dice = 2;
    int damage_sides = 10;
    std::string damage_type = "slashing";
    bool everyone = false;       // the whole party, or whoever walks into it
};

// One scene of an adventure (docs/adventures-and-currency.md): a fight
// (narration, the monsters scaled for the party, what it pays) -- or a
// challenge, trap, choice, or conversation (docs/scenes-and-npcs.md).
struct AdventureScene {
    std::string title;
    std::string narration;
    SceneKind kind = SceneKind::Fight;
    std::vector<SceneOption> options;  // Challenge and Choice
    SceneTrap trap;                    // Trap
    std::string npc;                   // Social: the key of who the party talks to
    int insight_dc = 12;               // Social: reading what they want and fear
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
    // The people met in Social scenes, as they are the first time the
    // campaign meets them (dnd/npc.hpp's meet_npc). Keys are unique.
    std::vector<Npc> npcs;
};

const std::vector<AdventureTemplate>& adventure_templates();
// nullptr if there's no adventure with this title.
const AdventureTemplate* find_adventure(const std::string& title);

}  // namespace dnd
