#include <catch2/catch_test_macros.hpp>

#include "dnd/adventure_run.hpp"
#include "dnd/character_creation.hpp"
#include "dnd/dice.hpp"
#include "dnd/progression.hpp"
#include "dnd/repository.hpp"

using dnd::AdventureStatus;

namespace {

class FixedRandomSource : public dnd::IRandomSource {
public:
    explicit FixedRandomSource(int value) : value_(value) {}
    int roll_die(int sides) override { return std::min(value_, sides); }

private:
    int value_;
};

}  // namespace

TEST_CASE("a run through a multi-scene adventure", "[adventure_run]") {
    const auto* warren = dnd::find_adventure("The Goblin Warren");
    REQUIRE(warren);
    REQUIRE(warren->scenes.size() == 3);
    REQUIRE(warren->scenes[1].camp_after);

    dnd::AdventureRun run;
    run.title = warren->title;
    REQUIRE(dnd::next_scene(run, *warren) == &warren->scenes[0]);
    REQUIRE_FALSE(dnd::between_scenes(run));
    REQUIRE_FALSE(dnd::can_long_rest(run, *warren));

    dnd::record_fight(run, *warren, true);  // the lookouts
    REQUIRE(dnd::between_scenes(run));
    REQUIRE(dnd::next_scene(run, *warren) == &warren->scenes[1]);
    REQUIRE_FALSE(dnd::can_long_rest(run, *warren));  // no camp after the first scene
    REQUIRE(dnd::progress_text(run, *warren) == "scene 2 of 3");

    dnd::record_fight(run, *warren, true);  // the wolf pens: a camp after
    REQUIRE(dnd::can_long_rest(run, *warren));
    dnd::record_long_rest(run);
    REQUIRE_FALSE(dnd::can_long_rest(run, *warren));  // once per camp
    REQUIRE(dnd::is_last_scene(run, *warren));

    dnd::record_fight(run, *warren, true);  // the boss
    REQUIRE(run.status == AdventureStatus::Finished);
    REQUIRE(dnd::next_scene(run, *warren) == nullptr);
    REQUIRE_FALSE(dnd::between_scenes(run));
}

TEST_CASE("defeat and retreat end a run", "[adventure_run]") {
    const auto* warren = dnd::find_adventure("The Goblin Warren");
    dnd::AdventureRun run;
    run.title = warren->title;

    SECTION("defeat") {
        dnd::record_fight(run, *warren, false);
        REQUIRE(run.status == AdventureStatus::Defeated);
        dnd::record_fight(run, *warren, true);  // nothing after it's over
        REQUIRE(run.next_scene == 0);
    }
    SECTION("retreat between scenes keeps the place") {
        dnd::record_fight(run, *warren, true);
        dnd::retreat(run);
        REQUIRE(run.status == AdventureStatus::Retreated);
        REQUIRE(run.next_scene == 1);
    }
    REQUIRE(dnd::adventure_status_from_name(dnd::adventure_status_name(run.status)) == run.status);
}

TEST_CASE("adventure runs are saved and resumed", "[adventure_run][repository]") {
    dnd::Repository repo(":memory:");
    dnd::AdventureRun run;
    run.title = "The Goblin Warren";
    run.party = {3, 5, 9};
    run.started_at = "2026-10-01T20:00:00Z";
    run.run_id = repo.add_adventure_run(run);
    REQUIRE(run.run_id > 0);

    run.next_scene = 2;
    run.long_rest_taken = true;
    repo.update_adventure_run(run);
    auto runs = repo.get_adventure_runs(true);
    REQUIRE(runs.size() == 1);
    REQUIRE(runs[0].title == "The Goblin Warren");
    REQUIRE(runs[0].next_scene == 2);
    REQUIRE(runs[0].party == std::vector<int>{3, 5, 9});
    REQUIRE(runs[0].long_rest_taken);

    run.status = AdventureStatus::Finished;
    repo.update_adventure_run(run);
    REQUIRE(repo.get_adventure_runs(true).empty());
    REQUIRE(repo.get_adventure_runs().size() == 1);
}

TEST_CASE("a short rest with a choice of hit dice", "[progression][adventure_run]") {
    const dnd::ClassInfo* fighter = dnd::find_class_info("Fighter");
    FixedRandomSource sixes(6);
    dnd::Dice dice(sixes);
    dnd::Character aria;
    aria.level = 3;
    aria.constitution = 14;  // +2
    aria.max_hp = 30;
    aria.current_hp = 10;
    aria.feature_uses_spent = {{"SecondWind", 1}};

    auto result = dnd::short_rest(aria, *fighter, dice, 2);  // 2 x (6 + 2)
    REQUIRE(result.hit_dice_spent == 2);
    REQUIRE(result.hp_regained == 16);
    REQUIRE(aria.current_hp == 26);
    REQUIRE(result.features_restored);
    REQUIRE(aria.feature_uses_spent.empty());

    // Stops once HP is full, and never spends dice that aren't there.
    auto again = dnd::short_rest(aria, *fighter, dice, 5);
    REQUIRE(again.hit_dice_spent == 1);  // 26 -> 30
    REQUIRE(aria.current_hp == 30);
    REQUIRE(dnd::hit_dice_remaining(aria) == 0);

    aria.dead = true;
    aria.current_hp = 0;
    REQUIRE(dnd::short_rest(aria, *fighter, dice, 1).hit_dice_spent == 0);
}
