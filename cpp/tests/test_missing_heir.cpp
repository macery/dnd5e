// The Missing Heir (docs/scenes-and-npcs.md, phase 4): an investigation where
// what the party learns decides the fights.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "dnd/adventure.hpp"
#include "dnd/adventure_run.hpp"
#include "dnd/repository.hpp"
#include "dnd/scenes.hpp"

namespace {

class FixedRandomSource : public dnd::IRandomSource {
public:
    int roll_die(int sides) override { return std::min(10, sides); }
};

const dnd::AdventureScene& scene(const dnd::AdventureTemplate& adventure, const std::string& title,
                                 dnd::SceneKind kind) {
    for (const auto& s : adventure.scenes) {
        if (s.title == title && s.kind == kind) return s;
    }
    FAIL("no scene " << title);
    return adventure.scenes.front();
}

std::vector<std::string> labels(const dnd::AdventureScene& s, const dnd::AdventureRun& run) {
    std::vector<std::string> shown;
    for (const auto* option : dnd::available_options(s, run)) shown.push_back(option->label);
    return shown;
}

bool offers(const std::vector<std::string>& shown, const std::string& start) {
    return std::any_of(shown.begin(), shown.end(), [&](const std::string& l) { return l.rfind(start, 0) == 0; });
}

}  // namespace

TEST_CASE("The Missing Heir: three people, each with part of the truth", "[adventure][heir]") {
    const auto* heir = dnd::find_adventure("The Missing Heir");
    REQUIRE(heir);
    REQUIRE(heir->suggested_level == 3);
    REQUIRE(heir->npcs.size() == 4);
    int social = 0;
    int traps = 0;
    int fights = 0;
    for (const auto& s : heir->scenes) {
        social += s.kind == dnd::SceneKind::Social;
        traps += s.kind == dnd::SceneKind::Trap;
        fights += s.kind == dnd::SceneKind::Fight;
    }
    REQUIRE(social == 4);  // the steward, the tavern keeper, the runner, and the heir
    REQUIRE(traps == 2);   // the trap-filled cellar
    REQUIRE(fights == 2);  // fewer fights than the other runs, and both can be avoided or changed
    REQUIRE(heir->scenes.back().boss);

    dnd::Repository repo(":memory:");
    auto monsters = repo.get_monsters();
    auto seeded = [&](const std::string& name) {
        return std::any_of(monsters.begin(), monsters.end(), [&](const dnd::Monster& m) { return m.name == name; });
    };
    for (const auto& s : heir->scenes) {
        for (const auto& group : s.monsters) REQUIRE(seeded(group.monster_name));
    }
    for (const auto& npc : heir->npcs) {
        if (!npc.stat_block.empty()) REQUIRE(seeded(npc.stat_block));
    }
}

TEST_CASE("The Missing Heir: what the party learns opens the quiet way and the accusation", "[adventure][heir]") {
    const auto* heir = dnd::find_adventure("The Missing Heir");
    dnd::AdventureRun run;
    run.title = heir->title;
    const auto& door = scene(*heir, "The Hideout Door", dnd::SceneKind::Challenge);
    const auto& before = scene(*heir, "Before Varro", dnd::SceneKind::Challenge);

    // Knowing nothing: the loud way in, and no accusation.
    REQUIRE_FALSE(offers(labels(door, run), "Drop in through the coal chute"));
    REQUIRE_FALSE(offers(labels(before, run), "Call her out"));
    REQUIRE_FALSE(offers(labels(before, run), "Leave them to it"));
    REQUIRE(offers(labels(before, run), "Say nothing, and attack"));

    run.flags = {"chute"};  // from the tavern keeper
    REQUIRE(offers(labels(door, run), "Drop in through the coal chute"));
    REQUIRE(offers(labels(before, run), "Leave them to it"));

    run.flags.push_back("truth");  // from the ledgers, the runner, or the heir
    REQUIRE(offers(labels(before, run), "Call her out"));

    // The truth comes from three places.
    int sources = 0;
    for (const auto& s : heir->scenes) {
        for (const auto& option : s.options) sources += option.success.unlock == "truth";
    }
    REQUIRE(sources == 3);
}

TEST_CASE("The Missing Heir: calling out the steward turns her against the party", "[adventure][heir]") {
    dnd::Repository repo(":memory:");
    int player = repo.add_player("Adam");
    dnd::Character aria;
    aria.character_name = "Aria";
    aria.player_id = player;
    aria.class_id = repo.get_classes().front().id;
    aria.race = repo.get_races().front().id;
    aria.alignment = repo.get_alignments().front().id;
    aria.max_hp = 20;
    aria.current_hp = 20;
    int aria_id = repo.add_character(aria);
    FixedRandomSource random;
    dnd::Dice dice(random);

    const auto* heir = dnd::find_adventure("The Missing Heir");
    dnd::AdventureRun run;
    run.title = heir->title;
    run.party = {aria_id};
    run.flags = {"truth"};
    const auto& before = scene(*heir, "Before Varro", dnd::SceneKind::Challenge);
    const dnd::SceneOption* accuse = nullptr;
    for (const auto* option : dnd::available_options(before, run)) {
        if (option->label.rfind("Call her out", 0) == 0) accuse = option;
    }
    REQUIRE(accuse);

    auto lines = dnd::apply_outcome(repo, run, accuse->success, aria_id, dice);
    REQUIRE(run.foes == std::vector<std::string>{"mirela"});
    auto foes = dnd::next_fight_foes(repo, run, repo.get_monsters());
    REQUIRE(foes.size() == 1);
    REQUIRE(foes.front().npc.name == "Mirela Voss");
    REQUIRE(foes.front().npc.attitude == dnd::Attitude::Hostile);
    REQUIRE(foes.front().stat_block.name == "Noble");
    for (const auto& c : repo.get_characters()) {
        if (c.character_id == aria_id) {
            REQUIRE(c.inspiration);
            REQUIRE(c.coins[3] == 30);  // the stolen silver: 30 gp
        }
    }
}
