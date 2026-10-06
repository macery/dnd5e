// Solo adventures: the picker, choosing the party, and the between-scenes panel.

#include <catch2/catch_test_macros.hpp>

#include <QListWidget>

#include "AdventureDialogs.hpp"
#include "AdventurePickerDialog.hpp"
#include "dnd/progression.hpp"
#include "ui_helpers.hpp"

using namespace ui_test;

TEST_CASE("adventure picker: runs to resume come first", "[ui][adventure]") {
    World world;
    auto aria = world.add("Aria", "Fighter", 3);
    dnd::AdventureRun run;
    run.title = "Crypt of the Restless";
    run.next_scene = 1;
    run.party = {aria.character_id};
    run.started_at = "2026-10-01T18:00:00Z";
    run.run_id = world.repo.add_adventure_run(run);

    AdventurePickerDialog picker({3}, world.repo.get_monsters(), world.repo.get_adventure_runs(true));
    picker.show();
    settle();
    auto* list = picker.findChild<QListWidget*>();
    REQUIRE(list->item(0)->text() == "▶ Resume: Crypt of the Restless — scene 2 of 6");
    REQUIRE(picker.isResume());
    REQUIRE(picker.selectedRun().run_id == run.run_id);

    for (int i = 0; i < list->count(); ++i) {
        if (list->item(i)->text().startsWith("The Goblin Warren")) list->setCurrentRow(i);
    }
    settle();
    REQUIRE_FALSE(picker.isResume());
    REQUIRE(picker.selectedAdventure().title == "The Goblin Warren");
    QString details = visibleText(picker);
    REQUIRE(details.contains("The Wolf Pens"));
    REQUIRE(details.contains("Then a camp"));
    REQUIRE(details.contains("(final fight)"));
}

TEST_CASE("party dialog: the current player's characters are picked by default", "[ui][adventure]") {
    World world;
    auto aria = world.add("Aria", "Fighter", 3);
    int beth = world.repo.add_player("Beth");
    dnd::Character other;
    other.character_name = "Bram";
    other.player_id = beth;
    other.class_id = world.classId("Cleric");
    other.race = 1;
    other.alignment = 1;
    world.repo.add_character(other);

    PartyDialog party(world.repo.get_characters(), world.repo.get_players(), world.repo.get_classes(), world.player,
                      "The Goblin Warren");
    party.show();
    settle();
    REQUIRE(party.partyIds() == std::vector<int>{aria.character_id});
    auto* list = party.findChild<QListWidget*>();
    for (int i = 0; i < list->count(); ++i) list->item(i)->setCheckState(Qt::Unchecked);
    settle();
    REQUIRE_FALSE(button(party, "Set Out")->isEnabled());  // someone has to go
}

TEST_CASE("between scenes: a long rest only at a camp, once", "[ui][adventure]") {
    World world;
    auto aria = world.add("Aria", "Fighter", 3);
    aria.current_hp = 4;
    aria.feature_uses_spent = {{"SecondWind", 1}};
    world.repo.update_character(aria);
    const auto* warren = dnd::find_adventure("The Goblin Warren");
    dnd::AdventureRun run;
    run.title = warren->title;
    run.party = {aria.character_id};
    run.run_id = world.repo.add_adventure_run(run);
    auto classes = world.repo.get_classes();

    SECTION("no camp after the first fight") {
        dnd::record_fight(run, *warren, true);
        BetweenScenesDialog between(world.repo, run, *warren, classes);
        between.show();
        settle();
        REQUIRE_FALSE(button(between, "Long Rest")->isEnabled());
        REQUIRE(visibleText(between).contains("1 of 6 scenes done"));
        button(between, "Press On")->click();
        REQUIRE(between.choice() == BetweenScenesDialog::Choice::PressOn);
    }

    SECTION("the camp after the wolf pens") {
        dnd::record_scene(run, *warren);       // the approach
        dnd::record_fight(run, *warren, true);  // the lookouts
        dnd::record_fight(run, *warren, true);  // the wolf pens
        BetweenScenesDialog between(world.repo, run, *warren, classes);
        between.show();
        settle();
        REQUIRE(visibleText(between).contains("The Snare Corridor"));
        auto* longRest = button(between, "Long Rest");
        REQUIRE(longRest->isEnabled());
        longRest->click();
        settle();
        auto rested = world.get(aria.character_id);
        REQUIRE(rested.current_hp == rested.max_hp);
        REQUIRE(rested.feature_uses_spent.empty());
        REQUIRE_FALSE(longRest->isEnabled());  // once per camp
        REQUIRE(world.repo.get_adventure_runs(true).front().long_rest_taken);  // saved
    }

    SECTION("retreat") {
        dnd::record_fight(run, *warren, true);
        BetweenScenesDialog between(world.repo, run, *warren, classes);
        between.show();
        button(between, "Retreat")->click();
        REQUIRE(between.choice() == BetweenScenesDialog::Choice::Retreat);
    }
}
